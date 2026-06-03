// phase3/ggml/cuda/cuda_ops.cu — Sovereign V4 全部 CUDA 内核
// 编译: nvcc -O3 -arch=sm_61 -ccbin gcc-13 -Xcompiler "-fPIC" -shared cuda_ops.cu -o libsovereign_cuda.so
#include "cuda_ops_fixed.hip.h"
#include <hip/hip_runtime.h>
#include <hip/hip_cooperative_groups.h>
#include <math.h>


namespace cg = cooperative_groups;
// ═══════════════════════════════════════════════════════════════
// 工具: 归约 (shared memory 树状和)
// ═══════════════════════════════════════════════════════════════
__device__ float block_sum(float val, float* shared) {
    int tid = threadIdx.x;
    shared[tid] = val;
    __syncthreads();
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) shared[tid] += shared[tid + s];
        __syncthreads();
    }
    return shared[0];
}

// ═══════════════════════════════════════════════════════════════
// 1. RMSNorm
// ═══════════════════════════════════════════════════════════════
__global__ void rmsnorm_kernel(const float* __restrict__ x,
    const float* __restrict__ weight, float* __restrict__ y, int D, float eps) {
    extern __shared__ float s_tmp[];
    int row = blockIdx.x;
    const float* xr = x + row * D;
    float* yr = y + row * D;
    int tid = threadIdx.x;

    float ssq = 0;
    for (int i = tid; i < D; i += blockDim.x) ssq += xr[i] * xr[i];
    float sum = block_sum(ssq, s_tmp);
    float rstd = rsqrtf(sum / D + eps);

    for (int i = tid; i < D; i += blockDim.x) yr[i] = xr[i] * rstd * weight[i];
}
void launch_rmsnorm(float* x, const float* w, float* y, int N, int D, float eps, hipStream_t s) {
    rmsnorm_kernel<<<N, min(D,256), min(D,256)*sizeof(float), s>>>(x, w, y, D, eps);
}

__global__ void rmsnorm_add_kernel(const float* __restrict__ x,
    const float* __restrict__ w, const float* __restrict__ res,
    float* __restrict__ y, int D, float eps) {
    extern __shared__ float s_tmp[];
    int row = blockIdx.x;
    const float* xr = x + row * D; const float* rr = res + row * D;
    float* yr = y + row * D; int tid = threadIdx.x;
    float ssq = 0;
    for (int i = tid; i < D; i += blockDim.x) ssq += xr[i] * xr[i];
    float rstd = rsqrtf(block_sum(ssq, s_tmp) / D + eps);
    for (int i = tid; i < D; i += blockDim.x) yr[i] = xr[i] * rstd * w[i] + rr[i];
}
void launch_rmsnorm_add(const float* x, const float* w, const float* res,
    float* y, int N, int D, float eps, hipStream_t s) {
    rmsnorm_add_kernel<<<N, min(D,256), min(D,256)*sizeof(float), s>>>(x, w, res, y, D, eps);
}

// ═══════════════════════════════════════════════════════════════
// 2. LayerNorm
// ═══════════════════════════════════════════════════════════════
__global__ void layernorm_kernel(const float* __restrict__ x,
    const float* __restrict__ w, const float* __restrict__ b,
    float* __restrict__ y, int D, float eps) {
    extern __shared__ float s_tmp[];
    int row = blockIdx.x; int tid = threadIdx.x;
    const float* xr = x + row * D; float* yr = y + row * D;

    float s = 0; for (int i = tid; i < D; i += blockDim.x) s += xr[i];
    float mean = block_sum(s, s_tmp) / D;

    float v = 0; for (int i = tid; i < D; i += blockDim.x) { float d = xr[i]-mean; v += d*d; }
    float is = rsqrtf(block_sum(v, s_tmp) / D + eps);

    for (int i = tid; i < D; i += blockDim.x) {
        float val = (xr[i] - mean) * is;
        if (w) val *= w[i]; if (b) val += b[i];
        yr[i] = val;
    }
}
void launch_layernorm(const float* x, const float* w, const float* b,
    float* y, int N, int D, float eps, hipStream_t s) {
    layernorm_kernel<<<N, min(D,256), min(D,256)*sizeof(float), s>>>(x, w, b, y, D, eps);
}

