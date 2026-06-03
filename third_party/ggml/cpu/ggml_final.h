// phase3/ggml/cpu/ggml_final.h — 最后四块拼图: LayerNorm + Embed + Add + CrossEntropy
// 编译: gcc -O3 -mavx2 -mfma -fPIC -shared ggml_final.c -o libggml_final.so -lm

#ifndef PHASE3_GGML_FINAL_H
#define PHASE3_GGML_FINAL_H

#include <stdint.h>
#include <stddef.h>

// ─────────────────────────────────────────────────────────────
// LayerNorm: y = (x - mean) / sqrt(var + eps) * weight + bias
//   x      [N, D] float32
//   weight [D]    float32  γ (scale)
//   bias   [D]    float32  β (shift, 可为 NULL)
//   y      [N, D] float32
// ─────────────────────────────────────────────────────────────
void fused_layernorm(const float* x, const float* weight, const float* bias,
                     float* y, int N, int D, float eps);

// ─────────────────────────────────────────────────────────────
// Embedding Lookup: y[t][d] = table[tokens[t]][d]
//   tokens [N, T] int32     token IDs
//   table  [V, D] float32   词表
//   y      [N, T, D] float32  输出
//   N, T, V, D
// ─────────────────────────────────────────────────────────────
void fused_embedding(const int32_t* tokens, const float* table,
                     float* y, int N, int T, int V, int D);

// ─────────────────────────────────────────────────────────────
// 原地残差: x[t][d] += residual[t][d]
// ─────────────────────────────────────────────────────────────
void fused_add_inplace(float* x, const float* residual, int N, int D);

// ─────────────────────────────────────────────────────────────
// CrossEntropy Loss: 对每行 logits 算 softmax + NLL
//   logits [N, C] float32    模型输出 (N 个样本, C 个类别)
//   targets[N]    int32      正确 token ID
//   loss   [1]    float*     输出 loss
//   N, C
// ─────────────────────────────────────────────────────────────
void fused_cross_entropy(const float* logits, const int32_t* targets,
                         float* loss, int N, int C);

#endif // PHASE3_GGML_FINAL_H
