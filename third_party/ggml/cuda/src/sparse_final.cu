// Sparse GEMM — 跳过 mask=0 的 32×32 块
// 编译: nvcc -O2 -arch=sm_61 -ccbin gcc-14 -Xcompiler "-fPIC" -shared sparse_final.cu -o libsparse_final.so
#include <cuda_runtime.h>
#include <stdint.h>

__global__ void sparse_k(const float* x, const float* w, const uint8_t* mk,
    float* y, int N, int M, int K) {
    int br = blockIdx.x, bn = blockIdx.y, r = threadIdx.x;
    if (r >= 32) return;
    int nc = K/32;
    const float* wr = w + (br*32+r)*K;
    float acc = 0;
    for (int bc = 0; bc < nc; bc++) {
        if (mk && !mk[br*nc+bc]) continue;
        const float* xb = x + bn*K + bc*32;
        for (int k = 0; k < 32; k++) acc += xb[k] * wr[bc*32+k];
    }
    y[bn*M + br*32 + r] += acc;
}

extern "C" void cuda_sparse_gemm(const float* x, const float* w, const uint8_t* mk,
    float* y, int N, int M, int K, cudaStream_t s) {
    sparse_k<<<dim3(M/32,N), 32, 0, s>>>(x, w, mk, y, N, M, K);
}
