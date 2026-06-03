/* Pascal GP106 — 补全最后8条指令探针: LD, ST, JMX, I2IP, SGXT, CCTL */
#include <cuda_runtime.h>

extern "C" {

// LD/ST: generic load/store (通过void*泛型指针触发)
__global__ void probe_ld_st(int* out, void* generic_ptr, int n) {
    int tid = threadIdx.x;
    if (tid < n) {
        // 泛型指针 → ptxas 可能生成 LD/ST 而非 LDG/STG
        int* ip = (int*)generic_ptr;
        int val = ip[tid];           // LD (generic load)
        out[tid] = val + 1;          // ST (generic store pattern)
    }
}

// I2IP: Integer to Integer Pack (通过 sub-word 操作触发)
__global__ void probe_i2ip(short* out, int a, int b) {
    // 将两个 int 的低16位打包为 short2
    short sa = (short)(a & 0xFFFF);
    short sb = (short)(b & 0xFFFF);
    out[0] = sa;
    out[1] = sb;                    // I2IP 可能在此触发
}

// JMX: indirect jump (通过函数指针触发)
__device__ int target_a(int x) { return x + 1; }
__device__ int target_b(int x) { return x * 2; }

__global__ void probe_jmx(int* out, int* in, int n) {
    int tid = threadIdx.x;
    if (tid < n) {
        // switch 语句 → 跳转表 → JMX
        switch (tid % 4) {
            case 0: out[tid] = in[tid] + 1; break;
            case 1: out[tid] = in[tid] * 2; break;
            case 2: out[tid] = in[tid] - 1; break;
            default: out[tid] = 0;
        }
    }
}

// CCTL: cache control (通过 __ldg / prefetch 触发)
__global__ void probe_cctl(int* out, const int* in, int n) {
    int tid = threadIdx.x;
    if (tid < n) {
        // __ldg 是 LDG.E.CONSTANT → 可能触发 CCTL
        out[tid] = __ldg(&in[tid]);
    }
}

} // extern "C"
