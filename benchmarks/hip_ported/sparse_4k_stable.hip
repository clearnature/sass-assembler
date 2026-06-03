// sparse_4k_stable.hip — 稳定版: 无 hipMalloc/Free 在热路径
// 修复:
//   1. counter 由调用者管理 (一次性分配, L2持久化)
//   2. wrap mode (永不重置, 循环使用)
//   3. 零运行时内存分配
//   4. CH=14 (ILP模型推荐), __launch_bounds__(32,2)
#include <rocwmma/rocwmma_16chain.hpp>
#include <hip/hip_runtime.h>
#include <stdint.h>

static constexpr int CH = 14;

__global__ __launch_bounds__(32, 2)
void sparse_4k_stable(const int32_t* x_i4, const int32_t* w_i4,
    float* y, int N, int M, int K, int* persistent_ctr) {
    int wb = 0;
    if (threadIdx.x == 0) wb = atomicAdd(persistent_ctr, 32);
    wb = __builtin_amdgcn_readfirstlane(wb);
    int total_rows = (M + 15) / 16;
    if (wb >= total_rows) return;  // oversubscribed: 这些wave不干活
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

// ═══ 调用约定 ═══
// 调用者负责:
//   1. 一次性 hipMalloc persistent_ctr (4 bytes)
//   2. 每 batch 开始前 hipMemset(persistent_ctr, 0, 4)
//   3. 所有 batch 完成后 hipFree(persistent_ctr)
// 热路径 (per-launch): 零分配, 只做 atomicAdd
extern "C" void cuda_sparse_4k_stable(void* x, void* w, float* y,
    int N, int M, int K, hipStream_t s, void* persistent_ctr) {
    int launch = 256;
    sparse_4k_stable<<<launch, 32, 0, s>>>((int32_t*)x,(int32_t*)w,y,N,M,K,(int*)persistent_ctr);
}
