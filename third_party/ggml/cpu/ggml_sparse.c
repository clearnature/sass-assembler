// phase3/ggml/cpu/ggml_sparse.c — 32×32 块稀疏 GEMM (8×8 AVX2 微内核)
// v3: 列优先权重重排 + 8 YMM 累加器 + OpenMP
// 编译: gcc -O3 -mavx2 -mfma -fopenmp -fPIC -shared ggml_sparse.c -o libggml_sparse.so -lm
#include "ggml_sparse.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <omp.h>

#if defined(__AVX2__) && defined(__FMA__)
#include <immintrin.h>
#endif

#define INV_SQRT_2 0.7071067811865475f

// ═══════════════════════════════════════════════════════════════
// 权重重排: 行优先 → 块内列优先 (8×8 微内核所需布局)
// 原: w_rep[block_id][r][k] = w[br*32+r][bc*32+k]  行优先
// 新: w_rep[block_id][k][r] = w[br*32+r][bc*32+k]  列优先
//
// 效果: 对固定 k, 8 个输出行 w[r+0..7][k] 连续存储
//       → 一次 _mm256_loadu_ps 加载 8 行权重
// ═══════════════════════════════════════════════════════════════
void repack_weights(const float* w, float* w_rep, int M, int K) {
    int nb_row = M / 32, nb_col = K / 32;
    for (int br = 0; br < nb_row; br++)
        for (int bc = 0; bc < nb_col; bc++)
            // 列优先: 外层循环 k, 内层 r
            for (int k = 0; k < 32; k++)
                for (int r = 0; r < 32; r++)
                    w_rep[((br * nb_col + bc) * 1024) + k * 32 + r] =
                        w[(br * 32 + r) * K + bc * 32 + k];
}

// ═══════════════════════════════════════════════════════════════
// 8×8 AVX2 微内核: 处理 32×32 块的全部 32 行 × N 批
//
// 寄存器预算:
//   4 YMM 累加器 (4×8=32 输出行)
//   4 YMM 权重加载 (每 K 循环 4 次 loadu)
//   1 YMM x 广播
//   9/16 — 舒适区 (probe 验证)
//
// wb 列优先: wb[k*32 + r] = w[br*32+r][bc*32+k]
// 对固定 k: loadu(wb + k*32 + 0) = 8 行权重在 k 位置
// ═══════════════════════════════════════════════════════════════
#if defined(__AVX2__) && defined(__FMA__)
static inline void micro_8x8(const float* x, const float* wb,
                               float* y, int row_off, int bc, int K,
                               int N, int M, int max_row) {
    for (int n = 0; n < N; n++) {
        const float* x_nk = x + n * K + bc * 32;

        __m256 acc0 = _mm256_setzero_ps();
        __m256 acc1 = _mm256_setzero_ps();
        __m256 acc2 = _mm256_setzero_ps();
        __m256 acc3 = _mm256_setzero_ps();

        for (int k = 0; k < 32; k++) {
            __m256 xk = _mm256_set1_ps(x_nk[k]);
            // 4 个 loadu 覆盖 32 个输出行
            __m256 w0 = _mm256_loadu_ps(wb + k * 32 + 0);
            __m256 w1 = _mm256_loadu_ps(wb + k * 32 + 8);
            __m256 w2 = _mm256_loadu_ps(wb + k * 32 + 16);
            __m256 w3 = _mm256_loadu_ps(wb + k * 32 + 24);

            acc0 = _mm256_fmadd_ps(xk, w0, acc0);
            acc1 = _mm256_fmadd_ps(xk, w1, acc1);
            acc2 = _mm256_fmadd_ps(xk, w2, acc2);
            acc3 = _mm256_fmadd_ps(xk, w3, acc3);
        }

        // 带边界检查的写回
        // 累加: y += acc (非覆盖, 支持多 bc 块)
        if (max_row > row_off + 0) _mm256_storeu_ps(y + n*M + row_off + 0,
            _mm256_add_ps(_mm256_loadu_ps(y + n*M + row_off + 0), acc0));
        if (max_row > row_off + 8) _mm256_storeu_ps(y + n*M + row_off + 8,
            _mm256_add_ps(_mm256_loadu_ps(y + n*M + row_off + 8), acc1));
        if (max_row > row_off + 16) _mm256_storeu_ps(y + n*M + row_off + 16,
            _mm256_add_ps(_mm256_loadu_ps(y + n*M + row_off + 16), acc2));
        if (max_row > row_off + 24) _mm256_storeu_ps(y + n*M + row_off + 24,
            _mm256_add_ps(_mm256_loadu_ps(y + n*M + row_off + 24), acc3));
    }
}
#else
// 纯 C 回退 (列优先布局)
static inline void micro_8x8(const float* x, const float* wb,
                               float* y, int row_off, int bc, int K,
                               int N, int M, int max_row) {
    for (int n = 0; n < N; n++) {
        const float* x_nk = x + n * K + bc * 32;
        float acc[32] = {0};
        for (int k = 0; k < 32; k++)
            for (int r = 0; r < 32; r++)
                acc[r] += x_nk[k] * wb[k * 32 + r];
        for (int r = 0; r < 32 && (row_off + r) < max_row; r++)
            y[n * M + (row_off + r)] += acc[r];
    }
}
#endif

// ═══════════════════════════════════════════════════════════════
// 公开 API
// ═══════════════════════════════════════════════════════════════

