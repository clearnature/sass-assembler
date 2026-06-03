// phase3/ggml/cpu/ggml_fused_norm.c — RMSNorm AVX2 实现
// 每行: mean = sum(x^2)/D, rstd = 1/sqrt(mean+eps), y = x * rstd * weight
// AVX2 + FMA, Broadwell E5 v4 优化
// 编译: gcc -O3 -mavx2 -mfma -fPIC -shared ggml_fused_norm.c -o libggml_fused_norm.so -lm
#include "ggml_fused_norm.h"
#include <math.h>
#include <stdint.h>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

// ── 水平求和: __m256 → float ──
#if defined(__AVX2__) && defined(__FMA__)
static inline float hsum_avx(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    lo = _mm_add_ps(lo, hi);
    lo = _mm_hadd_ps(lo, lo);
    lo = _mm_hadd_ps(lo, lo);
    return _mm_cvtss_f32(lo);
}
#endif

// ═══════════════════════════════════════════════════════════════
// RMSNorm 核心实现
// ═══════════════════════════════════════════════════════════════
// 两趟式:
//   趟 1: 逐行计算 sum(x^2) → mean → rstd
//   趟 2: y = x * rstd * weight  (内存连续, 逐块写入)
// ═══════════════════════════════════════════════════════════════

void fused_rmsnorm(const float* x, const float* weight, float* y,
                   int N, int D, float eps) {
#if defined(__AVX2__) && defined(__FMA__)
    int d = 0;

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        const float* x_row = x + i * D;
        float* y_row = y + i * D;

        // ── 趟 1: 平方累加 ──
        __m256 sum_sq = _mm256_setzero_ps();
        for (d = 0; d <= D - 8; d += 8) {
            __m256 xv = _mm256_loadu_ps(x_row + d);
            sum_sq = _mm256_fmadd_ps(xv, xv, sum_sq);  // sum_sq += x^2
        }
        float mean_sq = hsum_avx(sum_sq);
        for (; d < D; d++)
            mean_sq += x_row[d] * x_row[d];
        mean_sq = mean_sq / (float)D + eps;

        // ── rsqrt: 1/sqrt(mean_sq), 牛顿-拉夫逊迭代 ──
        // x0 = rsqrt(mean_sq), x1 = x0 * (1.5 - 0.5 * mean_sq * x0^2)
        float rstd = 1.0f / sqrtf(mean_sq);

        // ── 趟 2: y = x * rstd * weight ──
        __m256 rstd_v = _mm256_set1_ps(rstd);
        for (d = 0; d <= D - 8; d += 8) {
            __m256 xv = _mm256_loadu_ps(x_row + d);
            __m256 wv = _mm256_loadu_ps(weight + d);
            // y = x * rstd * weight = x * (rstd * weight)
            __m256 yv = _mm256_mul_ps(xv, _mm256_mul_ps(rstd_v, wv));
            _mm256_storeu_ps(y_row + d, yv);
        }
        for (; d < D; d++)
            y_row[d] = x_row[d] * rstd * weight[d];
    }

#else
    // ── 纯 C 回退 ──
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        const float* x_row = x + i * D;
        float* y_row = y + i * D;

        float sum_sq = 0.0f;
        for (int d = 0; d < D; d++)
            sum_sq += x_row[d] * x_row[d];
        float rstd = 1.0f / sqrtf(sum_sq / (float)D + eps);

        for (int d = 0; d < D; d++)
            y_row[d] = x_row[d] * rstd * weight[d];
    }
#endif
}

void fused_rmsnorm_add(const float* x, const float* weight,
                       const float* residual, float* y,
                       int N, int D, float eps) {
#if defined(__AVX2__) && defined(__FMA__)
    int d = 0;

    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        const float* x_row = x + i * D;
        float* y_row = y + i * D;
        const float* res_row = residual + i * D;

        __m256 sum_sq = _mm256_setzero_ps();
        for (d = 0; d <= D - 8; d += 8) {
            __m256 xv = _mm256_loadu_ps(x_row + d);
            sum_sq = _mm256_fmadd_ps(xv, xv, sum_sq);
        }
        float mean_sq = hsum_avx(sum_sq);
        for (; d < D; d++)
            mean_sq += x_row[d] * x_row[d];
        mean_sq = mean_sq / (float)D + eps;

        float rstd = 1.0f / sqrtf(mean_sq);
        __m256 rstd_v = _mm256_set1_ps(rstd);

        for (d = 0; d <= D - 8; d += 8) {
            __m256 xv = _mm256_loadu_ps(x_row + d);
            __m256 wv = _mm256_loadu_ps(weight + d);
            __m256 resv = _mm256_loadu_ps(res_row + d);
            __m256 yv = _mm256_fmadd_ps(xv, _mm256_mul_ps(rstd_v, wv), resv);  // y = x*rstd*weight + res
            _mm256_storeu_ps(y_row + d, yv);
        }
        for (; d < D; d++)
            y_row[d] = x_row[d] * rstd * weight[d] + res_row[d];
    }