// ═══════════════════════════════════════════════════════════════
// 3. RoPE
// ═══════════════════════════════════════════════════════════════
__global__ void rope_kernel(float* __restrict__ x,
    const float* __restrict__ c, const float* __restrict__ si, int T, int D) {
    int t = blockIdx.x, d = threadIdx.x;
    int h = D/2;
    if (t >= T || d >= h) return;
    float* xr = x + t*D;
    float x0 = xr[d], x1 = xr[d+h];
    float cs = c[t*D + d], sn = si[t*D + d];
    xr[d]   = x0*cs - x1*sn;
    xr[d+h] = x1*cs + x0*sn;
}
void launch_rope(float* x, const float* c, const float* si, int T, int D, hipStream_t s) {
    rope_kernel<<<T, D/2, 0, s>>>(x, c, si, T, D);
}

// non-in-place 单 tensor RoPE (可微分), 读 x 写 y
// x,y: [n, D], cos/sin: [T, D], 每行 cos/sin 索引 = row_idx % T
// 每 thread 处理 8 个 D 元素 (2 float4: 前半 + 后半配对)
__global__ void rope_fwd_k(const float* __restrict__ x,
    float* __restrict__ y, const float* __restrict__ cos,
    const float* __restrict__ sin, int n, int T, int D) {
    int h4 = D / 8;  // float4 groups in half
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = n * h4;  // n rows * h4 groups/row
    if (idx >= total) return;
    int g4 = idx % h4;  // float4 group in first half (0..h4-1)
    int t = idx / h4;   // row index
    int pos = t % T;    // position in sequence

    const float4* x4 = (const float4*)(x + t * D);
    float4* y4 = (float4*)(y + t * D);
    const float4* c4 = (const float4*)(cos + pos * D);
    const float4* s4 = (const float4*)(sin + pos * D);

    float4 v0 = x4[g4], v1 = x4[g4 + h4];
    float4 cs = c4[g4], sn = s4[g4];

    y4[g4]      = make_float4(v0.x*cs.x - v1.x*sn.x, v0.y*cs.y - v1.y*sn.y,
                              v0.z*cs.z - v1.z*sn.z, v0.w*cs.w - v1.w*sn.w);
    y4[g4 + h4] = make_float4(v1.x*cs.x + v0.x*sn.x, v1.y*cs.y + v0.y*sn.y,
                              v1.z*cs.z + v0.z*sn.z, v1.w*cs.w + v0.w*sn.w);
}
void launch_rope_fwd(const float* x, float* y, const float* cos,
    const float* sin, int n, int T, int D, hipStream_t s) {
    int total = n * (D / 8);  // n rows * h4 groups/row
    rope_fwd_k<<<(total+255)/256, 256, 0, s>>>(x, y, cos, sin, n, T, D);
}

// float4 向量化批量 RoPE — 一次处理 Q 和 K 所有 B×NH×T token
// Q, K: [N, D] 连续, N = B*NH*T
// cos, sin: [T, D] 预计算
// 每线程处理 4 个 D 元素 (float4), grid = N*D/4
__global__ void rope_batch_k(float* __restrict__ q, float* __restrict__ k,
    const float* __restrict__ cos, const float* __restrict__ sin,
    int N, int T, int D) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = N * D / 4;
    if (idx >= total) return;
    int g4 = idx % (D / 4);       // float4 group (0..15 for D=64)
    int t = idx / (D / 4);        // token index (0..N-1)
    int pos = t % T;              // position in sequence (for cos/sin)

    float4* q4 = (float4*)(q + t * D);
    float4* k4 = (float4*)(k + t * D);
    const float4* c4 = (const float4*)(cos + pos * D);
    const float4* s4 = (const float4*)(sin + pos * D);

    // 4 pairs: (d, d+h) where h = D/2, 在 float4 组中偏移 h/4
    int h4 = D / 8;  // D/2 / 4 groups
    float4 vq0 = q4[g4], vq1 = q4[g4 + h4];
    float4 vk0 = k4[g4], vk1 = k4[g4 + h4];
    float4 cs = c4[g4], sn = s4[g4];

    q4[g4]         = make_float4(vq0.x*cs.x - vq1.x*sn.x, vq0.y*cs.y - vq1.y*sn.y,
                                 vq0.z*cs.z - vq1.z*sn.z, vq0.w*cs.w - vq1.w*sn.w);
    q4[g4 + h4]    = make_float4(vq1.x*cs.x + vq0.x*sn.x, vq1.y*cs.y + vq0.y*sn.y,
                                 vq1.z*cs.z + vq0.z*sn.z, vq1.w*cs.w + vq0.w*sn.w);
    k4[g4]         = make_float4(vk0.x*cs.x - vk1.x*sn.x, vk0.y*cs.y - vk1.y*sn.y,
                                 vk0.z*cs.z - vk1.z*sn.z, vk0.w*cs.w - vk1.w*sn.w);
    k4[g4 + h4]    = make_float4(vk1.x*cs.x + vk0.x*sn.x, vk1.y*cs.y + vk0.y*sn.y,
                                 vk1.z*cs.z + vk0.z*sn.z, vk1.w*cs.w + vk0.w*sn.w);
}
void launch_rope_batch(float* q, float* k, const float* cos, const float* sin,
    int N, int T, int D, hipStream_t s) {
    int total = N * D / 4;
    rope_batch_k<<<(total+255)/256, 256, 0, s>>>(q, k, cos, sin, N, T, D);
}

