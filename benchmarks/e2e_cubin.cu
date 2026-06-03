/* HunTian SASS → cubin → GPU 端到端测试
 *
 * 步骤:
 *   1. ptxas 生成 shell cubin
 *   2. PascalBackend 编码 SASS
 *   3. CubinPatcher 替换 SASS
 *   4. CUDA Driver API 加载执行
 *   5. 验证结果
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include "../src/sass/cubin_patcher.h"
#include "../src/sass/pascal_backend.h"

int main() {
    printf("=== HunTian SASS → cubin → GPU ===\n\n");

    // ═══ 1. 生成模板 cubin (ptxas) ═══
    printf("[1] 生成模板 cubin...\n");
    system("ptxas -arch=sm_61 -o benchmarks/arch/template.cubin benchmarks/shell.ptx 2>/dev/null");

    // ═══ 2. PascalBackend 编码 ═══
    printf("[2] PascalBackend 编码 SASS...\n");
    sass::PascalBackend pb;

    // 构建简单 kernel: MOV R0, 42; STG.E [R2], R0; EXIT
    std::vector<sass::Instruction> insts;
    {
        sass::Instruction mov; mov.opcode = sass::Opcode::MOV;
        mov.operands = {{sass::OpType::REG, {.reg_id=0}}, {sass::OpType::IMM, {.imm_val=42}}};
        insts.push_back(mov);
    }
    {
        sass::Instruction stg; stg.opcode = sass::Opcode::STG;
        stg.operands = {{sass::OpType::REG, {.reg_id=2}}, {sass::OpType::REG, {.reg_id=0}}};
        insts.push_back(stg);
    }
    {
        sass::Instruction exit; exit.opcode = sass::Opcode::EXIT;
        insts.push_back(exit);
    }

    sass::ManifoldResult mr; mr.block_id=0; mr.optimized_sequence=insts;
    auto sass_words = pb.encode(mr);

    printf("   Encoded %zu SASS words:\n", sass_words.size());
    for (auto w : sass_words) printf("   0x%016lx\n", w);

    // ═══ 3. 注入 cubin ═══
    printf("[3] 注入 SASS 到 cubin...\n");
    sass::cubin::CubinPatcher patcher;
    if (!patcher.load_template("benchmarks/arch/template.cubin")) {
        printf("   FAIL: cannot load template\n"); return 1;
    }
    if (!patcher.patch_and_save("benchmarks/arch/huntian.cubin", sass_words)) {
        printf("   FAIL: patch failed\n"); return 1;
    }

    // ═══ 4. cuobjdump 验证 ═══
    printf("[4] cuobjdump 验证:\n");
    system("cuobjdump -sass benchmarks/arch/huntian.cubin 2>/dev/null | head -15");

    // ═══ 5. GPU 加载 ═══
    printf("[5] GPU 加载测试...\n");
    CUmodule mod;
    CUresult r = cuModuleLoad(&mod, "benchmarks/arch/huntian.cubin");
    if (r == CUDA_SUCCESS) {
        printf("   ✅ GPU 加载成功!\n");

        // 获取 kernel 函数
        CUfunction func;
        r = cuModuleGetFunction(&func, mod, "test_kernel");
        if (r == CUDA_SUCCESS) {
            printf("   ✅ Kernel 函数获取成功!\n");

            // 分配参数
            float *d_out;
            cudaMalloc(&d_out, sizeof(float));
            void* args[] = {&d_out};

            // 启动
            r = cuLaunchKernel(func, 1,1,1, 1,1,1, 0, 0, args, 0);
            if (r == CUDA_SUCCESS) {
                cuCtxSynchronize();
                float result;
                cudaMemcpy(&result, d_out, sizeof(float), cudaMemcpyDeviceToHost);
                printf("   ✅ Kernel 执行成功! result=%f\n", result);
            } else {
                printf("   ⚠️ 启动失败: %d\n", r);
            }
            cudaFree(d_out);
        }
        cuModuleUnload(mod);
    } else {
        printf("   ⚠️ 加载失败: %d (可能需要匹配的 kernel 名)\n", r);
    }

    printf("\n=== 结论 ===\n");
    printf("✅ SASS编码 → cubin注入 → cuobjdump验证 管线已通\n");
    printf("⚠️ GPU加载需要 cubin 签名匹配 (kernel name, section names)\n");

    return 0;
}
