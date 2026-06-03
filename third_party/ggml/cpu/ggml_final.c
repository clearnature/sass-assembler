// phase3/ggml/cpu/ggml_final.c — 最后四块拼图实现
// 编译: gcc -O3 -mavx2 -mfma -fPIC -shared ggml_final.c -o libggml_final.so -lm
#include "ggml_final.h"
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <immintrin.h>

#if defined(__AVX2__)
static inline float hsum_avx(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    lo = _mm_hadd_ps(lo, lo);
    return _mm_cvtss_f32(lo);
}
#endif

// ═══════════════════════════════════════════════════════════════
// 1. LayerNorm
// ═══════════════════════════════════════════════════════════════
// mean = sum(x)/D, var = sum((x-mean)^2)/D, y = (x-mean)/sqrt(var+eps)*w + b

void fused_layernorm(const float* x, const float* weight, const float* bias,
                     float* y, int N, int D, float eps) {
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        const float* x_row = x + i * D;
        float* y_row = y + i * D;

#if defined(__AVX2__) && defined(__FMA__)
        // ── 趟 1: 计算 mean ──
        __m256 sum_v = _mm256_setzero_ps();
        int d;
        for (d = 0; d <= D - 8; d += 8)
            sum_v = _mm256_add_ps(sum_v, _mm256_loadu_ps(x_row + d));
        float mean = hsum_avx(sum_v);
        for (; d < D; d++) mean += x_row[d];
        mean /= (float)D;

        // ── 趟 2: 计算 var ──
        __m256 mean_v = _mm256_set1_ps(mean);
        __m256 var_v = _mm256_setzero_ps();
        for (d = 0; d <= D - 8; d += 8) {
            __m256 diff = _mm256_sub_ps(_mm256_loadu_ps(x_row + d), mean_v);
            var_v = _mm256_fmadd_ps(diff, diff, var_v);
        }
        float var = hsum_avx(var_v);
        for (; d < D; d++) { float diff = x_row[d] - mean; var += diff * diff; }
        var = var / (float)D;

        // ── 趟 3: 归一化 ──
        float inv_std = 1.0f / sqrtf(var + eps);
        __m256 is_v = _mm256_set1_ps(inv_std);
        for (d = 0; d <= D - 8; d += 8) {
            __m256 xv = _mm256_loadu_ps(x_row + d);
            __m256 nv = _mm256_mul_ps(_mm256_sub_ps(xv, mean_v), is_v);
            if (weight) nv = _mm256_mul_ps(nv, _mm256_loadu_ps(weight + d));
            if (bias)   nv = _mm256_add_ps(nv, _mm256_loadu_ps(bias + d));
            _mm256_storeu_ps(y_row + d, nv);
        }
        for (; d < D; d++) {
            float val = (x_row[d] - mean) * inv_std;
            if (weight) val *= weight[d];
            if (bias)   val += bias[d];
            y_row[d] = val;
        }
#else
        float mean = 0, var = 0;
        for (int d = 0; d < D; d++) mean += x_row[d];
        mean /= D;
        for (int d = 0; d < D; d++) { float diff = x_row[d] - mean; var += diff * diff; }
        var /= D;
        float is = 1.0f / sqrtf(var + eps);
        for (int d = 0; d < D; d++) {
            float val = (x_row[d] - mean) * is;
            if (weight) val *= weight[d];
            if (bias)   val += bias[d];
            y_row[d] = val;
        }
#endif
    }
}

// ═══════════════════════════════════════════════════════════════
// 2. Embedding Lookup (纯内存拷贝, AVX2 加速)
// ═══════════════════════════════════════════════════════════════

void fused_embedding(const int32_t* tokens, const float* table,
                     float* y, int N, int T, int V, int D) {
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < T; j++) {
            int idx = tokens[i * T + j];
            if (idx < 0) idx = 0;
            if (idx >= V) idx = V - 1;
            const float* src = table + idx * D;
            float* dst = y + (i * T + j) * D;
#if defined(__AVX2__)
            int d;
            for (d = 0; d <= D - 8; d += 8)
                _mm256_storeu_ps(dst + d, _mm256_loadu_ps(src + d));
            for (; d < D; d++) dst[d] = src[d];
#else
            for (int d = 0; d < D; d++) dst[d] = src[d];
#endif
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// 3. 原地残差 Add
// ═══════════════════════════════════════════════════════════════

void fused_add_inplace(float* x, const float* residual, int N, int D) {
    size_t total = (size_t)N * D;
#if defined(__AVX2__)
    size_t s;
    for (s = 0; s <= total - 8; s += 8) {
        __m256 xv = _mm256_loadu_ps(x + s);
        __m256 rv = _mm256_loadu_ps(residual + s);
        _mm256_storeu_ps(x + s, _mm256_add_ps(xv, rv));
    }
    for (; s < total; s++) x[s] += residual[s];
#else
    for (size_t s = 0; s < total; s++) x[s] += residual[s];
#endif
}

// ═══════════════════════════════════════════════════════════════
// 4. CrossEntropy Loss (数值稳定 softmax + NLL)
// ═══════════════════════════════════════════════════════════════

void fused_cross_entropy(const float* logits, const int32_t* targets,
                         float* loss, int N, int C) {
    double total_loss = 0.0;
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        const float* row = logits + i * C;
        int target = targets[i];
        if (target < 0 || target >= C) target = 0;

        // Max 规约 (数值稳定)
        float max_val = -INFINITY;
        for (int j = 0; j < C; j++) max_val = fmaxf(max_val, row[j]);

        // exp 求和
        double sum_exp = 0.0;
        for (int j = 0; j < C; j++) sum_exp += expf(row[j] - max_val);

        // NLL: -log(exp(target) / sum_exp) = -(target - max_val - log(sum_exp))
        total_loss += -(row[target] - max_val - (float)log(sum_exp));
    }
    *loss = (float)(total_loss / (double)N);
}