__global__ void rope_transpose_kernel(const float* __restrict__ x,
    const float* __restrict__ c, const float* __restrict__ si,
    float* __restrict__ out, int B, int NH, int T, int D) {
    int b = blockIdx.x, h = blockIdx.y, t = threadIdx.x, d = threadIdx.y;
    int DH = D / NH;
    if (t >= T || d >= DH) return;
    int gd = h*DH + d;
    float x0 = x[(b*T+t)*D + gd], x1 = x[(b*T+t)*D + D/2 + gd];
    float cos_v = c[t*D + gd], sin_v = si[t*D + gd];
    int out_off = (b*NH + h)*T*D + t*D;
    out[out_off + d] = x0*cos_v - x1*sin_v;
    out[out_off + d + DH] = x1*cos_v + x0*sin_v;
}
void launch_rope_transpose(const float* x, const float* c, const float* si,
    float* out, int B, int NH, int T, int D, hipStream_t s) {
    dim3 grid(B, NH); dim3 block(T, D/NH);
    rope_transpose_kernel<<<grid, block, 0, s>>>(x, c, si, out, B, NH, T, D);
}

// ═══════════════════════════════════════════════════════════════
// 4. N14 门控融合
// ═══════════════════════════════════════════════════════════════
// float4 向量化: 每线程 4 元素, block(D/4), grid(B*T)
// B*T token 按 t%T 取 pos_bias, gate 值相同的 token 共享
__global__ void gate_blend_kernel(const float* __restrict__ x_pe,
    const float* __restrict__ x_rope, float logit,
    const float* __restrict__ pos_bias, float gate_min,
    float* __restrict__ y, int T, int D) {
    int t = blockIdx.x, d4 = threadIdx.x;
    if (d4 >= D/4) return;
    int pos = t % T;
    float raw = logit + pos_bias[pos];
    float gate = 1.0f / (1.0f + expf(-raw));
    gate = fminf(fmaxf(gate, gate_min), 0.99f);
    float g1 = 1.0f - gate;

    int base = t * D + d4 * 4;
    float4* y4 = (float4*)(y + base);
    const float4* pe4 = (const float4*)(x_pe + base);
    const float4* ro4 = (const float4*)(x_rope + base);

    float4 p = *pe4, r = *ro4;
    y4[0] = make_float4(gate*p.x + g1*r.x, gate*p.y + g1*r.y,
                        gate*p.z + g1*r.z, gate*p.w + g1*r.w);
}
void launch_gate_blend(const float* x_pe, const float* x_rope,
    float logit, const float* pos_bias, float gate_min,
    float* y, int B, int T, int D, hipStream_t s) {
    gate_blend_kernel<<<dim3(B*T,1), dim3(D/4,1), 0, s>>>(x_pe, x_rope, logit, pos_bias, gate_min, y, T, D);
}

// Gate blend with pre-computed gate (N14逻辑在 PyTorch 计算, kernel 只做乘加)
__global__ void gate_blend_gate_k(const float* __restrict__ x_pe,
    const float* __restrict__ x_rope, const float* __restrict__ gate,
    float* __restrict__ y, int N, int D) {
    int t = blockIdx.x, d4 = threadIdx.x;
    if (d4 >= D/4) return;
    float g = gate[t], g1 = 1.0f - g;
    int base = t * D + d4 * 4;
    float4* y4 = (float4*)(y + base);
    const float4* pe4 = (const float4*)(x_pe + base);
    const float4* ro4 = (const float4*)(x_rope + base);
    float4 p = *pe4, r = *ro4;
    y4[0] = make_float4(g*p.x + g1*r.x, g*p.y + g1*r.y, g*p.z + g1*r.z, g*p.w + g1*r.w);
}
void launch_gate_blend_gate(const float* x_pe, const float* x_rope,
    const float* gate, float* y, int N, int D, hipStream_t s) {
    gate_blend_gate_k<<<dim3(N,1), dim3(D/4,1), 0, s>>>(x_pe, x_rope, gate, y, N, D);
}

