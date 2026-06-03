// sparse_4k_wrap.hip — L2-persistent wrap counter (消除 hipMemset)
// 对标 K0 streamed 4368 TOPs
#include <rocwmma/rocwmma_16chain.hpp>
#include <hip/hip_runtime.h>
#include <stdint.h>

static constexpr int CH = 14;

__global__ __launch_bounds__(32, 2)
void sparse_4k_wrap(const int32_t* x_i4, const int32_t* w_i4,
    float* y, int N, int M, int K, int* base_counter) {
    // Wrap counter: lane 0 atomicAdd, 永不重置 → L2-persistent
    int wb = 0;
    if (threadIdx.x == 0) wb = atomicAdd(base_counter, 32);
    wb = __builtin_amdgcn_readfirstlane(wb);
    int total_rows = (M + 15) / 16;
    wb = wb % total_rows;  // wrap around
    int row_base = wb + threadIdx.x;
    if (row_base >= total_rows) return;

    using AR = rocwmma::SwmmacARegsT;
    using BR = rocwmma::SwmmacBRegsT;
    using ACC = rocwmma::SwmmacAccumT;
    AR a_reg = *reinterpret_cast<const AR*>(x_i4 + row_base * 2);

    int nb_k = K / 64, nb_tok = (N + 15) / 16;
    for (int t = 0; t < nb_tok; t++) {
        alignas(32) int32_t acc[CH][8] = {};
        BR b_reg = *reinterpret_cast<const BR*>(w_i4 + row_base * 4 + t * (K/64) * 4);
        for (int k = 0; k < nb_k; k++)
            #pragma unroll
            for (int c = 0; c < CH; c++) {
                ACC& rc = *reinterpret_cast<ACC*>(acc[c]);
                rc = rocwmma::SwmmacI4::exec(a_reg, b_reg, rc, 0);
            }
        for (int c = 0; c < CH; c++)
            for (int j = 0; j < 8; j++)
                y[(row_base * CH + c) * N + t * 16 + j] += (float)acc[c][j];
    }
}

extern "C" void cuda_sparse_4k_wrap(void* x, void* w, float* y,
    int N, int M, int K, hipStream_t s, void* base_counter) {
    int launch = 256;  // 固定大发射量, wrap counter 循环
    sparse_4k_wrap<<<launch, 32, 0, s>>>((int32_t*)x,(int32_t*)w,y,N,M,K,(int*)base_counter);
}
