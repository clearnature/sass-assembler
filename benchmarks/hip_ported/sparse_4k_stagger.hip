// sparse_4k_stagger.hip — SWMMAC stagger kernel 移植到 sparse GEMM
// 直接复用 bench_stagger_final.cpp 的 wave-level readfirstlane 模式
// 16-chain, rocwmma::SwmmacI4::exec, VGPR~19
#include <rocwmma/rocwmma_16chain.hpp>
#include <hip/hip_runtime.h>
#include <stdint.h>

static constexpr int CH = 16;  // 16-chain SWMMAC

__global__ __launch_bounds__(32,2)
void sparse_4k_stagger(const uint32_t* x_i4, const uint32_t* w_i4,
    const uint8_t* mk, float* y, int N, int M, int K, int* counter) {
    // swmmac stagger: lane 0 claims work, readfirstlane broadcasts
    int wb = 0;
    if (threadIdx.x == 0) wb = atomicAdd(counter, 32);
    wb = __builtin_amdgcn_readfirstlane(wb);
    int total_rows = M / 16;
    if (wb >= total_rows) return;
    int row = wb + threadIdx.x;  // each lane processes one row

    // Accumulator: 16 chains × 8 int32 per chain = 128 int32 = 32 float32
    alignas(32) int32_t acc[CH][8] = {};

    int nb_k = K / 64;  // SWMMAC: K=64 per instruction
    if (nb_k <= 0) nb_k = 1;

    // 加载权重 (pre-loaded, 所有 K 块相同)
    using AR = rocwmma::SwmmacARegsT;
    using BR = rocwmma::SwmmacBRegsT;
    using ACC = rocwmma::SwmmacAccumT;

    // 加载 A (输入激活 — 每行独立)
    AR a_reg;
    if (row < total_rows) {
        a_reg = *reinterpret_cast<const AR*>(x_i4 + row * 2);
    }

    // 遍历 tokens (输出列)
    int nb_tok = N / 16;
    for (int tok = 0; tok < nb_tok; tok++) {
        int token_start = tok * 16;
        alignas(32) int32_t local_acc[CH][8] = {};

        // 加载此 token 的权重
        BR b_reg = *reinterpret_cast<const BR*>(w_i4 + row * 4 + token_start * 4);

        // SWMMAC chain: 16 条指令展开
        for (int k = 0; k < nb_k; k++) {
            #pragma unroll
            for (int cc = 0; cc < CH; cc++) {
                ACC& rc = *reinterpret_cast<ACC*>(local_acc[cc]);
                rc = rocwmma::SwmmacI4::exec(a_reg, b_reg, rc, 0);
            }
        }

        // 存储结果
        for (int cc = 0; cc < CH; cc++) {
            for (int j = 0; j < 8; j++) {
                int out_row = row * CH + cc;
                int out_col = token_start + (j / 2);  // 2 int32 per float32
                if (out_row < M && out_col < N) {
                    atomicAdd((int*)(y + out_row * N + out_col), local_acc[cc][j]);
                }
            }
        }
    }
}

extern "C" void cuda_sparse_4k_stagger(const uint32_t* x_i4, const uint32_t* w_i4,
    const uint8_t* mk, float* y, int N, int M, int K, hipStream_t s) {
    int total_rows = M / 16;
    int launch_waves = (total_rows + 31) / 32 * 2;  // 2x oversubscription
    int32_t* counter;
    hipMalloc(&counter, 4);
    hipMemset(counter, 0, 4);
    sparse_4k_stagger<<<launch_waves, 32, 0, s>>>(x_i4, w_i4, mk, y, N, M, K, counter);
    hipFree(counter);
}
