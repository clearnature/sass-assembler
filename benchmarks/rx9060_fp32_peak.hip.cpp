/* RX 9060 XT FP32 峰值实测 */
#include <hip/hip_runtime.h>
#include <cstdio>
#include <chrono>

__global__ void fma_peak(float* d, int n) {
    int tid = threadIdx.x + blockIdx.x * blockDim.x;
    float a = 1.0f, b = 2.0f, c = 3.0f;
    if (tid < n) {
        for (int i = 0; i < 10000; i++)
            a = a * b + c;  // FMA
        d[tid] = a;
    }
}

int main() {
    printf("=== RX 9060 XT FP32 峰值实测 ===\n\n");

    int threads = 32 * 32 * 32;  // 32768 threads
    float *d;
    hipMalloc(&d, threads * 4);

    auto t0 = std::chrono::high_resolution_clock::now();
    fma_peak<<<threads/256, 256>>>(d, threads);
    hipDeviceSynchronize();
    auto t1 = std::chrono::high_resolution_clock::now();

    double ms = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count() / 1000.0;
    double ops = (double)threads * 10000.0 * 2.0;  // FMA = 2 ops
    double tflops = ops / (ms / 1000.0) / 1e12;

    printf("线程数: %d\n", threads);
    printf("耗时: %.1f ms\n", ms);
    printf("实测: %.2f TFLOPS\n", tflops);
    printf("理论: 22.8 TFLOPS (base 2780MHz) ~ 24.6 TFLOPS (boost 3000MHz)\n");
    printf("官方: 25.6 TFLOPS\n");
    printf("利用率: %.0f%%\n", tflops/22.8*100);

    hipFree(d);
    return 0;
}
