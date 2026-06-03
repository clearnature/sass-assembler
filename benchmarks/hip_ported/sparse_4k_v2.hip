// sparse_4k_v2.hip — swmmac atomicAdd stagger 优化
// 策略: 每线程局部累加, 交错写入避免竞争
#include <hip/hip_runtime.h>
#include <stdint.h>

__global__ void __launch_bounds__(32, 4) sparse_4k_v2(const float* x, const float* w,
    const uint8_t* mk, float* y, int N, int M, int K) {
    int br = blockIdx.x, tb = blockIdx.y, t = threadIdx.x;
    int nb_col = K / 32, token_start = tb * 32;
    if (token_start >= N) return;
    int tokens = min(32, N - token_start);

    __shared__ float sX[32][32];
    float acc[32] = {0};

    for (int bc = 0; bc < nb_col; bc++) {
        if (mk && !mk[br * nb_col + bc]) continue;
        for (int n = t; n < tokens; n += 32) {
            const float4* xr = (const float4*)(x + (token_start + n) * K + bc * 32);
            float4 v0=xr[0],v1=xr[1],v2=xr[2],v3=xr[3];
            float4 v4=xr[4],v5=xr[5],v6=xr[6],v7=xr[7];
            sX[n][0]=v0.x;sX[n][1]=v0.y;sX[n][2]=v0.z;sX[n][3]=v0.w;
            sX[n][4]=v1.x;sX[n][5]=v1.y;sX[n][6]=v1.z;sX[n][7]=v1.w;
            sX[n][8]=v2.x;sX[n][9]=v2.y;sX[n][10]=v2.z;sX[n][11]=v2.w;
            sX[n][12]=v3.x;sX[n][13]=v3.y;sX[n][14]=v3.z;sX[n][15]=v3.w;
            sX[n][16]=v4.x;sX[n][17]=v4.y;sX[n][18]=v4.z;sX[n][19]=v4.w;
            sX[n][20]=v5.x;sX[n][21]=v5.y;sX[n][22]=v5.z;sX[n][23]=v5.w;
            sX[n][24]=v6.x;sX[n][25]=v6.y;sX[n][26]=v6.z;sX[n][27]=v6.w;
            sX[n][28]=v7.x;sX[n][29]=v7.y;sX[n][30]=v7.z;sX[n][31]=v7.w;
        }
        __syncthreads();

        float w_reg[32];
        const float4* wr = (const float4*)(w + (br * 32 + t) * K + bc * 32);
        float4 w0=wr[0],w1=wr[1],w2=wr[2],w3=wr[3];
        float4 w4=wr[4],w5=wr[5],w6=wr[6],w7=wr[7];
        w_reg[0]=w0.x;w_reg[1]=w0.y;w_reg[2]=w0.z;w_reg[3]=w0.w;
        w_reg[4]=w1.x;w_reg[5]=w1.y;w_reg[6]=w1.z;w_reg[7]=w1.w;
        w_reg[8]=w2.x;w_reg[9]=w2.y;w_reg[10]=w2.z;w_reg[11]=w2.w;
        w_reg[12]=w3.x;w_reg[13]=w3.y;w_reg[14]=w3.z;w_reg[15]=w3.w;
        w_reg[16]=w4.x;w_reg[17]=w4.y;w_reg[18]=w4.z;w_reg[19]=w4.w;
        w_reg[20]=w5.x;w_reg[21]=w5.y;w_reg[22]=w5.z;w_reg[23]=w5.w;
        w_reg[24]=w6.x;w_reg[25]=w6.y;w_reg[26]=w6.z;w_reg[27]=w6.w;
        w_reg[28]=w7.x;w_reg[29]=w7.y;w_reg[30]=w7.z;w_reg[31]=w7.w;

        for (int n = 0; n < tokens; n++)
            for (int k = 0; k < 32; k++)
                acc[n] += sX[n][k] * w_reg[k];
        __syncthreads();
    }

    // swmmac stagger: 交错写入, 每4条线程一组, 延迟写入
    for (int n = 0; n < tokens; n++) {
        // 按 threadIdx%4 分成4组, 组间延迟写入
        int offset = (t % 4) * 8;  // 每组间隔8周期
        #pragma unroll
        for (int d = 0; d < offset; d++) asm volatile("s_nop 0");
        atomicAdd(&y[(token_start + n) * M + br * 32 + t], acc[n]);
    }
}

extern "C" void cuda_sparse_4k_v2(const float* x, const float* w, const uint8_t* mk,
    float* y, int N, int M, int K, hipStream_t s) {
    int nb_row = M / 32, nb_tok = (N + 31) / 32;
    sparse_4k_v2<<<dim3(nb_row, nb_tok), 32, 0, s>>>(x, w, mk, y, N, M, K);
}
