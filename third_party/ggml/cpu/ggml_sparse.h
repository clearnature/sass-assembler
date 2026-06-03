// phase3/ggml/cpu/ggml_sparse.h — 32×32 块稀疏 GEMM (跳过零块)
// 架构: V4TopologyScheduler Block-wise 剪枝
// 编译: gcc -O3 -mavx2 -fPIC -shared ggml_sparse.c -o libggml_sparse.so

#ifndef PHASE3_GGML_SPARSE_H
#define PHASE3_GGML_SPARSE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SPARSE_BLOCK 32  // 块大小 32×32

// ─────────────────────────────────────────────────────────────
// 32×32 块稀疏 GEMM: y = x @ W^T (跳过 mask=0 的块)
//
//   x      [N, K]   float32  输入
//   w      [M, K]   float32  权重
//   mask   [M/32, K/32] uint8  0/1 块掩码 (1=计算该块)
//   y      [N, M]   float32  输出 (零初始化后累加)
//   N, M, K                    矩阵维度 (K % 32 == 0, M % 32 == 0)
//
//   性能: 稀疏度 50% 时约 2x 加速 (对比稠密)
// ─────────────────────────────────────────────────────────────
void sparse_gemm(const float* x, const float* w, const uint8_t* mask,
                 float* y, int N, int M, int K);

// ─────────────────────────────────────────────────────────────
// 稀疏 GEMM + GELU 融合: y = GELU(x @ W^T)
// ─────────────────────────────────────────────────────────────
void sparse_gemm_gelu(const float* x, const float* w, const uint8_t* mask,
                      float* y, int N, int M, int K);

// ─────────────────────────────────────────────────────────────
// 稀疏 GEMM + GELU + 残差: y = GELU(x @ W^T) + residual
// ─────────────────────────────────────────────────────────────
void sparse_gemm_gelu_add(const float* x, const float* w, const uint8_t* mask,
                          const float* residual, float* y,
                          int N, int M, int K);

// ─────────────────────────────────────────────────────────────
// 权重重排: 行优先 → 块连续 [nb_row, nb_col, 32, 32]
// 每个 32×32 块连续 4KB, L1 缓存友好
// ─────────────────────────────────────────────────────────────
void repack_weights(const float* w, float* w_repacked, int M, int K);

// ─────────────────────────────────────────────────────────────
// 生成块掩码: 从 float mask 矩阵转为 uint8 块掩码
//   mask_float [M, K] float32    0.0/1.0 元素级掩码
//   mask_block [M/32, K/32] uint8  块级掩码 (块内全部非零才为 1)
// ─────────────────────────────────────────────────────────────
void make_block_mask(const float* mask_float, uint8_t* mask_block, int M, int K);

#endif // PHASE3_GGML_SPARSE_H

// ─────────────────────────────────────────────────────────────
// 稀疏 GEMM Backward
//   dW = X^T @ dY (仅 mask=1 的块)  [M, K]
//   dX = dY @ W^T (仅 mask=1 的块)  [N, K]
//   x      [N, K]  float32
//   dy     [N, M]  float32  上游梯度
//   w      [M, K]  float32  权重
//   mask   [M/32, K/32] uint8  块掩码
// ─────────────────────────────────────────────────────────────
void sparse_gemm_bwd_dw(const float* x, const float* dy,
                        const uint8_t* mask, float* dw,
                        int N, int M, int K);

void sparse_gemm_bwd_dx(const float* dy, const float* w,
                        const uint8_t* mask, float* dx,
                        int N, int M, int K);