// ═══════════════════════════════════════════════════════════════
// 5. WS + GEMM + GELU (fused_linear_ws_gelu)
// ── 每 block = 32 输出行, 32 K 个线程, warp tile 32×32
// ═══════════════════════════════════════════════════════════════
__global__ void ws_gemm_gelu_kernel(const float* __restrict__ x,
    const float* __restrict__ w, const float* __restrict__ bias,
    const float* __restrict__ mask, float* __restrict__ y,
    int N, int M, int K) {
    extern __shared__ float s_w[];
    int br = blockIdx.x;     // 输出行块 (32行)
    int b_n = blockIdx.y;    // batch
    int tid = threadIdx.x;   // 0..31 (处理 32 行中的 1 行)

    if (br >= M/32) return;
    int out_row = br * 32 + tid;  // 这个线程处理的输出行
    if (out_row >= M) return;

    // Step 1: 权重标准化 (单行)
    float sum = 0, sq = 0;
    for (int k = 0; k < K; k++) {
        float v = w[out_row * K + k];
        sum += v; sq += v * v;
    }
    float mean = sum / K;
    float var = (sq - sum * mean) / (K - 1);  // Bessel: Var=E[x²]-E[x]²
    if (var < 0) var = 0;
    float std = sqrtf(var) + 1e-6f;

    // Step 2: GEMM
    float acc = 0;
    const float* xb = x + b_n * K;
    for (int k = 0; k < K; k++) {
        float w_std = (w[out_row * K + k] - mean) / std;
        if (mask) w_std *= mask[out_row * K + k];
        acc += xb[k] * w_std;
    }
    if (bias) acc += bias[out_row];

    // Step 3: GELU
    float z = acc * 0.70710678f;
    y[b_n * M + out_row] = acc * 0.5f * (1.0f + erff(z));
}
void launch_ws_gemm_gelu(const float* x, const float* w, const float* bias,
    const float* mask, float* y, int N, int M, int K, hipStream_t s) {
    ws_gemm_gelu_kernel<<<dim3(M/32,N), 32, 0, s>>>(x, w, bias, mask, y, N, M, K);
}

// WS + GEMM (无激活)
__global__ void ws_gemm_kernel(const float* __restrict__ x,
    const float* __restrict__ w, const float* __restrict__ bias,
    const float* __restrict__ mask, float* __restrict__ y,
    int N, int M, int K) {
    int out_row = blockIdx.x * blockDim.x + threadIdx.x;
    int b_n = blockIdx.y;
    if (out_row >= M) return;

    float sum = 0, sq = 0;
    for (int k = 0; k < K; k++) { float v = w[out_row*K+k]; sum += v; sq += v*v; }
    float mean = sum/K, var = (sq - sum*mean)/(K-1);
    if (var < 0) var = 0;
    float std = sqrtf(var)+1e-6f;

    float acc = 0; const float* xb = x + b_n*K;
    for (int k = 0; k < K; k++) {
        float ws = (w[out_row*K+k] - mean)/std;
        if (mask) ws *= mask[out_row*K+k];
        acc += xb[k] * ws;
    }
    y[b_n*M + out_row] = acc + (bias ? bias[out_row] : 0);
}
void launch_ws_gemm(const float* x, const float* w, const float* bias,
    const float* mask, float* y, int N, int M, int K, hipStream_t s) {
    ws_gemm_kernel<<<dim3((M+255)/256,N), 256, 0, s>>>(x, w, bias, mask, y, N, M, K);
}

// ═══════════════════════════════════════════════════════════════
// 6. 32×32 块稀疏 GEMM + GELU
// ═══════════════════════════════════════════════════════════════
__global__ void sparse_gemm_kernel(const float* __restrict__ x,
    const float* __restrict__ w, const uint8_t* __restrict__ mask,
    float* __restrict__ y, int N, int M, int K) {
    int br = blockIdx.x, bn = blockIdx.y;
    int r = threadIdx.x;
    if (r >= 32) return;
    int nb_col = K/32;

    const float* w_row = w + (br * 32 + r) * K;
    float* y_out = y + bn * M + br * 32;

    float acc = 0;
    for (int bc = 0; bc < nb_col; bc++) {
        if (mask && !mask[br * nb_col + bc]) continue;
        const float* xb = x + bn * K + bc * 32;
        for (int k = 0; k < 32; k++)
            acc += xb[k] * w_row[bc * 32 + k];
    }
    y_out[r] += acc;
}

