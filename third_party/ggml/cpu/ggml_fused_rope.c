// phase3/ggml/cpu/ggml_fused_rope.c — RoPE AVX2 融合核
// 编译: gcc -O3 -mavx2 -mfma -fPIC -shared ggml_fused_rope.c -o libggml_fused_rope.so -lm
#include "ggml_fused_rope.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#if defined(__AVX2__) && defined(__FMA__)
#include <immintrin.h>
#endif

// ═══════════════════════════════════════════════════════════════
// RoPE: 旋转位置编码
// ═══════════════════════════════════════════════════════════════
// 核心变换 (对每个位置 t, 每对维度 d 和 d+D/2):
//   x[t][d]     = x[t][d] * cos[t][d] - x[t][d+D/2] * sin[t][d]
//   x[t][d+D/2] = x[t][d+D/2] * cos[t][d] + x[t][d] * sin[t][d]
//
// 每个 FMA 周期处理 2 对 (4 个 float: d0,d1 和 d0+HD, d1+HD):
//   第一对: out0 = x0*cos - x1*sin
//   第二对: out1 = x1*cos + x0*sin

void rope_make_inv_freq(float* inv_freq, int D, float base) {
    int half = D / 2;
    for (int i = 0; i < half; i++)
        inv_freq[i] = 1.0f / powf(base, 2.0f * i / (float)D);
}

void rope_precompute(float* cos, float* sin,
                     const float* inv_freq, int T, int D, float base) {
    int half = D / 2;
    for (int t = 0; t < T; t++) {
        float* c_row = cos + t * D;
        float* s_row = sin + t * D;
        // 每对 dim 计算 cos 和 sin
        for (int d = 0; d < half; d++) {
            float freq = t * inv_freq[d];
            float c = cosf(freq);
            float s = sinf(freq);
            c_row[d] = c;
            s_row[d] = s;
            // repeat_interleave 2: d 和 d+half 共享相同 cos/sin
            c_row[d + half] = c;
            s_row[d + half] = s;
        }
    }
}

void rope_apply(float* x, const float* cos, const float* sin, int T, int D) {
    int half = D / 2;
#if defined(__AVX2__) && defined(__FMA__)
    for (int t = 0; t < T; t++) {
        float* x_row = x + t * D;
        const float* c_row = cos + t * D;
        const float* s_row = sin + t * D;

        // 每轮处理 4 对 (8 个 float: 4 from first half, 4 from second half)
        int d;
        for (d = 0; d <= half - 8; d += 8) {
            __m256 x0 = _mm256_loadu_ps(x_row + d);               // x[d..d+7]
            __m256 x1 = _mm256_loadu_ps(x_row + half + d);        // x[half+d..half+d+7]
            __m256 c  = _mm256_loadu_ps(c_row + d);               // cos[d..d+7]
            __m256 s  = _mm256_loadu_ps(s_row + d);               // sin[d..d+7]

            // out0 = x0*cos - x1*sin
            __m256 out0 = _mm256_sub_ps(_mm256_mul_ps(x0, c), _mm256_mul_ps(x1, s));
            // out1 = x1*cos + x0*sin
            __m256 out1 = _mm256_add_ps(_mm256_mul_ps(x1, c), _mm256_mul_ps(x0, s));

            _mm256_storeu_ps(x_row + d, out0);
            _mm256_storeu_ps(x_row + half + d, out1);
        }
        for (; d < half; d++) {
            float c = c_row[d], s = s_row[d];
            float x0 = x_row[d], x1 = x_row[half + d];
            x_row[d]        = x0 * c - x1 * s;
            x_row[half + d] = x1 * c + x0 * s;
        }
    }
#else
    for (int t = 0; t < T; t++) {
        float* x_row = x + t * D;
        const float* c_row = cos + t * D;
        const float* s_row = sin + t * D;
        for (int d = 0; d < half; d++) {
            float c = c_row[d], s = s_row[d];
            float x0 = x_row[d], x1 = x_row[half + d];
            x_row[d]        = x0 * c - x1 * s;
            x_row[half + d] = x1 * c + x0 * s;
        }
    }
#endif
}

void rope_apply_transpose(const float* x, const float* cos, const float* sin,
                          float* out, int B, int NH, int T, int D) {
    int half = D / 2;
    int DH = D / NH;  // head_dim

    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            const float* x_bt = x + (b * T + t) * D;
            const float* c_t = cos + t * D;
            const float* s_t = sin + t * D;

            for (int h = 0; h < NH; h++) {
                float* out_h = out + (b * NH + h) * T * D + t * DH;
#if defined(__AVX2__) && defined(__FMA__)
                int d;
                for (d = 0; d <= DH - 8; d += 8) {
                    // d 在 head_dim 范围内, 全局 dim = h*DH + d
                    int gd = h * DH + d;
                    __m256 x0 = _mm256_loadu_ps(x_bt + gd);
                    __m256 x1 = _mm256_loadu_ps(x_bt + half + gd);
                    __m256 c  = _mm256_loadu_ps(c_t + gd);
                    __m256 s  = _mm256_loadu_ps(s_t + gd);
                    __m256 out0 = _mm256_sub_ps(_mm256_mul_ps(x0, c), _mm256_mul_ps(x1, s));
                    __m256 out1 = _mm256_add_ps(_mm256_mul_ps(x1, c), _mm256_mul_ps(x0, s));
                    _mm256_storeu_ps(out_h + d, out0);
                    _mm256_storeu_ps(out_h + d + DH, out1);
                }
#else
                for (int d = 0; d < DH; d++) {
                    int gd = h * DH + d;
                    float c = c_t[gd], s = s_t[gd];
                    float x0 = x_bt[gd], x1 = x_bt[half + gd];
                    out_h[d] = x0 * c - x1 * s;
                    out_h[d + DH] = x1 * c + x0 * s;
                }
#endif
            }
        }
    }
}
