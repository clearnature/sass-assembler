// phase3/ggml/cpu/ggml_fused.c — 融合算子实现 (AVX2)
// 编译: gcc -O3 -mavx2 -fPIC -shared ggml_fused.c -o libggml_fused.so
#include "ggml_fused.h"
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <immintrin.h>

#define WS_EPS 1e-6f
#define INV_SQRT_2 0.7071067811865475f  // 1/sqrt(2)
#define SQRT_2_PI 0.7978845608028654f    // sqrt(2/pi)

// ═══════════════════════════════════════════════════════════════
// AVX2 工具宏
// ═══════════════════════════════════════════════════════════════
#if defined(__AVX2__)
#define AVX_FLOAT_STEP 8

// ── 水平求和: __m256 → float ──
static inline float hsum_avx(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    lo = _mm_add_ps(lo, hi);
    lo = _mm_hadd_ps(lo, lo);
    lo = _mm_hadd_ps(lo, lo);
    return _mm_cvtss_f32(lo);
}

// ── GELU 近似 (AVX2: 8 路标量 erff, 确保精度) ──
// GELU(x) = x * 0.5 * (1 + erf(x / sqrt(2)))
// erff() 来自 math.h, glibc 有向量化版本, 精度 = 1 ULP
static inline __m256 gelu_avx(__m256 x) {
    float buf[8];
    _mm256_storeu_ps(buf, x);
    float inv_sqrt2 = 0.7071067811865475f;
    for (int i = 0; i < 8; i++)
        buf[i] = buf[i] * 0.5f * (1.0f + erff(buf[i] * inv_sqrt2));
    return _mm256_loadu_ps(buf);
}

// ═══════════════════════════════════════════════════════════════
// 权重标准化: 单行, AVX2 加速
// ═══════════════════════════════════════════════════════════════
static void ws_row(const float* w_row, float* w_std, int K) {
    // 计算均值 mean = sum(w) / K
    __m256 sum_v = _mm256_setzero_ps();
    int k;
    for (k = 0; k <= K - 8; k += 8)
        sum_v = _mm256_add_ps(sum_v, _mm256_loadu_ps(w_row + k));
    float mean = hsum_avx(sum_v);
    // 处理剩余元素
    for (; k < K; k++) mean += w_row[k];
    mean /= (float)K;

    // 计算标准差 std = sqrt(sum((w - mean)^2) / K)
    __m256 mean_v = _mm256_set1_ps(mean);
    __m256 sq_sum_v = _mm256_setzero_ps();
    for (k = 0; k <= K - 8; k += 8) {
        __m256 diff = _mm256_sub_ps(_mm256_loadu_ps(w_row + k), mean_v);
        sq_sum_v = _mm256_add_ps(sq_sum_v, _mm256_mul_ps(diff, diff));
    }
    float sq_sum = hsum_avx(sq_sum_v);
    for (; k < K; k++) {
        float d = w_row[k] - mean;
        sq_sum += d * d;
    }
    float std = sqrtf(sq_sum / (float)(K - 1)) + WS_EPS;  // Bessel, 匹配 PyTorch std
    __m256 std_v = _mm256_set1_ps(std);

    // 标准化: w_std = (w - mean) / std
    for (k = 0; k <= K - 8; k += 8) {
        __m256 diff = _mm256_sub_ps(_mm256_loadu_ps(w_row + k), mean_v);
        _mm256_storeu_ps(w_std + k, _mm256_div_ps(diff, std_v));
    }
    for (; k < K; k++)
        w_std[k] = (w_row[k] - mean) / std;
}

