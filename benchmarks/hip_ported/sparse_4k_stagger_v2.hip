// sparse_4k_stagger_v2.hip — 正确的 INT4 packing
// SwmmacARegsT = VecT<int32_t, 2> (64bit = 16×4bit)
// SwmmacBRegsT = VecT<int32_t, 4> (128bit = 32×4bit)  
// SwmmacAccumT = VecT<int32_t, 8> (256bit = 8 int32 accumulators)
// SWMMAC: 16×16×64 INT4, dst=v[0:7], A=v[8:9], B=v[10:13]
#include <rocwmma/rocwmma_16chain.hpp>
#include <hip/hip_runtime.h>
#include <stdint.h>

// ILP Task Scheduler: CH=14 optimal (VGPR=118 < 135 dual-wave threshold)
// bench_stagger_final实测: 14ch=1560 TOPs vs 16ch=795 TOPs (+96%)
static constexpr int CH = 14;

__global__ __launch_bounds__(32,2)
void sparse_4k_stagger_v2(const int32_t* x_i4, const int32_t* w_i4,
    const uint8_t* mk, float* y, int N, int M, int K, int* counter) {
    // wave-level work claiming (bench_stagger_final pattern)
    int wb = 0;
    if (threadIdx.x == 0) wb = atomicAdd(counter, 32);
    wb = __builtin_amdgcn_readfirstlane(wb);
    int total_rows = (M + 15) / 16;
    if (wb >= total_rows) return;
    int row_base = wb + threadIdx.x;
    if (row_base >= total_rows) return;

    using AR = rocwmma::SwmmacARegsT;    // 2×int32
    using BR = rocwmma::SwmmacBRegsT;    // 4×int32
    using ACC = rocwmma::SwmmacAccumT;   // 8×int32

    int nb_k = K / 64;
    int nb_tok = (N + 15) / 16;

    // 加载权重行 (16 rows per SwmmacARegsT)
    AR a_reg;
    const int32_t* a_ptr = x_i4 + row_base * 2;  // 2 int32 per 16 rows
    a_reg = *reinterpret_cast<const AR*>(a_ptr);

    for (int t = 0; t < nb_tok; t++) {
        alignas(32) int32_t acc[CH][8] = {};

        // 加载此 token 的权重
        BR b_reg;
        const int32_t* b_ptr = w_i4 + row_base * 4 + t * K/64 * 4;
        b_reg = *reinterpret_cast<const BR*>(b_ptr);

        // SWMMAC 16-chain
        for (int k = 0; k < nb_k; k++) {
            #pragma unroll
            for (int c = 0; c < CH; c++) {
                ACC& rc = *reinterpret_cast<ACC*>(acc[c]);
                rc = rocwmma::SwmmacI4::exec(a_reg, b_reg, rc, 0);
            }
        }

        // 存储: acc[c][j] → y
        int token_start = t * 16;
        for (int c = 0; c < CH; c++) {
            int out_row = row_base * CH + c;
            for (int j = 0; j < 8; j++) {
                int out_col = token_start + j;
                if (out_row < M && out_col < N)
                    y[out_row * N + out_col] += (float)acc[c][j];
            }
        }
    }
}

extern "C" void cuda_sparse_4k_stagger_v2(void* x, void* w,
    void* mk, float* y, int N, int M, int K, hipStream_t s) {
    int total_rows = (M + 15) / 16;
    int launch = (total_rows + 31) / 32;
    int32_t* cnt; hipMalloc(&cnt, 4);
    sparse_4k_stagger_v2<<<launch, 32, 0, s>>>((int32_t*)x,(int32_t*)w,0,y,N,M,K,cnt);
    hipFree(cnt);
}
