/* 优化 pass 集成验证: 循环展开 + Bank 冲突避免 + 寄存器分配
 *
 * 构造特定指令模式, 验证三个 pass 的实际效果:
 *   1. loop_unroll: IADD;ISETP;BRA 模式 → 循环体副本插入, 步进 ×2
 *   2. avoid_bank_conflicts: 同 bank 连续 LDS → 中间插入非内存指令
 *   3. reg_allocate: 长序列 (>=8) → 寄存器重命名到 <=48 物理寄存器
 */
#include <vector>
#include <cstdio>
#include <cstring>
#include <set>
#include "sass/instruction.h"
#include "sass/optimizer.h"

using namespace sass;
using namespace sass::optimizer;

// ─── 辅助: 构造内存操作数 ───
static Operand make_mem(uint8_t base, int16_t offset) {
    Operand op;
    op.type = OpType::MEM;
    op.mem.base = base;
    op.mem.offset = offset;
    return op;
}

// ═══════════════════════════════════════════════════════════
// 1. 循环展开验证
// ═══════════════════════════════════════════════════════════
static bool test_loop_unroll() {
    std::printf("[循环展开] ");
    // 构造: 4条循环体 + IADD R0,R0,#1 + ISETP + BRA
    std::vector<Instruction> insts;
    // 循环体: 4 条独立 FMA
    for (int i = 0; i < 4; i++) {
        insts.push_back(Instruction::CreateRegOp(Opcode::FFMA,
            (uint8_t)(10+i), (uint8_t)(20+i), (uint8_t)(30+i)));
    }
    // IADD R0, R0, #1
    Instruction iadd; iadd.opcode = Opcode::IADD;
    iadd.operands = {
        {OpType::REG, {.reg_id=0}, ""},
        {OpType::REG, {.reg_id=0}, ""},
        {OpType::IMM, {.imm_val=1}, ""}
    };
    insts.push_back(iadd);
    // ISETP
    insts.push_back(Instruction::CreateRegOp(Opcode::ISETP, 0, 0, 1));
    // BRA
    Instruction bra; bra.opcode = Opcode::BRA;
    bra.operands = {{OpType::LABEL, {.reg_id=0}, "loop_start"}};
    insts.push_back(bra);

    size_t orig_size = insts.size();  // 7
    loop_unroll(insts, 2);
    size_t new_size = insts.size();

    if (new_size != orig_size + 4) {
        std::printf("FAIL: 展开后 %zu 条 (期望 %zu)\n", new_size, orig_size + 4);
        return false;
    }
    // 验证 IADD 步进翻倍 (从 #1 → #2)
    // 展开后布局: [4条循环体][4条副本][IADD][ISETP][BRA]
    // IADD 在 index 8
    const Instruction& modified_iadd = insts[8];
    if (modified_iadd.opcode != Opcode::IADD) {
        std::printf("FAIL: index 8 不是 IADD\n");
        return false;
    }
    int64_t step = modified_iadd.operands[2].imm_val;
    if (step != 2) {
        std::printf("FAIL: 步进=%lld (期望 2)\n", (long long)step);
        return false;
    }
    std::printf("PASS: %zu→%zu 条, 步进 1→%lld\n", orig_size, new_size, (long long)step);
    return true;
}

// ═══════════════════════════════════════════════════════════
// 2. Bank 冲突避免验证
// ═══════════════════════════════════════════════════════════
static bool test_bank_conflict_avoidance() {
    std::printf("[Bank冲突] ");
    // 构造: LDS R0, [R10+0]; LDS R1, [R10+0] (同 bank=0) + 一条独立 FMA
    std::vector<Instruction> insts;
    Instruction lds1; lds1.opcode = Opcode::LDS;
    lds1.operands = {{OpType::REG, {.reg_id=0}, ""}, make_mem(10, 0)};
    Instruction lds2; lds2.opcode = Opcode::LDS;
    lds2.operands = {{OpType::REG, {.reg_id=1}, ""}, make_mem(10, 0)};  // offset=0 → bank 0 (同 bank)
    Instruction fma = Instruction::CreateRegOp(Opcode::FFMA, 5, 6, 7);  // 独立指令
    insts.push_back(lds1);
    insts.push_back(lds2);
    insts.push_back(fma);

    avoid_bank_conflicts(insts);

    // 验证: FMA 被移到两条 LDS 之间
    if (insts[1].opcode != Opcode::FFMA) {
        std::printf("FAIL: index 1 是 %u (期望 FFMA)\n", (unsigned)insts[1].opcode);
        return false;
    }
    if (insts[0].opcode != Opcode::LDS || insts[2].opcode != Opcode::LDS) {
        std::printf("FAIL: LDS 未在 0 和 2 位置\n");
        return false;
    }
    std::printf("PASS: 同bank LDS 被 FMA 打散\n");
    return true;
}

// ═══════════════════════════════════════════════════════════
// 3. 寄存器分配验证
// ═══════════════════════════════════════════════════════════
static bool test_reg_allocate() {
    std::printf("[寄存器分配] ");
    // 构造 16 条指令, 使用 R100~R115 (超过 MAX_REGS=48)
    std::vector<Instruction> insts;
    for (int i = 0; i < 16; i++) {
        insts.push_back(Instruction::CreateRegOp(Opcode::FFMA,
            (uint8_t)(100+i), (uint8_t)(100+i), (uint8_t)(100+i)));
    }
    reg_allocate(insts);

    // 验证: 所有寄存器 ID <= 47 (MAX_REGS=48)
    std::set<int> used_regs;
    for (auto& inst : insts) {
        for (auto& op : inst.operands) {
            if (op.type == OpType::REG && op.reg_id != 255) {  // RZ=255
                used_regs.insert(op.reg_id);
                if (op.reg_id >= 48) {
                    std::printf("FAIL: R%d >= 48\n", op.reg_id);
                    return false;
                }
            }
        }
    }
    std::printf("PASS: %zu 个寄存器全部 <= 47\n", used_regs.size());
    return true;
}

// ═══════════════════════════════════════════════════════════
// 4. 短序列不触发寄存器分配验证
// ═══════════════════════════════════════════════════════════
static bool test_short_sequence_preserved() {
    std::printf("[短序列保护] ");
    // 3 条指令 (R5, R10, R15) → 不应被重命名
    std::vector<Instruction> insts;
    insts.push_back(Instruction::CreateRegOp(Opcode::IADD, 5, 10, 15));
    insts.push_back(Instruction::CreateRegOp(Opcode::FFMA, 5, 10, 15));
    insts.push_back(Instruction::CreateRegOp(Opcode::EXIT, 0, 0, 0));

    reg_allocate(insts);

    if (insts[0].operands[0].reg_id != 5) {
        std::printf("FAIL: R5 被重命名为 R%d\n", insts[0].operands[0].reg_id);
        return false;
    }
    std::printf("PASS: 3条指令 R5 保持不变\n");
    return true;
}

int main() {
    std::printf("=== 优化 pass 集成验证 ===\n\n");
    bool ok = true;
    ok &= test_loop_unroll();
    ok &= test_bank_conflict_avoidance();
    ok &= test_reg_allocate();
    ok &= test_short_sequence_preserved();
    std::printf("\n%s\n", ok ? "[全部 PASS]" : "[存在 FAIL]");
    return ok ? 0 : 1;
}
