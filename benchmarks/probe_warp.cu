/* SM80 warp intrinsics — no asm */
extern "C" {
__global__ void probe_warp(unsigned* d, int val) {
    int tid = threadIdx.x;
    d[0] = __shfl_sync(0xFFFFFFFF, val, 1);
    d[1] = __shfl_up_sync(0xFFFFFFFF, val, 2);
    d[2] = __shfl_down_sync(0xFFFFFFFF, val, 4);
    d[3] = __shfl_xor_sync(0xFFFFFFFF, val, 8);
    d[4] = __any_sync(0xFFFFFFFF, tid < 16);
    d[5] = __all_sync(0xFFFFFFFF, tid < 32);
    d[6] = __ballot_sync(0xFFFFFFFF, tid < 16);
    __syncwarp();
}
__global__ void probe_prmt(int* d, int a, int b) {
    d[0] = __byte_perm(a, b, 0x3210);
}
}