// sparse_gemm_gelu_kernel: sparse GEMM with fused GELU
__global__ void sparse_gemm_gelu_kernel(const float* __restrict__ x,
    const float* __restrict__ w, const uint8_t* __restrict__ mask,
    float* __restrict__ y, int N, int M, int K) {
    int br = blockIdx.x, bn = blockIdx.y;
    int r = threadIdx.x;
    if (r >= 32) return;
    int nb_col = K/32;

    const float* w_row = w + (br * 32 + r) * K;
    float acc = 0;
    for (int bc = 0; bc < nb_col; bc++) {
        if (mask && !mask[br * nb_col + bc]) continue;
        const float* xb = x + bn * K + bc * 32;
        for (int k = 0; k < 32; k++)
            acc += xb[k] * w_row[bc * 32 + k];
    }
    float z = acc * 0.70710678f;
    y[bn * M + br * 32 + r] = acc * 0.5f * (1.0f + erff(z));
}

void launch_sparse_gemm(const float* x, const float* w, const uint8_t* mask,
    float* y, int N, int M, int K, hipStream_t s) {
    sparse_gemm_kernel<<<dim3(M/32,N), 32, 0, s>>>(x, w, mask, y, N, M, K);
}

void launch_sparse_gemm_gelu(const float* x, const float* w, const uint8_t* mask,
    float* y, int N, int M, int K, hipStream_t s) {
    sparse_gemm_gelu_kernel<<<dim3(M/32,N), 32, 0, s>>>(x, w, mask, y, N, M, K);
}

// ═══════════════════════════════════════════════════════════════
// 7. 融合注意力 + 双分支门控 (attn_kernel 已废弃, 4K 用 attn2.cu)
// ═══════════════════════════════════════════════════════════════
#if 0
__global__ void attn_kernel(const float* __restrict__ Q,
    const float* __restrict__ K, const float* __restrict__ V,
    float* __restrict__ O, int T, int D, bool causal) {
    int bh = blockIdx.x; int tid = threadIdx.x;
    if (tid >= T) return;

    extern __shared__ float s_tile[];
    float* s_K = s_tile;
    float* s_V = s_tile + 32 * D;

    float m = -1e30f, l = 0;    float O_row[64];
    for (int d = 0; d < 64; d++) O_row[d] = 0;

    for (int kvs = 0; kvs < T; kvs += 32) {
        int Bc = min(32, T - kvs);
        if (tid < D) {
            for (int j = 0; j < Bc; j++) {
                int off = (bh * T + kvs + j) * D + tid;
                s_K[j * D + tid] = K[off];
                s_V[j * D + tid] = V[off];
            }
        }
        __syncthreads();

        float local_max = -1e30f;
        float S_reg[32];
        for (int j = 0; j < Bc; j++) {
            float dot = 0;
            for (int d = 0; d < D; d++)
                dot += Q[(bh * T + tid) * D + d] * s_K[j * D + d];
            float sv = dot / sqrtf((float)D);
            if (causal && tid < kvs + j) sv = -1e30f;
            S_reg[j] = sv;
            if (sv > local_max) local_max = sv;
        }

        float new_m = fmaxf(m, local_max);
        float alpha = expf(m - new_m);
        m = new_m;
        l *= alpha;

        for (int j = 0; j < Bc; j++) {
            float p = expf(S_reg[j] - local_max);
            l += p;
            for (int d = 0; d < D; d++)
                O_row[d] = O_row[d] * alpha + p * s_V[j * D + d];
        }
        __syncthreads();
    }
    if (l < 1e-10f) l = 1e-10f;
    #pragma unroll
    for (int d = 0; d < D; d++)
        O[(bh * T + tid) * D + d] = O_row[d] / l;
}
void launch_attn(const float* q, const float* k, const float* v, float* o,
    int B, int NH, int T, int D, bool causal, hipStream_t s) {
    size_t smem = 2 * 32 * D * sizeof(float);
    attn_kernel<<<B*NH, min(T,256), smem, s>>>(q, k, v, o, T, D, causal);
}
#endif

