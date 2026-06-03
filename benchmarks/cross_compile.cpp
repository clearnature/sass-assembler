/* 全架构汇编器 — 统一交叉编译脚本 */
#include <cstdio>
#include <cuda_runtime.h>

extern "C" {

// 整数全覆盖
__global__ void cover_int(int* d, int a, int b) {
    int tid = threadIdx.x;
    int x = a + b;           // IADD
    int y = a - b;           // ISUB
    int z = a * b;           // IMUL
    int w = __popc(a);       // POPC
    int v = min(a, b);       // IMNMX
    int u = max(a, b);       // IMNMX
    d[tid] = x + y + z + w + v + u;
}
// 浮点全覆盖
__global__ void cover_float(float* d, float a, float b, float c) {
    int tid = threadIdx.x;
    d[0] = a + b;             // FADD
    d[1] = a * b;             // FMUL
    d[2] = a * b + c;         // FFMA
    d[3] = 1.0f / a;          // MUFU.RCP
    d[4] = sqrtf(a);          // MUFU.SQRT
    d[5] = (a > b) ? a : b;   // FSEL/FMNMX
}
// 控制流全覆盖
__global__ void cover_cf(int* d, int n) {
    int tid = threadIdx.x;
    int sum = 0;
    for (int i = 0; i < n && i < 100; i++) {
        sum += i;
        if (sum > 1000) break;  // BRA, BRK
    }
    d[tid] = sum;
}
// 内存全覆盖
__global__ void cover_mem(int* d, const int* s) {
    extern __shared__ int sm[];
    int tid = threadIdx.x;
    sm[tid] = s[tid];         // LDG
    __syncthreads();           // BAR
    d[tid] = sm[tid] * 2;     // LDS
    sm[tid] = d[tid];         // STS
    __syncthreads();           // BAR
    d[tid] = sm[tid];          // STG via store
}
// 双精度
__global__ void cover_double(double* d, double a, double b) {
    d[0] = a + b;              // DADD
    d[1] = a * b;              // DMUL
}
// 位操作
__global__ void cover_bit(int* d, int a, int b) {
    d[0] = a & b;              // LOP.AND
    d[1] = a | b;              // LOP.OR
    d[2] = a ^ b;              // LOP.XOR
    d[3] = (a >> 8) & 0xFF;   // BFE/SHR
    d[4] = __brev(a);          // BREV
}
}

int main(int argc, char** argv) {
    int arch = 61;
    if (argc > 1) arch = atoi(argv[1]);

    // 编译每种架构的 cubin
    char cmd[512];
    for (int sm : {61, 70, 75, 80, 86, 89, 90, 100}) {
        if (arch && sm != arch) continue;
        snprintf(cmd, sizeof(cmd),
            "nvcc -arch=sm_%d -cubin -o benchmarks/arch/x_sm%d.cubin %s 2>/dev/null", sm, sm, __FILE__);
        printf("[sm_%d] %s\n", sm, system(cmd) == 0 ? "OK" : "FAIL");
        snprintf(cmd, sizeof(cmd),
            "cuobjdump -sass benchmarks/arch/x_sm%d.cubin > benchmarks/arch/x_sm%d.sass 2>/dev/null", sm, sm);
        system(cmd);
    }
    return 0;
}
