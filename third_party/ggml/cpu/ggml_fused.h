// phase3/ggml/cpu/ggml_fused.h — 融合算子: Weight Standardization + Mask + GEMM + GELU
// 架构: Sovereign V4 MaskedLinearWS (Block-wise 32x32 Sparse)
// 编译: gcc -O3 -mavx2 -fPIC -shared ggml_fused.c -o libggml_fused.so

#ifndef PHASE3_GGML_FUSED_H
#define PHASE3_GGML_FUSED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────
// 融合算子: WS + Mask + GEMM + GELU
// 对应: mask → weight_std → x @ W_std^T + bias → GELU
//
//   x      [N, K] float32    输入激活
//   w      [M, K] float32    权重
//   bias   [M]    float32    偏置 (可为 NULL)
//   mask   [M, K] float32    0/1 掩码 (可为 NULL, 非 NULL 时应用)
//   y      [N, M] float32    输出: y = GELU(x @ W_std^T + bias)
//
//   内部: W_std = (w - mean(w,1)) / (std(w,1) + eps), 再 apply mask
// ─────────────────────────────────────────────────────────────
void fused_linear_ws_gelu(const float* x, const float* w, const float* bias,
                          const float* mask, float* y,
                          int N, int M, int K);

// ─────────────────────────────────────────────────────────────
// 融合算子: WS + Mask + GEMM (无激活, 用于 fc2)
//   y = x @ W_std^T + bias
// ─────────────────────────────────────────────────────────────
void fused_linear_ws(const float* x, const float* w, const float* bias,
                     const float* mask, float* y,
                     int N, int M, int K);

// ─────────────────────────────────────────────────────────────
// 融合算子: WS + Mask + GEMM + GELU + 残差叠加
//   y = GELU(x1 @ W_std^T + bias) + x2
//   用于完整的 fc1 → GELU → residual 链路
// ─────────────────────────────────────────────────────────────
void fused_linear_ws_gelu_add(const float* x1, const float* w, const float* bias,
                               const float* mask, const float* x2, float* y,
                               int N, int M, int K);

#endif // PHASE3_GGML_FUSED_H
