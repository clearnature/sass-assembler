#include <hip/hip_runtime.h>
#include <cstdio>
int main() {
    hipDeviceProp_t p;
    hipGetDeviceProperties(&p,0);
    printf("name: %s\n", p.name);
    printf("multiProcessorCount: %d\n", p.multiProcessorCount);
    printf("maxThreadsPerMultiProcessor: %d\n", p.maxThreadsPerMultiProcessor);
    printf("warpSize: %d\n", p.warpSize);
    printf("maxThreadsPerBlock: %d\n", p.maxThreadsPerBlock);
    printf("clockRate: %d kHz\n", p.clockRate);
    printf("totalGlobalMem: %.1f GB\n", p.totalGlobalMem/1e9);
    printf("sharedMemPerBlock: %zu KB\n", p.sharedMemPerBlock/1024);
    printf("regsPerBlock: %d\n", p.regsPerBlock);
    printf("gcnArchName: %s\n", p.gcnArchName);
    return 0;
}
