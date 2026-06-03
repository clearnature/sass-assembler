/* SM80 全覆盖探针 — CUDA 内建函数触发指令 */
#include <cuda_fp16.h>
#include <cuda_bf16.h>
#include <mma.h>

extern "C" {

// HMMA via mma.h API
__global__ void probe_hmma_cuda(half* d, half* a, half* b, half* c) {
    using namespace nvcuda;
    wmma::fragment<wmma::matrix_a, 16,16,16,half,wmma::row_major> a_frag;
    wmma::fragment<wmma::matrix_b, 16,16,16,half,wmma::col_major> b_frag;
    wmma::fragment<wmma::accumulator, 16,16,16,half> c_frag;
    wmma::load_matrix_sync(a_frag, a, 16);
    wmma::load_matrix_sync(b_frag, b, 16);
    wmma::fill_fragment(c_frag, 0.0);
    wmma::mma_sync(c_frag, a_frag, b_frag, c_frag);
    wmma::store_matrix_sync(d, c_frag, 16, wmma::mem_row_major);
}

// Warp match
__global__ void probe_match(unsigned* d, int val) {
    unsigned mask = __match_any_sync(0xFFFFFFFF, val);
    d[threadIdx.x] = mask;
}

// PRMT
__global__ void probe_prmt(int* d, int a, int b) {
    d[0] = __byte_perm(a, b, 0x3210);
}

// Async copy (triggers LDGSTS)
__global__ void probe_async_copy(float* gmem) {
    __shared__ float smem[256];
    asm("cp.async.ca.shared.global [%0], [%1], 16;" :: "r"(smem), "l"(gmem));
    asm("cp.async.commit_group;");
}

// WARPSYNC
__global__ void probe_warpsync(int* d) {
    __syncwarp();
    d[0] = 1;
}

} // extern "C"
