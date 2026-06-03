// Attention 4K — register-tiled: S_reg[16] in regs instead of sScore in smem
// Saves 2KB shared memory (10.8KB → 8.8KB), increases occupancy (8 → 10 blocks/SM)
#include <cuda_runtime.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define WARP     32
#define TILE_K   16
#define TILE_Q   32
#define D_REAL   64
#define D_STRIDE 68

__global__ __launch_bounds__(WARP, 10)
void attn2(const float* __restrict__ Q,
    const float* __restrict__ K, const float* __restrict__ V,
    float* __restrict__ O, int T, int D, bool causal) {
    int bh = blockIdx.x, qb = blockIdx.y, t = threadIdx.x;
    int q_pos = qb * TILE_Q + t;
    bool valid = (q_pos < T);

    extern __shared__ float smem[];
    float* sK = smem;
    float* sV = sK + TILE_K * D_STRIDE;
    // sScore eliminated — use S_reg[16] in registers instead

    float Or[64] = {0.0f};
    float m_global = -1e30f, l_val = 0.0f;
    float scale = rsqrtf((float)D_REAL);
    const float* qr = Q + (bh * T + q_pos) * D_REAL;

    for (int ks = 0; ks < T; ks += TILE_K) {
        int Bc = T - ks; if (Bc > TILE_K) Bc = TILE_K;
        int kv_base = (bh * T + ks) * D_REAL;

        for (int i = t; i < Bc * D_REAL; i += WARP) {
            int j = i / D_REAL, d = i % D_REAL;
            sK[j * D_STRIDE + d] = K[kv_base + j * D_REAL + d];
            sV[j * D_STRIDE + d] = V[kv_base + j * D_REAL + d];
        }
        __syncthreads();

        if (valid) {
            float local_max = -1e30f;
            float S_reg[TILE_K];  // scores in registers, not shared memory

            for (int j = 0; j < Bc; j++) {
                float dot = 0.0f;
                const float* kr = sK + j * D_STRIDE;
                #pragma unroll
                for (int d = 0; d < D_REAL; d += 4) {
                    float4 q4 = *((const float4*)(qr + d));
                    float4 k4 = *((const float4*)(kr + d));
                    dot += q4.x * k4.x + q4.y * k4.y + q4.z * k4.z + q4.w * k4.w;
                }
                float sv = dot * scale;
                if (causal && q_pos < ks + j) sv = -1e30f;
                S_reg[j] = sv;
                if (sv > local_max) local_max = sv;
            }

            float new_m = (m_global > local_max) ? m_global : local_max;
            float alpha = __expf(m_global - new_m);
            m_global = new_m; l_val *= alpha;
            #pragma unroll
            for (int d = 0; d < D_REAL; d += 4) {
                float4* o4 = (float4*)(Or + d);
                o4->x *= alpha; o4->y *= alpha; o4->z *= alpha; o4->w *= alpha;
            }

            for (int j = 0; j < Bc; j++) {
                float p = __expf(S_reg[j] - m_global);
                l_val += p;
                const float* vr = sV + j * D_STRIDE;
                #pragma unroll
                for (int d = 0; d < D_REAL; d += 4) {
                    float4 v4 = *((const float4*)(vr + d));
                    Or[d+0] += p * v4.x; Or[d+1] += p * v4.y;
                    Or[d+2] += p * v4.z; Or[d+3] += p * v4.w;
                }
            }
        }
        __syncthreads();
    }

    if (valid) {
        float inv = (l_val > 1e-10f) ? __frcp_rn(l_val) : 1e10f;
        float* out = O + (bh * T + q_pos) * D_REAL;
        #pragma unroll
        for (int d = 0; d < D_REAL; d += 4) {
            float4 o4; o4.x = Or[d+0] * inv; o4.y = Or[d+1] * inv;
            o4.z = Or[d+2] * inv; o4.w = Or[d+3] * inv;
            *((float4*)(out + d)) = o4;
        }
    }
}

extern "C" void cuda_attn2(const float* q, const float* k, const float* v,
    float* o, int B, int NH, int T, int D, bool causal, cudaStream_t s) {
    int bh_total = B * NH;
    int q_blocks = (T + TILE_Q - 1) / TILE_Q;
    // sK(16×68) + sV(16×68) = 8.7KB, no sScore
    size_t smem = (TILE_K * D_STRIDE * 2) * sizeof(float);
    attn2<<<dim3(bh_total, q_blocks), WARP, smem, s>>>(q, k, v, o, T, D, causal);
}
