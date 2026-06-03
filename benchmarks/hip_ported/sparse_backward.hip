// Sparse GEMM Backward: dW = X^T @ dY (仅激活的 32×32 块)
// 前向: y = x @ W^T (masked)
// dL/dW = X^T @ dL/dY — 外积, 只对活跃块计算
//
// 每 block = 一个 32×32 权重块
// 32 线程, 每线程处理 1 行 (32 列)
//
// 内存: 无额外中间张量, 直接在 grad_weight 上 atomicAdd
#include <hip/hip_runtime.h>
#include <stdint.h>

__global__ void sparse_dw_kernel(const float* x, const float* dy,
    const uint8_t* mask, float* dw, int N, int M, int K) {
    // br: 输出行块 (32 行), bc: 输入列块 (32 列)
    int br = blockIdx.x, bc = blockIdx.y;
    int r = threadIdx.x;  // 0..31: 块内行
    if (r >= 32) return;

    int nb_col = K / 32;
    if (mask && !mask[br * nb_col + bc]) return;  // 跳过非活跃块

    // 计算梯度: dw[br*32+r][bc*32+k] = sum_n x[n][bc*32+k] * dy[n][br*32+r]
    // 每个线程负责一行 (r), 遍历所有 k (32 列)
    const float* x_col_base = x + bc * 32;       // X 起点: 列 bc*32
    const float* dy_row = dy + br * 32;           // dY 起点: 行 br*32
    float* dw_row = dw + (br * 32 + r) * K;       // dW 目标行

    for (int k = 0; k < 32; k++) {
        float grad = 0;
        for (int n = 0; n < N; n++) {
            grad += x_col_base[k + n * K] * dy_row[r + n * M];
        }
        // atomicAdd 因为多个 bc 可能写同一行 (不同列)
        atomicAdd(&dw_row[bc * 32 + k], grad);
    }
}

extern "C" void hip_sparse_dw(const float* x, const float* dy,
    const uint8_t* mask, float* dw, int N, int M, int K, hipStream_t s) {
    int nb_row = M / 32, nb_col = K / 32;
    sparse_dw_kernel<<<dim3(nb_row, nb_col), 32, 0, s>>>(x, dy, mask, dw, N, M, K);
}