__global__ void gated_attn_kernel(const float* __restrict__ qr,
    const float* __restrict__ kr, const float* __restrict__ vr,
    const float* __restrict__ qp, const float* __restrict__ kp,
    const float* __restrict__ vp, const float* __restrict__ gate_base,
    float* __restrict__ o, int T, int D) {
    int bh = blockIdx.x; int tid = threadIdx.x;
    extern __shared__ float s_shared[];
    float* s_Kr = s_shared; float* s_Vr = s_shared + 32*D;
    float* s_Kp = s_shared + 64*D; float* s_Vp = s_shared + 96*D;

    float mr = -INFINITY, lr = 0, mp = -INFINITY, lp = 0;
    float Or[64] = {0}, Op[64] = {0};

    for (int kvs = 0; kvs < T; kvs += 32) {
        int Bc = min(32, T-kvs);
        for (int i = tid; i < 32*D; i += blockDim.x) {
            int kk = i/D, kd = i%D;
            if (kk < Bc) {
                int src_off = (bh*T + kvs+kk)*D + kd;
                s_Kr[kk*D+kd] = kr[src_off]; s_Vr[kk*D+kd] = vr[src_off];
                s_Kp[kk*D+kd] = kp[src_off]; s_Vp[kk*D+kd] = vp[src_off];
            }
        }
        __syncthreads();

        if (tid < T) {
            // RoPE branch
            float lmax_r = -INFINITY;
            for (int j = 0; j < Bc; j++) {
                float d = 0;
                for (int d2 = 0; d2 < D; d2++) d += qr[(bh*T+tid)*D+d2]*s_Kr[j*D+d2];
                float s = d*0.125f; if (tid < kvs+j) s = -INFINITY;
                lmax_r = fmaxf(lmax_r, s);
            }
            float nm_r = fmaxf(mr, lmax_r); float al_r = __expf(mr-nm_r);
            mr = nm_r; lr *= al_r;
            for (int j = 0; j < Bc; j++) {
                float d = 0;
                for (int d2 = 0; d2 < D; d2++) d += qr[(bh*T+tid)*D+d2]*s_Kr[j*D+d2];
                float s = d*0.125f; if (tid < kvs+j) s = -INFINITY;
                float p = __expf(s-lmax_r); lr += p;
                for (int d2 = 0; d2 < D; d2++) Or[d2] = Or[d2]*al_r + p*s_Vr[j*D+d2];
            }

            // PE branch
            float lmax_p = -INFINITY;
            for (int j = 0; j < Bc; j++) {
                float d = 0;
                for (int d2 = 0; d2 < D; d2++) d += qp[(bh*T+tid)*D+d2]*s_Kp[j*D+d2];
                float s = d*0.125f; if (tid < kvs+j) s = -INFINITY;
                lmax_p = fmaxf(lmax_p, s);
            }
            float nm_p = fmaxf(mp, lmax_p); float al_p = __expf(mp-nm_p);
            mp = nm_p; lp *= al_p;
            for (int j = 0; j < Bc; j++) {
                float d = 0;
                for (int d2 = 0; d2 < D; d2++) d += qp[(bh*T+tid)*D+d2]*s_Kp[j*D+d2];
                float s = d*0.125f; if (tid < kvs+j) s = -INFINITY;
                float p = __expf(s-lmax_p); lp += p;
                for (int d2 = 0; d2 < D; d2++) Op[d2] = Op[d2]*al_p + p*s_Vp[j*D+d2];
            }
        }
        __syncthreads();
    }

    if (tid < T) {
        float g = 1.0f/(1.0f+__expf(-gate_base[tid]));
        for (int d = 0; d < D; d++)
            o[(bh*T+tid)*D + d] = g*(Op[d]/lp) + (1.0f-g)*(Or[d]/lr);
    }
}
void launch_gated_attn(const float* qr, const float* kr, const float* vr,
    const float* qp, const float* kp, const float* vp,
    const float* gate_base, float* o,
    int B, int NH, int T, int D, hipStream_t s) {
    size_t smem = 4 * 32 * D * sizeof(float);
    gated_attn_kernel<<<B*NH, min(T,256), smem, s>>>(qr, kr, vr, qp, kp, vp, gate_base, o, T, D);
}