#else
    #pragma omp parallel for
    for (int i = 0; i < N; i++) {
        const float* x_row = x + i * D;
        float* y_row = y + i * D;
        const float* res_row = residual + i * D;

        float sum_sq = 0.0f;
        for (int d = 0; d < D; d++)
            sum_sq += x_row[d] * x_row[d];
        float rstd = 1.0f / sqrtf(sum_sq / (float)D + eps);

        for (int d = 0; d < D; d++)
            y_row[d] = x_row[d] * rstd * weight[d] + res_row[d];
    }
#endif
}

// ═══════════════════════════════════════════════════════════════
#include <string.h>

// RMSNorm Backward: dx, dw (AVX2 + FMA)
//   dx = rstd * w * (grad_y - xn * mean(grad_y * w * xn))
//   dw = sum(grad_y * xn)  where xn = x * rstd
// ═══════════════════════════════════════════════════════════════
void fused_rmsnorm_bwd(const float* grad_y, const float* x,
                       const float* weight, const float* rstd,
                       float* dx, float* dw,
                       int N, int D) {
#if defined(__AVX2__) && defined(__FMA__)
    float* dw_local = (float*)calloc(D, sizeof(float));
    if (!dw_local) return;

    #pragma omp parallel
    {
        float* dw_thread = (float*)calloc(D, sizeof(float));

        #pragma omp for
        for (int i = 0; i < N; i++) {
            const float* gy_row = grad_y + i * D;
            const float* x_row = x + i * D;
            float* dx_row = dx + i * D;
            float ri = rstd[i];

            // Compute mean(xn * grad_y * weight)
            float mean_xn_gy_w = 0.0f;
            __m256 sum_v = _mm256_setzero_ps();
            int d;
            for (d = 0; d <= D - 8; d += 8) {
                __m256 xv = _mm256_loadu_ps(x_row + d);
                __m256 wv = _mm256_loadu_ps(weight + d);
                __m256 gyv = _mm256_loadu_ps(gy_row + d);
                __m256 xnv = _mm256_mul_ps(xv, _mm256_set1_ps(ri));
                sum_v = _mm256_fmadd_ps(xnv, _mm256_mul_ps(gyv, wv), sum_v);
            }
            mean_xn_gy_w = hsum_avx(sum_v);
            for (; d < D; d++) {
                float xn = x_row[d] * ri;
                mean_xn_gy_w += xn * gy_row[d] * weight[d];
            }
            mean_xn_gy_w /= (float)D;

            // dx = rstd * (w * gy - xn * mean)
            // dw_thread += gy * xn
            __m256 mean_v = _mm256_set1_ps(mean_xn_gy_w);
            __m256 ri_v = _mm256_set1_ps(ri);

            for (d = 0; d <= D - 8; d += 8) {
                __m256 gyv = _mm256_loadu_ps(gy_row + d);
                __m256 wv = _mm256_loadu_ps(weight + d);
                __m256 xv = _mm256_loadu_ps(x_row + d);
                __m256 xnv = _mm256_mul_ps(xv, ri_v);
                __m256 gy_w = _mm256_mul_ps(gyv, wv);

                __m256 dxv = _mm256_mul_ps(ri_v,
                    _mm256_sub_ps(gy_w, _mm256_mul_ps(xnv, mean_v)));
                _mm256_storeu_ps(dx_row + d, dxv);

                __m256 dw_cur = _mm256_loadu_ps(dw_thread + d);
                _mm256_storeu_ps(dw_thread + d,
                    _mm256_fmadd_ps(gyv, xnv, dw_cur));
            }
            for (; d < D; d++) {
                float xn = x_row[d] * ri;
                dx_row[d] = ri * (gy_row[d] * weight[d] - xn * mean_xn_gy_w);
                dw_thread[d] += gy_row[d] * xn;
            }
        }

        #pragma omp critical
        { for (int d = 0; d < D; d++) dw_local[d] += dw_thread[d]; }
        free(dw_thread);
    }
    memcpy(dw, dw_local, D * sizeof(float));
    free(dw_local);
#else
    memset(dw, 0, D * sizeof(float));
    for (int i = 0; i < N; i++) {
        const float* gy_row = grad_y + i * D;
        const float* x_row = x + i * D;
        float* dx_row = dx + i * D;
        float ri = rstd[i];

        float mean_xn_gy_w = 0.0f;
        for (int d = 0; d < D; d++) {
            float xn = x_row[d] * ri;
            mean_xn_gy_w += xn * gy_row[d] * weight[d];
        }
        mean_xn_gy_w /= (float)D;

        for (int d = 0; d < D; d++) {
            float xn = x_row[d] * ri;
            dx_row[d] = ri * (gy_row[d] * weight[d] - xn * mean_xn_gy_w);
            dw[d] += gy_row[d] * xn;
        }
    }
#endif
}
