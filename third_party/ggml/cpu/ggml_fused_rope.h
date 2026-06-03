// phase3/ggml/cpu/ggml_fused_rope.h — RoPE 融合核 (AVX2)
// 旋转位置编码: y[d] = x[d]*cos(t,d) - x[d+D/2]*sin(t,d)
//               y[d+D/2] = x[d+D/2]*cos(t,d) + x[d]*sin(t,d)
// 编译: gcc -O3 -mavx2 -mfma -fPIC -shared ggml_fused_rope.c -o libggml_fused_rope.so -lm

#ifndef PHASE3_GGML_FUSED_ROPE_H
#define PHASE3_GGML_FUSED_ROPE_H

#include <stdint.h>

// ─────────────────────────────────────────────────────────────
// 预计算 RoPE cos/sin 表
//   inv_freq [D/2]  float32  频率基: 1/base^(2d/D) for d in [0, D/2)
//   cos      [T, D] float32  输出 cos 表
//   sin      [T, D] float32  输出 sin 表
//   T, D, base
// ─────────────────────────────────────────────────────────────
void rope_precompute(float* cos, float* sin,
                     const float* inv_freq, int T, int D, float base);

// ─────────────────────────────────────────────────────────────
// 生成本地 inv_freq 数组
//   inv_freq [D/2]  float32  输出: 1/base^(2d/D)
//   D, base
// ─────────────────────────────────────────────────────────────
void rope_make_inv_freq(float* inv_freq, int D, float base);

// ─────────────────────────────────────────────────────────────
// 应用 RoPE (原地)
//   x    [T, D] float32  输入/输出 Q 或 K 矩阵
//   cos  [T, D] float32  预计算 cos 表
//   sin  [T, D] float32  预计算 sin 表
//   T, D
//
// 对每个 (t, d) 对:
//   x[t][d]     = x[t][d]*cos[t][d] - x[t][d+D/2]*sin[t][d]
//   x[t][d+D/2] = x[t][d+D/2]*cos[t][d] + x[t][d]*sin[t][d]
// ─────────────────────────────────────────────────────────────
void rope_apply(float* x, const float* cos, const float* sin, int T, int D);

// ─────────────────────────────────────────────────────────────
// RoPE + 多头转置 (QKV 投影后直接原地 RoPE)
//   x    [B, T, D]       float32  输入 Q/K (head_last 布局)
//   cos  [T, D]          float32  cos 表
//   sin  [T, D]          float32  sin 表
//   out  [B, NH, T, DH]  float32  输出 (head_first 布局, 供 attention)
//   B, NH, T, D (=NH*DH)
// ─────────────────────────────────────────────────────────────
void rope_apply_transpose(const float* x, const float* cos, const float* sin,
                          float* out, int B, int NH, int T, int D);

#endif // PHASE3_GGML_FUSED_ROPE_H