// ═══════════════════════════════════════════════════════════════
// 8. Embedding + 残差 Add + CrossEntropy
// ═══════════════════════════════════════════════════════════════
__global__ void embedding_kernel(const int* __restrict__ tokens,
    const float* __restrict__ table, float* __restrict__ y,
    int N, int T, int V, int D) {
    int idx = blockIdx.x*blockDim.x + threadIdx.x;
    int total = N*T*D; if (idx >= total) return;
    int pos = idx / D;
    int tok = tokens[pos];
    if (tok < 0) tok = 0; if (tok >= V) tok = V-1;
    y[idx] = table[tok*D + idx%D];
}
void launch_embedding(const int* tokens, const float* table,
    float* y, int N, int T, int V, int D, hipStream_t s) {
    int total = N*T*D;
    embedding_kernel<<<(total+255)/256, 256, 0, s>>>(tokens, table, y, N, T, V, D);
}

// float4 向量化 add (in-place): x += res
__global__ void add_inplace_kernel(float* __restrict__ x,
    const float* __restrict__ res, int N) {
    int i4 = blockIdx.x * blockDim.x + threadIdx.x;
    int N4 = N / 4;
    if (i4 < N4) {
        float4* x4 = (float4*)x + i4;
        const float4* r4 = (const float4*)(res) + i4;
        float4 vx = *x4, vr = *r4;
        x4[0] = make_float4(vx.x+vr.x, vx.y+vr.y, vx.z+vr.z, vx.w+vr.w);
    }
    int i = N4 * 4 + i4;
    if (i < N) x[i] += res[i];
}

// float4 向量化 add (非 in-place): y = a + b, autograd 安全
__global__ void add_kernel(const float* __restrict__ a,
    const float* __restrict__ b, float* __restrict__ y, int N) {
    int i4 = blockIdx.x * blockDim.x + threadIdx.x;
    int N4 = N / 4;
    if (i4 < N4) {
        const float4* a4 = (const float4*)a + i4;
        const float4* b4 = (const float4*)b + i4;
        float4* y4 = (float4*)y + i4;
        float4 va = *a4, vb = *b4;
        y4[0] = make_float4(va.x+vb.x, va.y+vb.y, va.z+vb.z, va.w+vb.w);
    }
    int i = N4 * 4 + i4;
    if (i < N) y[i] = a[i] + b[i];
}
void launch_add(const float* a, const float* b, float* y, int n, hipStream_t s) {
    int n4 = (n + 3) / 4;
    add_kernel<<<(n4+255)/256, 256, 0, s>>>(a, b, y, n);
}
void launch_add_inplace(float* x, const float* res, int n, hipStream_t s) {
    int n4 = (n + 3) / 4;
    add_inplace_kernel<<<(n4+255)/256, 256, 0, s>>>(x, res, n);
}

__global__ void cross_entropy_kernel(const float* __restrict__ logits,
    const int* __restrict__ targets, float* __restrict__ loss,
    int N, int C) {
    extern __shared__ float s_ce[];
    int i = blockIdx.x, tid = threadIdx.x;
    if (i >= N) return;
    const float* row = logits + i*C;
    int tgt = targets[i]; if (tgt < 0 || tgt >= C) tgt = 0;

    // Block max (shared memory tree reduction)
    s_ce[tid] = -INFINITY;
    for (int j = tid; j < C; j += blockDim.x) s_ce[tid] = fmaxf(s_ce[tid], row[j]);
    for (int s = 1; s < blockDim.x; s <<= 1) {
        __syncthreads();
        if (tid + s < blockDim.x) s_ce[tid] = fmaxf(s_ce[tid], s_ce[tid + s]);
    }
    __syncthreads();
    float mx = s_ce[0];
    __syncthreads();  // 确保所有线程读完 mx 后才允许覆写 s_ce

    // exp sum (shared memory tree)
    s_ce[tid] = 0;
    for (int j = tid; j < C; j += blockDim.x) s_ce[tid] += __expf(row[j] - mx);
    for (int s = 1; s < blockDim.x; s <<= 1) {
        __syncthreads();
        if (tid + s < blockDim.x) s_ce[tid] += s_ce[tid + s];
    }
    __syncthreads();

    if (tid == 0) {
        float log_sum = mx + logf(s_ce[0]);
        loss[i] = -(row[tgt] - log_sum);
    }
}
void launch_cross_entropy(const float* logits, const int* targets,
    float* loss, int N, int C, hipStream_t s) {
    cross_entropy_kernel<<<N, min(C,256), min(C,256)*sizeof(float), s>>>(logits, targets, loss, N, C);
}

