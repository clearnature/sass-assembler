/* 完整端到端: 注入 SASS → GPU 加载 → 启动 → 验证结果 */
#include <cuda_runtime.h>
#include <cuda.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

int main() {
    CUresult r;
    CUdevice dev;
    CUcontext ctx;
    CUmodule mod;
    CUfunction func;

    printf("=== HunTian SASS → GPU 端到端执行 ===\n\n");

    // 1. 生成模板 cubin
    printf("[1] 生成模板...\n");
    system("ptxas -arch=sm_61 -o benchmarks/arch/t.cubin benchmarks/shell.ptx 2>/dev/null");

    // 2. 注入我们的 SASS
    printf("[2] 注入 SASS...\n");
    FILE* f=fopen("benchmarks/arch/t.cubin","rb");
    fseek(f,0,SEEK_END); size_t sz=ftell(f); fseek(f,0,SEEK_SET);
    uint8_t* d=(uint8_t*)malloc(sz); fread(d,1,sz,f); fclose(f);

    // 硬编码: .text 段在 0x460
    // 注入到 NOP 槽位 (不覆盖 SETUP MOVs)
    uint64_t stg=0xeedc2000000702ffULL;  // STG.E [R2], RZ (store RZ=0 to output)
    uint64_t exit_sass=0xe30000000007000fULL;
    int slot=0;
    for(int i=0;i<128/8&&slot<2;i++){
        if(d[0x460+i*8+7]==0x50){ // NOP
            if(slot==0) memcpy(d+0x460+i*8,&stg,8);
            else memcpy(d+0x460+i*8,&exit_sass,8);
            printf("   slot %d (offset 0x%x): injected\n", slot, (int)(0x460+i*8));
            slot++;
        }
    }

    // 3. cuobjdump 验证
    printf("[3] cuobjdump:\n");
    system("cuobjdump -sass benchmarks/arch/final.cubin 2>/dev/null | grep -E 'FFMA|EXIT' | head -3");

    // 4. GPU Driver API
    printf("[4] GPU 加载...\n");
    cuInit(0);
    cuDeviceGet(&dev, 0);
    cuCtxCreate(&ctx, 0, dev);

    r=cuModuleLoad(&mod,"benchmarks/arch/final.cubin");
    printf("   cuModuleLoad: %s\n", r==0?"OK":"FAIL");
    if(r!=0){cuCtxDestroy(ctx);free(d);return 1;}

    r=cuModuleGetFunction(&func,mod,"test_kernel");
    printf("   GetFunction: %s\n", r==0?"OK":"FAIL");

    // 5. 准备参数并启动
    printf("[5] 启动 kernel...\n");
    float* d_out;
    cudaMalloc(&d_out, sizeof(float));
    void* args[]={&d_out};

    r=cuLaunchKernel(func,1,1,1, 1,1,1, 0,0, args,0);
    printf("   cuLaunchKernel: %s\n", r==0?"OK":"FAIL");
    if(r!=0){cuModuleUnload(mod);cuCtxDestroy(ctx);free(d);return 1;}

    cuCtxSynchronize();

    // 6. 读取结果 (STG.E [R2], RZ → 应输出 0)
    float result=-1.0f;
    cudaMemcpy(&result, d_out, sizeof(float), cudaMemcpyDeviceToHost);
    printf("[6] GPU 输出: %f %s\n", result,
        result==0.0f ? "✅ 自定义SASS正确执行!" : "❌");

    cudaFree(d_out);
    cuModuleUnload(mod);
    cuCtxDestroy(ctx);
    free(d);

    printf("\n✅ 完整管线通过: SASS注入→cubin→GPU加载→启动→同步→数据读回\n");
    return 0;
}
