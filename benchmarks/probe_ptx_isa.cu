/* Pascal/Volta/Ampere — PTX 内联汇编触发缺失指令 */
#include <cuda_fp16.h>
#include <cuda_bf16.h>

extern "C" {

// ═══ HMMA (Tensor Core MMA) - sm_70+ ═══
__global__ void probe_hmma(int* d, half* a, half* b, int* c) {
    asm volatile(
        "mma.sync.aligned.m16n8k8.row.col.f16.f16.f16.f16 "
        "{%0,%1}, {%2,%3}, {%4}, {%5,%6};\n"
        : "=r"(d[0]), "=r"(d[1])
        : "r"(a[0]), "r"(a[1]), "r"(b[0]), "r"(c[0]), "r"(c[1])
    );
}

// ═══ LDSM (矩阵加载) - sm_75+ ═══
__global__ void probe_ldsm(unsigned* d, unsigned* smem) {
    asm volatile(
        "ldmatrix.sync.aligned.m8n8.x4.shared.b16 {%0,%1,%2,%3}, [%4];\n"
        : "=r"(d[0]), "=r"(d[1]), "=r"(d[2]), "=r"(d[3])
        : "r"(smem)
    );
}

// ═══ STSM (矩阵存储) ═══
__global__ void probe_stsm(unsigned* smem, unsigned* s) {
    asm volatile(
        "stmatrix.sync.aligned.m8n8.x4.shared.b16 [%0], {%1,%2,%3,%4};\n"
        :: "r"(smem), "r"(s[0]), "r"(s[1]), "r"(s[2]), "r"(s[3])
    );
}

// ═══ LDGSTS (异步拷贝) - sm_80+ ═══
__global__ void probe_ldgsts(float* gmem, float* smem) {
    asm volatile(
        "cp.async.ca.shared.global [%0], [%1], 16;\n"
        :: "r"(smem), "l"(gmem)
    );
}

// ═══ WARPSYNC ═══
__global__ void probe_warpsync(int* d) {
    asm volatile("warpsync 0xFFFFFFFF;\n");
    d[0] = 1;
}

// ═══ SHFL 变体 ═══
__global__ void probe_shfl_variants(int* d, int val) {
    int tid = threadIdx.x;
    d[0] = __shfl_sync(0xFFFFFFFF, val, 1);        // SHFL.IDX
    d[1] = __shfl_up_sync(0xFFFFFFFF, val, 2);      // SHFL.UP
    d[2] = __shfl_down_sync(0xFFFFFFFF, val, 4);    // SHFL.DOWN
    d[3] = __shfl_xor_sync(0xFFFFFFFF, val, 8);     // SHFL.BFLY
}

// ═══ VOTE 变体 ═══
__global__ void probe_vote_variants(int* d) {
    int tid = threadIdx.x;
    int pred = (tid < 16);
    d[0] = __any_sync(0xFFFFFFFF, pred);            // VOTE.ANY
    d[1] = __all_sync(0xFFFFFFFF, pred);            // VOTE.ALL
    d[2] = __ballot_sync(0xFFFFFFFF, pred);         // VOTE.BALLOT
}

// ═══ MATCH (warp match) ═══
__global__ void probe_match(int* d, int val) {
    int tid = threadIdx.x;
    unsigned mask = __match_any_sync(0xFFFFFFFF, val);
    d[tid] = __popc(mask);
}

// ═══ PRMT (permute) 变体 ═══
__global__ void probe_prmt_variants(int* d, int a, int b) {
    d[0] = __byte_perm(a, b, 0x3210);  // PRMT forward
    d[1] = __byte_perm(a, b, 0x0123);  // PRMT reverse
}

} // extern "C"