// ═══════════════════════════════════════════════════════════════
// 9. 权重重排 + 块掩码 (CPU 端预处理)
// ═══════════════════════════════════════════════════════════════
// 这些是 CPU 操作 (GPU 不做重排), 在 host wrapper 中实现
// 见 sovereign_cuda.py

// ═══════════════════════════════════════════════════════════════
// extern "C" 包装 - 导出 unmangled 符号
// ═══════════════════════════════════════════════════════════════
extern "C" {

void cuda_rmsnorm(float* x, const float* w, float* y, int N, int D, float eps, hipStream_t s) {
    launch_rmsnorm(x, w, y, N, D, eps, s); }
void cuda_rmsnorm_add(const float* x, const float* w, const float* res, float* y, int N, int D, float eps, hipStream_t s) {
    launch_rmsnorm_add(x, w, res, y, N, D, eps, s); }
void cuda_layernorm(const float* x, const float* w, const float* b, float* y, int N, int D, float eps, hipStream_t s) {
    launch_layernorm(x, w, b, y, N, D, eps, s); }
void cuda_rope(float* x, const float* c, const float* si, int T, int D, hipStream_t s) {
    launch_rope(x, c, si, T, D, s); }
void cuda_rope_fwd(const float* x, float* y, const float* cos,
    const float* sin, int n, int T, int D, hipStream_t s) {
    launch_rope_fwd(x, y, cos, sin, n, T, D, s); }
void cuda_rope_batch(float* q, float* k, const float* cos, const float* sin,
    int N, int T, int D, hipStream_t s) {
    launch_rope_batch(q, k, cos, sin, N, T, D, s); }
void cuda_rope_transpose(const float* x, const float* c, const float* si, float* out, int B, int NH, int T, int D, hipStream_t s) {
    launch_rope_transpose(x, c, si, out, B, NH, T, D, s); }
void cuda_gate_blend(const float* x_pe, const float* x_rope, float logit, const float* pos_bias, float gate_min, float* y, int B, int T, int D, hipStream_t s) {
    launch_gate_blend(x_pe, x_rope, logit, pos_bias, gate_min, y, B, T, D, s); }
void cuda_gate_blend_gate(const float* x_pe, const float* x_rope,
    const float* gate, float* y, int N, int D, hipStream_t s) {
    launch_gate_blend_gate(x_pe, x_rope, gate, y, N, D, s); }
void cuda_ws_gemm_gelu(const float* x, const float* w, const float* bias, const float* mask, float* y, int N, int M, int K, hipStream_t s) {
    launch_ws_gemm_gelu(x, w, bias, mask, y, N, M, K, s); }
void cuda_ws_gemm(const float* x, const float* w, const float* bias, const float* mask, float* y, int N, int M, int K, hipStream_t s) {
    launch_ws_gemm(x, w, bias, mask, y, N, M, K, s); }
void cuda_sparse_gemm(const float* x, const float* w, const uint8_t* mask, float* y, int N, int M, int K, hipStream_t s) {
    launch_sparse_gemm(x, w, mask, y, N, M, K, s); }
void cuda_sparse_gemm_gelu(const float* x, const float* w, const uint8_t* mask, float* y, int N, int M, int K, hipStream_t s) {
    launch_sparse_gemm_gelu(x, w, mask, y, N, M, K, s); }
void cuda_attn(const float* q, const float* k, const float* v, float* o, int B, int NH, int T, int D, bool causal, hipStream_t s) {
    /* launch_attn disabled — 4K 用 attn2.cu */ }
void cuda_gated_attn(const float* qr, const float* kr, const float* vr, const float* qp, const float* kp, const float* vp, const float* gate_base, float* o, int B, int NH, int T, int D, hipStream_t s) {
    launch_gated_attn(qr, kr, vr, qp, kp, vp, gate_base, o, B, NH, T, D, s); }
void cuda_embedding(const int* tokens, const float* table, float* y, int N, int T, int V, int D, hipStream_t s) {
    launch_embedding(tokens, table, y, N, T, V, D, s); }
void cuda_add_inplace(float* x, const float* res, int n, hipStream_t s) {
    launch_add_inplace(x, res, n, s); }
void cuda_add(const float* a, const float* b, float* y, int n, hipStream_t s) {
    launch_add(a, b, y, n, s); }
void cuda_cross_entropy(const float* logits, const int* targets, float* loss, int N, int C, hipStream_t s) {
    launch_cross_entropy(logits, targets, loss, N, C, s); }

}