void sparse_gemm(const float* x, const float* w, const uint8_t* mask,
                 float* y, int N, int M, int K) {
    int nb_row = M / 32, nb_col = K / 32;
    memset(y, 0, (size_t)N * M * sizeof(float));

    float* w_rep = (float*)malloc(M * K * sizeof(float));
    if (!w_rep) return;
    repack_weights(w, w_rep, M, K);

    #pragma omp parallel for schedule(static)
    for (int br = 0; br < nb_row; br++) {
        int block_end = (br * 32 + 32 < M) ? br * 32 + 32 : M;
        for (int bc = 0; bc < nb_col; bc++) {
            if (mask && mask[br * nb_col + bc] == 0) continue;
            const float* wb = w_rep + (br * nb_col + bc) * 1024;
            micro_8x8(x, wb, y, br * 32, bc, K, N, M, block_end);
        }
    }
    free(w_rep);
}

void sparse_gemm_gelu(const float* x, const float* w, const uint8_t* mask,
                      float* y, int N, int M, int K) {
    sparse_gemm(x, w, mask, y, N, M, K);
    size_t total = (size_t)N * M;
    for (size_t i = 0; i < total; i++)
        y[i] *= 0.5f * (1.0f + erff(y[i] * INV_SQRT_2));
}

void sparse_gemm_gelu_add(const float* x, const float* w, const uint8_t* mask,
                          const float* residual, float* y,
                          int N, int M, int K) {
    float* tmp = (float*)malloc((size_t)N * M * sizeof(float));
    if (!tmp) return;
    sparse_gemm(x, w, mask, tmp, N, M, K);
    size_t total = (size_t)N * M;
    for (size_t i = 0; i < total; i++)
        y[i] = tmp[i] * 0.5f * (1.0f + erff(tmp[i] * INV_SQRT_2)) + residual[i];
    free(tmp);
}

void make_block_mask(const float* mask_float, uint8_t* mask_block, int M, int K) {
    int nb_row = M / 32, nb_col = K / 32;
    for (int br = 0; br < nb_row; br++)
        for (int bc = 0; bc < nb_col; bc++) {
            uint8_t active = 0;
            for (int i = 0; i < 32 && !active; i++)
                for (int j = 0; j < 32 && !active; j++)
                    if (mask_float[(br*32+i)*K + bc*32 + j] != 0) active = 1;
            mask_block[br * nb_col + bc] = active;
        }
}

// ═══════════════════════════════════════════════════════════════
// 稀疏 GEMM Backward
// ═══════════════════════════════════════════════════════════════

// dW = X^T @ dY — 只计算 mask=1 的块
// 对每个激活的输出块 (br, bc):
//   dW[br*32:br*32+32][bc*32:bc*32+32] += X[token_block]^T @ dY[token_block][br_block]
void sparse_gemm_bwd_dw(const float* x, const float* dy,
                        const uint8_t* mask, float* dw,
                        int N, int M, int K) {
    int nb_row = M / 32, nb_col = K / 32;
    int nb_token = (N + 31) / 32;
    memset(dw, 0, M * K * sizeof(float));

    #pragma omp parallel for collapse(2)
    for (int br = 0; br < nb_row; br++) {
        for (int bc = 0; bc < nb_col; bc++) {
            // Skip inactive blocks
            if (mask && !mask[br * nb_col + bc]) continue;

            // Accumulate over token blocks
            for (int tb = 0; tb < nb_token; tb++) {
                int t_start = tb * 32;
                int tokens = (t_start + 32 < N) ? 32 : N - t_start;

                // dW[32][32] += X[tokens][32]^T @ dY[tokens][32]
                for (int tt = 0; tt < tokens; tt++) {
                    const float* x_row = x + (t_start + tt) * K + bc * 32;
                    const float* dy_row = dy + (t_start + tt) * M + br * 32;
                    float* dw_block = dw + br * 32 * K + bc * 32;

                    for (int k = 0; k < 32; k++) {
                        float x_val = x_row[k];
                        for (int m = 0; m < 32; m++)
                            dw_block[m * K + k] += dy_row[m] * x_val;
                    }
                }
            }
        }
    }
}

// dX = dY @ W^T — 只计算 mask=1 的块
void sparse_gemm_bwd_dx(const float* dy, const float* w,
                        const uint8_t* mask, float* dx,
                        int N, int M, int K) {
    int nb_row = M / 32, nb_col = K / 32;
    memset(dx, 0, N * K * sizeof(float));

    #pragma omp parallel for
    for (int n = 0; n < N; n++) {
        const float* dy_row = dy + n * M;
        float* dx_row = dx + n * K;

        for (int br = 0; br < nb_row; br++) {
            for (int bc = 0; bc < nb_col; bc++) {
                if (mask && !mask[br * nb_col + bc]) continue;

                const float* w_block = w + br * 32 * K + bc * 32;
                const float* dy_block = dy_row + br * 32;
                float* dx_block = dx_row + bc * 32;

                // dx[n][bc*32:bc*32+32] += dy[n][br*32:br*32+32] @ w[br*32:br*32+32][bc*32:bc*32+32]
                for (int m = 0; m < 32; m++) {
                    float dy_val = dy_block[m];
                    for (int k = 0; k < 32; k++)
                        dx_block[k] += dy_val * w_block[m * K + k];
                }
            }
        }
    }
}
