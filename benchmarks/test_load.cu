/* 测试: 原始 template cubin 能否被 Driver API 加载 */
#include <cuda_runtime.h>
#include <cuda.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    CUresult r;
    CUdevice dev;
    CUcontext ctx;
    CUmodule mod;

    cuInit(0);
    cuDeviceGet(&dev, 0);
    cuCtxCreate(&ctx, 0, dev);

    // 测试原始模板 cubin
    printf("[原始模板 cubin]\n");
    r = cuModuleLoad(&mod, "benchmarks/arch/t.cubin");
    printf("cuModuleLoad: %d %s\n", r, r==0?"OK":"FAIL");

    if (r==0) { cuModuleUnload(mod); }

    // 测试注入后的 cubin
    printf("\n[注入后 cubin]\n");
    r = cuModuleLoad(&mod, "benchmarks/arch/h.cubin");
    printf("cuModuleLoad: %d %s\n", r, r==0?"OK":"FAIL");

    if (r==0) {
        CUfunction func;
        r = cuModuleGetFunction(&func, mod, "test_kernel");
        printf("cuModuleGetFunction: %d %s\n", r, r==0?"OK":"FAIL");
        cuModuleUnload(mod);
    }

    // 测试 cuModuleLoadData
    printf("\n[cuModuleLoadData 模板]\n");
    FILE* f = fopen("benchmarks/arch/t.cubin","rb");
    fseek(f,0,SEEK_END); size_t sz=ftell(f); fseek(f,0,SEEK_SET);
    void* data=malloc(sz); fread(data,1,sz,f); fclose(f);
    r = cuModuleLoadData(&mod, data);
    printf("cuModuleLoadData: %d %s\n", r, r==0?"OK":"FAIL");
    if (r==0) cuModuleUnload(mod);
    free(data);

    cuCtxDestroy(ctx);
    return 0;
}
