/* 最简测试: nvcc kernel → 验证GPU正常 → 自定义cubin */
#include <cuda_runtime.h>
#include <cuda.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

__global__ void ping(float* d) { d[0] = 42.0f; }

int main() {
    // 0. GPU 自检
    printf("[0] GPU自检... ");
    float *d_test; cudaMalloc(&d_test,4);
    ping<<<1,1>>>(d_test); cudaDeviceSynchronize();
    float v; cudaMemcpy(&v,d_test,4,cudaMemcpyDeviceToHost);
    printf("%.0f %s\n",v,v==42?"OK":"FAIL");
    cudaFree(d_test);
    if(v!=42) return 1;

    // 1. 制作 cubin
    printf("[1] 制作cubin...\n");
    system("ptxas -arch=sm_61 -o benchmarks/arch/t2.cubin benchmarks/shell.ptx 2>/dev/null");
    FILE* f=fopen("benchmarks/arch/t2.cubin","rb");
    fseek(f,0,SEEK_END); size_t sz=ftell(f); fseek(f,0,SEEK_SET);
    uint8_t* d=(uint8_t*)malloc(sz); fread(d,1,sz,f); fclose(f);

    // 注入 STG + EXIT 到 NOP 槽
    uint64_t stg=0xeedc2000000702ffULL; // STG.E [R2],RZ
    uint64_t ex =0xe30000000007000fULL; // EXIT
    for(int i=0;i<128/8;i++){
        if(d[0x460+i*8+7]==0x50){memcpy(d+0x460+i*8,&stg,8);break;}
    }
    for(int i=0;i<128/8;i++){
        if(d[0x460+i*8+7]==0x50){memcpy(d+0x460+i*8,&ex,8);break;}
    }
    f=fopen("benchmarks/arch/t2p.cubin","wb");fwrite(d,1,sz,f);fclose(f);
    free(d);

    // 2. cuobjdump
    printf("[2] cuobjdump:\n");
    system("cuobjdump -sass benchmarks/arch/t2p.cubin 2>/dev/null | grep -E 'STG|EXIT' | head -3");

    // 3. GPU 加载
    printf("[3] GPU加载... ");
    CUdevice dev; CUcontext ctx; CUmodule mod; CUfunction func;
    cuInit(0); cuDeviceGet(&dev,0); cuCtxCreate(&ctx,0,dev);

    if(cuModuleLoad(&mod,"benchmarks/arch/t2p.cubin")){printf("FAIL\n");return 1;}
    printf("OK\n[4] 启动... ");
    if(cuModuleGetFunction(&func,mod,"test_kernel")){printf("FAIL\n");return 1;}

    float* d_out; cudaMalloc(&d_out,4); float h_out=-99;
    void* args[]={&d_out};
    if(cuLaunchKernel(func,1,1,1,1,1,1,0,0,args,0)){printf("FAIL\n");return 1;}
    cuCtxSynchronize();
    cudaMemcpy(&h_out,d_out,4,cudaMemcpyDeviceToHost);
    printf("output=%.0f %s\n",h_out,h_out==0?"✅ 自定义SASS执行成功!":"❌");

    cudaFree(d_out); cuModuleUnload(mod); cuCtxDestroy(ctx);
    return 0;
}
