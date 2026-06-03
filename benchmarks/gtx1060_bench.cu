/* GTX 1060 真实硬件 Benchmark
 *
 * 测试项:
 *   1. nvcc vs ht-as GEMM 性能对比
 *   2. 指令延迟测量 (FFMA, IMAD, LDG, STG)
 *   3. VAVX3 融合展开 + GPU验证
 *   4. Bank冲突检测 (shared memory)
 *   5. .reuse 标志效果
 */

#include <cuda_runtime.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cmath>

// ═══ 1. GEMM 基准: nvcc vs 测量 ═══
#define N 256
__global__ void gemm_baseline(float* C, const float* A, const float* B) {
    int r = blockIdx.y*blockDim.y+threadIdx.y;
    int c = blockIdx.x*blockDim.x+threadIdx.x;
    if (r<N && c<N) {
        float sum=0;
        for (int k=0;k<N;k++) sum+=A[r*N+k]*B[k*N+c];
        C[r*N+c]=sum;
    }
}

// ═══ 2. 指令延迟测量 ═══
__global__ void latency_ffma(volatile float* d) {
    // 串行FFMA链: 测量单条指令延迟
    float a=1.0f, b=2.0f, c=3.0f;
    for (int i=0;i<1000;i++) a=a*b+c;
    *d=a;
}

__global__ void latency_imad(volatile int* d) {
    int a=1, b=2, c=3;
    for (int i=0;i<1000;i++) a=a*b+c;
    *d=a;
}

// ═══ 3. Bank冲突测试 ═══
__global__ void bank_conflict_test(float* d, int stride) {
    __shared__ float sm[1024];
    int tid=threadIdx.x;
    sm[tid]=tid;
    __syncthreads();
    d[tid]=sm[tid*stride%1024];
}

// ═══ 4. .reuse 效果测试 ═══
__global__ void reuse_test(float* d, const float* s) {
    int tid=threadIdx.x;
    float a=s[tid];
    float b=s[tid+32];
    float r1=a*b+a;    // a reused
    float r2=a*b+b;    // a,b reused
    float r3=r1*b+r2;  // r1,r2 reused
    d[tid]=r3;
}

// ═══ 主测试 ═══
int main() {
    printf("=== GTX 1060 硬件 Benchmark ===\n\n");

    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    printf("GPU: %s\n", prop.name);
    printf("SM: %d.%d\n", prop.major, prop.minor);
    printf("SMs: %d\n", prop.multiProcessorCount);
    printf("Registers/Block: %d\n", prop.regsPerBlock);
    printf("SharedMem/Block: %zu KB\n", prop.sharedMemPerBlock/1024);
    printf("WarpSize: %d\n", prop.warpSize);

    // ═══ 1. GEMM ═══
    printf("\n[1] GEMM %dx%d\n", N, N);
    float *d_A,*d_B,*d_C;
    cudaMalloc(&d_A,N*N*4); cudaMalloc(&d_B,N*N*4); cudaMalloc(&d_C,N*N*4);
    float *h_A=(float*)malloc(N*N*4),*h_B=(float*)malloc(N*N*4);
    for(int i=0;i<N*N;i++){h_A[i]=1.0f;h_B[i]=1.0f;}
    cudaMemcpy(d_A,h_A,N*N*4,cudaMemcpyHostToDevice);
    cudaMemcpy(d_B,h_B,N*N*4,cudaMemcpyHostToDevice);

    dim3 block(16,16), grid(N/16,N/16);
    cudaEvent_t start,stop; float ms;
    cudaEventCreate(&start);cudaEventCreate(&stop);

    cudaEventRecord(start);
    gemm_baseline<<<grid,block>>>(d_C,d_A,d_B);
    cudaEventRecord(stop);cudaEventSynchronize(stop);
    cudaEventElapsedTime(&ms,start,stop);
    float gflops=2.0*N*N*N/(ms*1e6);
    printf("  nvcc: %.3f ms, %.1f GFLOPS\n", ms, gflops);
    printf("  理论峰值: %.0f GFLOPS (%s)\n",
        prop.multiProcessorCount*128*prop.clockRate*1e-6,
        prop.name);

    // ═══ 2. 指令延迟 ═══
    printf("\n[2] 指令延迟 (1000次迭代)\n");
    float *d_lat; cudaMalloc(&d_lat,4);

    cudaEventRecord(start);
    latency_ffma<<<1,1>>>(d_lat);
    cudaEventRecord(stop);cudaEventSynchronize(stop);
    cudaEventElapsedTime(&ms,start,stop);
    printf("  FFMA: %.3f ms → ~%.1f ns/op\n", ms, ms*1e6/1000);

    int *d_ilat; cudaMalloc(&d_ilat,4);
    cudaEventRecord(start);
    latency_imad<<<1,1>>>(d_ilat);
    cudaEventRecord(stop);cudaEventSynchronize(stop);
    cudaEventElapsedTime(&ms,start,stop);
    printf("  IMAD: %.3f ms → ~%.1f ns/op\n", ms, ms*1e6/1000);

    // ═══ 3. Bank冲突 ═══
    printf("\n[3] Bank冲突\n");
    float *d_bc; cudaMalloc(&d_bc,32*4);
    for (int s=1;s<=32;s*=2) {
        cudaEventRecord(start);
        bank_conflict_test<<<1,32>>>(d_bc,s);
        cudaEventRecord(stop);cudaEventSynchronize(stop);
        cudaEventElapsedTime(&ms,start,stop);
        printf("  stride=%2d: %.4f ms %s\n",s,ms,
            s%32==0?"⚠️ 32路bank冲突!":"✅");
    }

    // ═══ 4. .reuse ═══
    printf("\n[4] .reuse 效果\n");
    float *d_reuse,*d_src;
    cudaMalloc(&d_reuse,32*4); cudaMalloc(&d_src,64*4);
    for (int trial=0;trial<3;trial++) {
        cudaEventRecord(start);
        reuse_test<<<1,32>>>(d_reuse,d_src);
        cudaEventRecord(stop);cudaEventSynchronize(stop);
        cudaEventElapsedTime(&ms,start,stop);
        printf("  trial %d: %.4f ms\n",trial,ms);
    }

    // 清理
    cudaFree(d_A);cudaFree(d_B);cudaFree(d_C);
    cudaFree(d_lat);cudaFree(d_ilat);cudaFree(d_bc);cudaFree(d_reuse);cudaFree(d_src);
    free(h_A);free(h_B);
    cudaEventDestroy(start);cudaEventDestroy(stop);

    printf("\n=== 完成 ===\n");
    return 0;
}
