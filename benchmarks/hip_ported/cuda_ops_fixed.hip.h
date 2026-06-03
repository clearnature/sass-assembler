// phase3/ggml/hip/hip_ops.h — 全部 CUDA 内核声明
#ifndef PHASE3_CUDA_OPS_H
#define PHASE3_CUDA_OPS_H
#include <hip/hip_runtime.h>

#include <stdint.h>
#include <stdbool.h>
void launch_rmsnorm(float* x, const float* w, float* y, int N, int D, float eps, hipStream_t s);
void launch_rmsnorm_add(const float* x, const float* w, const float* res, float* y, int N, int D, float eps, hipStream_t s);
void launch_layernorm(const float* x, const float* w, const float* b, float* y, int N, int D, float eps, hipStream_t s);
void launch_rope(float* x, const float* c, const float* si, int T, int D, hipStream_t s);
void launch_rope_fwd(const float* x, float* y, const float* cos, const float* sin, int n, int T, int D, hipStream_t s);
void launch_rope_batch(float* q, float* k, const float* cos, const float* sin, int N, int T, int D, hipStream_t s);
void launch_rope_transpose(const float* x, const float* c, const float* si, float* out, int B, int NH, int T, int D, hipStream_t s);
void launch_gate_blend(const float* x_pe, const float* x_rope, float logit, const float* pos_bias, float gate_min, float* y, int B, int T, int D, hipStream_t s);
void launch_ws_gemm_gelu(const float* x, const float* w, const float* bias, const float* mask, float* y, int N, int M, int K, hipStream_t s);
void launch_ws_gemm(const float* x, const float* w, const float* bias, const float* mask, float* y, int N, int M, int K, hipStream_t s);
void launch_sparse_gemm(const float* x, const float* w, const uint8_t* mask, float* y, int N, int M, int K, hipStream_t s);
void launch_sparse_gemm_gelu(const float* x, const float* w, const uint8_t* mask, float* y, int N, int M, int K, hipStream_t s);
void launch_attn(const float* q, const float* k, const float* v, float* o, int B, int NH, int T, int D, bool causal, hipStream_t s);
void launch_gated_attn(const float* qr, const float* kr, const float* vr, const float* qp, const float* kp, const float* vp, const float* gate_base, float* o, int B, int NH, int T, int D, hipStream_t s);
void launch_embedding(const int* tokens, const float* table, float* y, int N, int T, int V, int D, hipStream_t s);
void launch_add_inplace(float* x, const float* res, int n, hipStream_t s);
void launch_cross_entropy(const float* logits, const int* targets, float* loss, int N, int C, hipStream_t s);

#endif
