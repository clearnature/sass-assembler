// phase3/ggml/cpu/ggml_fused_norm.h — RMSNorm AVX2
// 融合: x → x.pow(2).mean(-1).rsqrt() → x * rstd * weight
// 编译: gcc -O3 -mavx2 -mfma -fPIC -shared ggml_fused_norm.c -o libggml_fused_norm.so

#ifndef PHASE3_GGML_FUSED_NORM_H
#define PHASE3_GGML_FUSED_NORM_H

#include <stdint.h>

// ─────────────────────────────────────────────────────────────
// RMSNorm: y = x * rsqrt(mean(x^2) + eps) * weight
//
//   x      [N, D]  float32  输入
//   weight [D]     float32  可学习缩放 (broadcast)
//   y      [N, D]  float32  输出
//   N, D             batch 和维度
//   eps              epsilon (默认 1e-6)
// ─────────────────────────────────────────────────────────────
void fused_rmsnorm(const float* x, const float* weight, float* y,
                   int N, int D, float eps);

// ─────────────────────────────────────────────────────────────
// RMSNorm 残差融合: y = x * rsqrt(mean(x^2) + eps) * weight + residual
//
//   x      [N, D]  输入
//   weight [D]     缩放
//   residual [N,D] 残差
//   y      [N, D]  输出
// ─────────────────────────────────────────────────────────────
void fused_rmsnorm_add(const float* x, const float* weight,
                       const float* residual, float* y,
                       int N, int D, float eps);

// RMSNorm Backward
//   grad_y [N,D], x [N,D], weight [D], rstd [N]
// → dx [N,D], dw [D]
void fused_rmsnorm_bwd(const float* grad_y, const float* x,
                       const float* weight, const float* rstd,
                       float* dx, float* dw,
                       int N, int D);

#endif // PHASE3_GGML_FUSED_NORM_H
