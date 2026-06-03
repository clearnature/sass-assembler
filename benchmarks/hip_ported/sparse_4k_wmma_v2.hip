// sparse_4k_wmma_v2.hip — 正确的 rocWMMA, 16×16 tile
#include <hip/hip_runtime.h>
#include <rocwmma/rocwmma.hpp>
#include <stdint.h>
using namespace rocwmma;

__global__ void sparse_4k_wmma_v2(const half* x, const half* w,
    const uint8_t* mk, float* y, int N, int M, int K) {
    // 每 block 处理 16 tokens × 16 output rows
    int token_blk = blockIdx.y;  // token block: 16 tokens
    int row_blk = blockIdx.x;    // output row block: 16 rows
    int token_start = token_blk * 16;
    int row_start = row_blk * 16;
    if (token_start >= N || row_start >= M) return;

    fragment<accumulator, 16, 16, 16, float> acc;
    fill_fragment(acc, 0.0f);

    int tiles = K / 16;
    for (int k = 0; k < tiles; k++) {
        fragment<matrix_a, 16, 16, 16, half, row_major> x_frag;
        fragment<matrix_b, 16, 16, 16, half, col_major> w_frag;

        load_matrix_sync(x_frag, x + token_start * K + k * 16, K);
        load_matrix_sync(w_frag, w + row_start * K + k * 16, K);

        mma_sync(acc, x_frag, w_frag, acc);
    }

    // 存储: store_matrix_sync 写入 row-major 输出
    store_matrix_sync(y + token_start * M + row_start, acc, M, mem_row_major);
}

extern "C" void cuda_sparse_4k_wmma_v2(const half* x, const half* w, const uint8_t* mk,
    float* y, int N, int M, int K, hipStream_t s) {
    int nb_row = (M + 15) / 16, nb_tok = (N + 15) / 16;
    sparse_4k_wmma_v2<<<dim3(nb_row, nb_tok), 32, 0, s>>>(x, w, mk, y, N, M, K);
}