// ═══════════════════════════════════════════════════════════════
// 融合核心: WS + mask + GEMM
//   gemm: y_out[i][j] = sum_k x[i][k] * w_std[j][k]
//   但 w_std 是实时计算 + mask 后的结果
// ═══════════════════════════════════════════════════════════════
static void ws_mask_gemm(const float* x, const float* w, const float* mask,
                         float* y_out, int N, int M, int K) {
    // 为 w_std 分配临时空间 (单行缓存)
    float* w_std_row = (float*)malloc(K * sizeof(float));
    if (!w_std_row) return;

    // 对每个输出行 (M 个输出神经元) 依次处理
    for (int j = 0; j < M; j++) {
        // Step 1: 权重标准化 (单行)
        ws_row(w + j * K, w_std_row, K);

        // Step 2: 应用 mask (如果非 NULL)
        if (mask) {
            int k2;
            for (k2 = 0; k2 <= K - 8; k2 += 8) {
                __m256 mv = _mm256_loadu_ps(mask + j * K + k2);
                __m256 wv = _mm256_loadu_ps(w_std_row + k2);
                _mm256_storeu_ps(w_std_row + k2, _mm256_mul_ps(wv, mv));
            }
            for (; k2 < K; k2++)
                w_std_row[k2] *= mask[j * K + k2];
        }

        // Step 3: GEMM — x @ w_std^T, 即 y[i] += x[i][k] * w_std[k]
        for (int i = 0; i < N; i++) {
            __m256 acc = _mm256_setzero_ps();
            int k3;
            for (k3 = 0; k3 <= K - 8; k3 += 8) {
                __m256 xv = _mm256_loadu_ps(x + i * K + k3);
                __m256 wv = _mm256_loadu_ps(w_std_row + k3);
                acc = _mm256_add_ps(acc, _mm256_mul_ps(xv, wv));
            }
            float sum = hsum_avx(acc);
            for (; k3 < K; k3++)
                sum += x[i * K + k3] * w_std_row[k3];
            y_out[i * M + j] = sum;
        }
    }
    free(w_std_row);
}

// ═══════════════════════════════════════════════════════════════
// 公开 API
// ═══════════════════════════════════════════════════════════════

void fused_linear_ws_gelu(const float* x, const float* w, const float* bias,
                          const float* mask, float* y,
                          int N, int M, int K) {
    // 临时输出 (N, M)
    float* tmp = (float*)malloc(N * M * sizeof(float));
    if (!tmp) return;

    // WS + mask + GEMM → tmp
    ws_mask_gemm(x, w, mask, tmp, N, M, K);

    // 加 bias + GELU
    for (int i = 0; i < N; i++) {
        for (int j = 0; j <= M - 8; j += 8) {
            __m256 val = _mm256_loadu_ps(tmp + i * M + j);
            if (bias)
                val = _mm256_add_ps(val, _mm256_loadu_ps(bias + j));
            __m256 act = gelu_avx(val);
            _mm256_storeu_ps(y + i * M + j, act);
        }
        for (int j = j; j < M; j++) {
            float val = tmp[i * M + j] + (bias ? bias[j] : 0.0f);
            y[i * M + j] = val * 0.5f * (1.0f + erff(val * INV_SQRT_2));
        }
    }
    free(tmp);
}

void fused_linear_ws(const float* x, const float* w, const float* bias,
                     const float* mask, float* y,
                     int N, int M, int K) {
    // WS + mask + GEMM → tmp
    float* tmp = (float*)malloc(N * M * sizeof(float));
    if (!tmp) return;

    ws_mask_gemm(x, w, mask, tmp, N, M, K);

    // 加 bias
    for (int i = 0; i < N; i++) {
        for (int j = 0; j <= M - 8; j += 8) {
            __m256 val = _mm256_loadu_ps(tmp + i * M + j);
            if (bias)
                val = _mm256_add_ps(val, _mm256_loadu_ps(bias + j));
            _mm256_storeu_ps(y + i * M + j, val);
        }
        for (int j = j; j < M; j++)
            y[i * M + j] = tmp[i * M + j] + (bias ? bias[j] : 0.0f);
    }
    free(tmp);
}

