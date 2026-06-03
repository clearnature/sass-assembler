// Attention — 双 bug 修复完成
// Bug1: Or *= al 移到 j 循环外 (不反复乘 alpha)
// Bug2: p = expf(S - new_m) 使用全局 max, 非局部 max
#include <cuda_runtime.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

__global__ void __launch_bounds__(64, 2) attn_fixed2(const float* Q, const float* K, const float* V,
    float* O, int T, int D, bool causal) {
    int bh = blockIdx.x, t = threadIdx.x;

    __shared__ float sK[16][64];
    __shared__ float sV[16][64];

    float m_global = -1e20f, l = 0;
    float Or[64]; for (int d = 0; d < D; d++) Or[d] = 0;

    for (int ks = 0; ks < T; ks += 16) {
        int Bc = min(16, T - ks);

        for (int i = t; i < 16 * D; i += blockDim.x) {
            int kk = i / D, kd = i % D;
            if (kk < Bc) {
                int off = (bh * T + ks + kk) * D + kd;
                sK[kk][kd] = K[off];
                sV[kk][kd] = V[off];
            }
        }
        __syncthreads();

        if (t < T) {
            float local_max = -1e20f;
            float S_reg[16];
            for (int j = 0; j < Bc; j++) {
                float dot = 0;
                for (int d = 0; d < D; d++)
                    dot += Q[(bh * T + t) * D + d] * sK[j][d];
                float sv = dot / sqrtf((float)D);
                if (causal && t < ks + j) sv = -1e20f;
                S_reg[j] = sv;
                if (sv > local_max) local_max = sv;
            }

            float new_m = fmaxf(m_global, local_max);
            float alpha = expf(m_global - new_m);
            m_global = new_m;

            // [Bug 1 修复] Or 和 l 的 alpha 缩放 — 在 j 循环外, 只做一次
            for (int d = 0; d < D; d++) Or[d] *= alpha;
            l *= alpha;

            for (int j = 0; j < Bc; j++) {
                // [Bug 2 修复] p = expf(S - new_m), 使用全局 max m_global
                // 但 m_global == new_m, 所以用 new_m
                float p = expf(S_reg[j] - new_m);
                l += p;
                for (int d = 0; d < D; d++)
                    Or[d] += p * sV[j][d];  // 纯累加, 不再乘 alpha
            }
        }
        __syncthreads();
    }

    if (t < T) {
        if (l < 1e-10f) l = 1e-10f;
        for (int d = 0; d < D; d++)
            O[(bh * T + t) * D + d] = Or[d] / l;
    }
}

extern "C" void cuda_attn(const float* q, const float* k, const float* v,
    float* o, int B, int NH, int T, int D, bool causal, cudaStream_t s) {
    attn_fixed2<<<B*NH, min(T, 64), 0, s>>>(q, k, v, o, T, D, causal);
}
