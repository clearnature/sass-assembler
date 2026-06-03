/* 用 GTX 1060 验证 HunTian Pascal 后端 */
#include <cuda_runtime.h>
#include <cstdio>

extern "C" __global__ void verify_kernel(int* out) {
    out[0] = threadIdx.x + blockIdx.x * blockDim.x;
    out[1] = out[0] * 2;
    out[2] = out[1] + 1;
    // 包含: MOV, IADD, IMUL, S2R, EXIT, BRA
}

int main() {
    int* d_out;
    cudaMalloc(&d_out, 3 * sizeof(int));
    verify_kernel<<<1, 32>>>(d_out);
    int h_out[3];
    cudaMemcpy(h_out, d_out, 3*sizeof(int), cudaMemcpyDeviceToHost);
    printf("GPU result: %d %d %d\n", h_out[0], h_out[1], h_out[2]);
    cudaFree(d_out);
    return 0;
}