void fused_linear_ws_gelu_add(const float* x1, const float* w, const float* bias,
                               const float* mask, const float* x2, float* y,
                               int N, int M, int K) {
    float* tmp = (float*)malloc(N * M * sizeof(float));
    if (!tmp) return;

    ws_mask_gemm(x1, w, mask, tmp, N, M, K);

    // 加 bias + GELU + 残差叠加
    for (int i = 0; i < N; i++) {
        for (int j = 0; j <= M - 8; j += 8) {
            __m256 val = _mm256_loadu_ps(tmp + i * M + j);
            if (bias)
                val = _mm256_add_ps(val, _mm256_loadu_ps(bias + j));
            __m256 act = gelu_avx(val);
            __m256 res = _mm256_loadu_ps(x2 + i * M + j);
            _mm256_storeu_ps(y + i * M + j, _mm256_add_ps(act, res));
        }
        for (int j = j; j < M; j++) {
            float val = tmp[i * M + j] + (bias ? bias[j] : 0.0f);
            y[i * M + j] = val * 0.5f * (1.0f + erff(val * INV_SQRT_2)) + x2[i * M + j];
        }
    }
    free(tmp);
}
#else
// ── 纯 C 回退 (无 AVX2) ──
static float hsum_fallback(const float* v, int n) {
    float s = 0; for (int i = 0; i < n; i++) s += v[i]; return s;
}

static void ws_row_fallback(const float* w_row, float* w_std, int K) {
    float mean = hsum_fallback(w_row, K) / (float)K;
    float sq = 0;
    for (int i = 0; i < K; i++) { float d = w_row[i] - mean; sq += d*d; }
    float std = sqrtf(sq / (float)(K - 1)) + WS_EPS;  // Bessel, 匹配 PyTorch
    for (int i = 0; i < K; i++) w_std[i] = (w_row[i] - mean) / std;
}

static float gelu_fallback(float x) {
    return x * 0.5f * (1.0f + erff(x * INV_SQRT_2));
}

void fused_linear_ws_gelu(const float* x, const float* w, const float* bias,
                          const float* mask, float* y, int N, int M, int K) {
    float* w_std = (float*)malloc(K * sizeof(float));
    for (int j = 0; j < M; j++) {
        ws_row_fallback(w + j*K, w_std, K);
        if (mask)
            for (int k = 0; k < K; k++) w_std[k] *= mask[j*K + k];
        for (int i = 0; i < N; i++) {
            float sum = 0;
            for (int k = 0; k < K; k++) sum += x[i*K + k] * w_std[k];
            float val = sum + (bias ? bias[j] : 0.0f);
            y[i*M + j] = gelu_fallback(val);
        }
    }
    free(w_std);
}

void fused_linear_ws(const float* x, const float* w, const float* bias,
                     const float* mask, float* y, int N, int M, int K) {
    float* w_std = (float*)malloc(K * sizeof(float));
    for (int j = 0; j < M; j++) {
        ws_row_fallback(w + j*K, w_std, K);
        if (mask)
            for (int k = 0; k < K; k++) w_std[k] *= mask[j*K + k];
        for (int i = 0; i < N; i++) {
            float sum = 0;
            for (int k = 0; k < K; k++) sum += x[i*K + k] * w_std[k];
            y[i*M + j] = sum + (bias ? bias[j] : 0.0f);
        }
    }
    free(w_std);
}

void fused_linear_ws_gelu_add(const float* x1, const float* w, const float* bias,
                               const float* mask, const float* x2, float* y,
                               int N, int M, int K) {
    float* w_std = (float*)malloc(K * sizeof(float));
    for (int j = 0; j < M; j++) {
        ws_row_fallback(w + j*K, w_std, K);
        if (mask)
            for (int k = 0; k < K; k++) w_std[k] *= mask[j*K + k];
        for (int i = 0; i < N; i++) {
            float sum = 0;
            for (int k = 0; k < K; k++) sum += x1[i*K + k] * w_std[k];
            float val = sum + (bias ? bias[j] : 0.0f);
            y[i*M + j] = gelu_fallback(val) + x2[i*M + j];
        }
    }
    free(w_std);
}
#endif
