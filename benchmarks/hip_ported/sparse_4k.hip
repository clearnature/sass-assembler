// Sparse GEMM for 4K — float4 向量化 + 32 token/block tiling
// grid: (M/32, N/32) = (96, 256) = 24,576 blocks
// block: 32 threads × 32 tokens × 32 K, 共享内存 X tile
#include <hip/hip_runtime.h>
#include <stdint.h>

__global__ void __launch_bounds__(32, 4) sparse_4k(const float* x, const float* w,
    const uint8_t* mk, float* y, int N, int M, int K) {
    int br = blockIdx.x, tb = blockIdx.y, t = threadIdx.x;
    int nb_col = K / 32;
    int token_start = tb * 32;
    if (token_start >= N) return;
    int tokens = min(32, N - token_start);

    __shared__ float sX[32][32];
    float acc[32];
    #pragma unroll
    for (int n = 0; n < tokens; n++) acc[n] = 0;

    for (int bc = 0; bc < nb_col; bc++) {
        if (mk && !mk[br * nb_col + bc]) continue;

        // ── float4 向量化加载 X tile ──
        // 每个线程加载一个完整 token 的 32 K 值 (8 × float4)
        for (int n = t; n < tokens; n += 32) {
            const float4* x_row = (const float4*)(x + (token_start + n) * K + bc * 32);
            float4 v0 = x_row[0], v1 = x_row[1], v2 = x_row[2], v3 = x_row[3];
            float4 v4 = x_row[4], v5 = x_row[5], v6 = x_row[6], v7 = x_row[7];
            sX[n][0]=v0.x; sX[n][1]=v0.y; sX[n][2]=v0.z; sX[n][3]=v0.w;
            sX[n][4]=v1.x; sX[n][5]=v1.y; sX[n][6]=v1.z; sX[n][7]=v1.w;
            sX[n][8]=v2.x; sX[n][9]=v2.y; sX[n][10]=v2.z; sX[n][11]=v2.w;
            sX[n][12]=v3.x; sX[n][13]=v3.y; sX[n][14]=v3.z; sX[n][15]=v3.w;
            sX[n][16]=v4.x; sX[n][17]=v4.y; sX[n][18]=v4.z; sX[n][19]=v4.w;
            sX[n][20]=v5.x; sX[n][21]=v5.y; sX[n][22]=v5.z; sX[n][23]=v5.w;
            sX[n][24]=v6.x; sX[n][25]=v6.y; sX[n][26]=v6.z; sX[n][27]=v6.w;
            sX[n][28]=v7.x; sX[n][29]=v7.y; sX[n][30]=v7.z; sX[n][31]=v7.w;
        }
        __syncthreads();

        // ── float4 向量化加载权重 ──
        float w_reg[32];
        const float4* w_row = (const float4*)(w + (br * 32 + t) * K + bc * 32);
        float4 w0 = w_row[0], w1 = w_row[1], w2 = w_row[2], w3 = w_row[3];
        float4 w4 = w_row[4], w5 = w_row[5], w6 = w_row[6], w7 = w_row[7];
        w_reg[0]=w0.x; w_reg[1]=w0.y; w_reg[2]=w0.z; w_reg[3]=w0.w;
        w_reg[4]=w1.x; w_reg[5]=w1.y; w_reg[6]=w1.z; w_reg[7]=w1.w;
        w_reg[8]=w2.x; w_reg[9]=w2.y; w_reg[10]=w2.z; w_reg[11]=w2.w;
        w_reg[12]=w3.x; w_reg[13]=w3.y; w_reg[14]=w3.z; w_reg[15]=w3.w;
        w_reg[16]=w4.x; w_reg[17]=w4.y; w_reg[18]=w4.z; w_reg[19]=w4.w;
        w_reg[20]=w5.x; w_reg[21]=w5.y; w_reg[22]=w5.z; w_reg[23]=w5.w;
        w_reg[24]=w6.x; w_reg[25]=w6.y; w_reg[26]=w6.z; w_reg[27]=w6.w;
        w_reg[28]=w7.x; w_reg[29]=w7.y; w_reg[30]=w7.z; w_reg[31]=w7.w;

        // ── 计算: acc[n] += sum_k sX[n][k] * w_reg[k] ──
        #pragma unroll
        for (int n = 0; n < tokens; n++) {
            #pragma unroll
            for (int k = 0; k < 32; k++)
                acc[n] += sX[n][k] * w_reg[k];
        }
        __syncthreads();
    }

    for (int n = 0; n < tokens; n++)
        atomicAdd(&y[(token_start + n) * M + br * 32 + t], acc[n]);
}

extern "C" void cuda_sparse_4k(const float* x, const float* w, const uint8_t* mk,
    float* y, int N, int M, int K, hipStream_t s) {
    int nb_row = M / 32, nb_tok = (N + 31) / 32;
    sparse_4k<<<dim3(nb_row, nb_tok), 32, 0, s>>>(x, w, mk, y, N, M, K);
}
