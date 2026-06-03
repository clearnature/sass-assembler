// Sparse Backward dW for 4K — 直接 atomicAdd (已验证, 53ms, diff=0.002)
#include <cuda_runtime.h>
#include <stdint.h>

__global__ void __launch_bounds__(32, 4) sparse_dw_4k(const float* x,
    const float* dy, const uint8_t* mk, float* dw, int N, int M, int K) {
    int br = blockIdx.x, tb = blockIdx.y, t = threadIdx.x;
    int nb_col = K / 32;
    int token_start = tb * 32;
    if (token_start >= N) return;
    int tokens = min(32, N - token_start);

    float dy_reg[32];
    for (int n = 0; n < tokens; n++)
        dy_reg[n] = dy[(token_start + n) * M + br * 32 + t];

    __shared__ float sX[32][32];

    for (int bc = 0; bc < nb_col; bc++) {
        if (mk && !mk[br * nb_col + bc]) continue;

        for (int i = t; i < 32 * tokens; i += 32) {
            int n = i / 32, k = i % 32;
            if (n < tokens)
                sX[n][k] = x[(token_start + n) * K + bc * 32 + k];
        }
        __syncthreads();

        for (int k = 0; k < 32; k++) {
            float grad = 0;
            for (int n = 0; n < tokens; n++)
                grad += dy_reg[n] * sX[n][k];
            atomicAdd(&dw[(br * 32 + t) * K + bc * 32 + k], grad);
        }
        __syncthreads();
    }
}

// Sparse dX: dx = dy @ w_std, only active blocks contribute
// dx[n][bc*32+k] += sum_{active br} sum_t dy[n][br*32+t] * w_std[br*32+t][bc*32+k]
// grid: (K/32, ceil(N/32)) — input_col_blocks × token_blocks
__global__ void __launch_bounds__(32, 4) sparse_dx_4k(const float* __restrict__ dy,
    const float* __restrict__ w, const uint8_t* __restrict__ mk,
    float* __restrict__ dx, int N, int M, int K) {
    int bc = blockIdx.x, tb = blockIdx.y, t = threadIdx.x;
    int nb_row = M / 32;
    int token_start = tb * 32;
    if (token_start >= N) return;
    int tokens = min(32, N - token_start);

    __shared__ float sW[32][32];
    __shared__ float sDY[32][32];

    float dx_reg[32];
    for (int n = 0; n < tokens; n++) dx_reg[n] = 0;

    for (int br = 0; br < nb_row; br++) {
        if (mk && !mk[br * (K/32) + bc]) continue;

        for (int i = t; i < 1024; i += 32) {  // 32×32 block
            int row = i / 32, col = i % 32;
            sW[row][col] = w[(br*32+row) * K + bc*32 + col];
        }
        for (int i = t; i < 32 * tokens; i += 32) {
            int n = i / 32, row = i % 32;
            if (row < 32)
                sDY[n][row] = dy[(token_start+n) * M + br*32 + row];
        }
        __syncthreads();

        for (int n = 0; n < tokens; n++) {
            float acc = 0;
            for (int row = 0; row < 32; row++)
                acc += sDY[n][row] * sW[row][t];
            dx_reg[n] += acc;
        }
        __syncthreads();
    }

    for (int n = 0; n < tokens; n++)
        atomicAdd(&dx[(token_start+n) * K + bc*32 + t], dx_reg[n]);
}

extern "C" void cuda_sparse_dw_4k(const float* x, const float* dy,
    const uint8_t* mk, float* dw, int N, int M, int K, cudaStream_t s) {
    int nb_row = M / 32, nb_tok = (N + 31) / 32;
    sparse_dw_4k<<<dim3(nb_row, nb_tok), 32, 0, s>>>(x, dy, mk, dw, N, M, K);
}

extern "C" void cuda_sparse_dx_4k(const float* dy, const float* w,
    const uint8_t* mk, float* dx, int N, int M, int K, cudaStream_t s) {
    int nb_col = K / 32, nb_tok = (N + 31) / 32;
    sparse_dx_4k<<<dim3(nb_col, nb_tok), 32, 0, s>>>(dy, w, mk, dx, N, M, K);
}
