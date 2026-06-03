// phase3/ggml/cpu/ggml_fused_attn.h — 融合 FlashAttention 算子 (CPU, AVX2)
// 在线 softmax tiling, 避免物化 T×T 注意力矩阵
// 架构: GatedHybridAttention (RoPE + Learned PE + Gate)
// 编译: gcc -O3 -mavx2 -fPIC -shared ggml_fused_attn.c -o libggml_fused_attn.so

#ifndef PHASE3_GGML_FUSED_ATTN_H
#define PHASE3_GGML_FUSED_ATTN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ─────────────────────────────────────────────────────────────
// 融合多头注意力: Q @ K^T → softmax → @ V  (在线 tiling, 无 T×T 物化)
//
// 等价于:
//   scores[b][h][i][j] = (Q[b][h][i] @ K[b][h][j]^T) / sqrt(head_dim)
//   attn = softmax(scores, dim=-1)
//   O[b][h][i] = attn[i][:] @ V[b][h][:]
//
//   内部: 分块迭代, 每块做局部 softmax + rescale 合并
//
//   q      [B, NH, T, D] float32    查询
//   k      [B, NH, T, D] float32    键
//   v      [B, NH, T, D] float32    值
//   o      [B, NH, T, D] float32    输出
//   causal bool                    是否因果掩码 (GPT: 上三角 -inf)
//   B, NH, T, D                     参数
//
//   性能: T=4096, NH=12, D=64 时, 峰值 ~2-3x 朴素实现
// ─────────────────────────────────────────────────────────────
void fused_attn(const float* q, const float* k, const float* v, float* o,
                int B, int NH, int T, int D, bool causal);

// ─────────────────────────────────────────────────────────────
// 融合多头注意力 + N14 门控 (GatedHybridAttention)
//
// 等价于:
//   O_rope = fused_attn(Q_rope, K_rope, V_rope, T, D)
//   O_pe   = fused_attn(Q_pe, K_pe, V_pe, T, D)
//   gate   = sigmoid(logit_gate + pos_bias)  // [T, 1] 逐位置门控
//   O = gate * O_pe + (1 - gate) * O_rope
//
// 相比分开算两次 fused_attn + gate, 此函数在 tiling 循环内融合 gate,
// 减少一次完整 T×D 的读写.
//
//   q_rope, k_rope, v_rope  [B, NH, T, D]  RoPE 分支 QKV
//   q_pe,   k_pe,   v_pe    [B, NH, T, D]  PE 分支 QKV
//   gate_base [T] float32   logit_gate + pos_bias (sigmoid 前)
//   o      [B, NH, T, D]    输出: gate softmax(Q_pe @ K_pe) V_pe + (1-gate) softmax(Q_rope @ K_rope) V_rope
//   B, NH, T, D
//   causal
// ─────────────────────────────────────────────────────────────
void fused_gated_attn(const float* q_rope, const float* k_rope, const float* v_rope,
                      const float* q_pe,   const float* k_pe,   const float* v_pe,
                      const float* gate_base, float* o,
                      int B, int NH, int T, int D, bool causal);

#endif // PHASE3_GGML_FUSED_ATTN_H
