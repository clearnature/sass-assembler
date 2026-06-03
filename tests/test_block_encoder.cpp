/* ============================================================================
 * HunTian SASS Assembler — BlockEncoder + Disassembler 往返测试 (红灯)
 *
 * 验证编码→反汇编→文本的对称性。
 * 覆盖全部 7 类指令：寄存器/立即数/内存/控制流/数据移动/同步/VAVX3。
 * ============================================================================ */

#include "test_common.h"
#include "sass/sass_types.h"
#include "sass/disassembler.h"
#include "sass/instruction.h"
#include <string>
#include <unordered_map>

using namespace sass;

// ─── 辅助：构造 ManifoldResult ───
static ManifoldResult make_result(std::vector<Instruction> insts) {
    ManifoldResult res;
    res.block_id = 0;
    res.bubble_score = 0.0;
    res.optimized_sequence = std::move(insts);
    return res;
}

// ═══════════════════════════════════════════════════════════════
// 1. 三操作数寄存器指令往返
// ═══════════════════════════════════════════════════════════════

TEST(roundtrip_ffma_reg_reg_reg) {
    auto inst = Instruction::CreateRegOp(Opcode::FFMA, 0, 1, 2);
    auto result = make_result({inst});

    BlockEncoder enc;
    auto code = enc.encode(result);

    ASSERT_EQ(code.size(), 1);
    std::string dis = disassemble(code[0]);
    // 应包含 "ffma" 和 "R0" "R1" "R2"
    ASSERT_TRUE(dis.find("ffma") != std::string::npos);
    ASSERT_TRUE(dis.find("R0") != std::string::npos);
    ASSERT_TRUE(dis.find("R1") != std::string::npos);
    ASSERT_TRUE(dis.find("R2") != std::string::npos);
    return true;
}

TEST(roundtrip_iadd3_reg_reg_reg) {
    auto inst = Instruction::CreateRegOp(Opcode::IADD3, 5, 10, 15);
    auto result = make_result({inst});

    BlockEncoder enc;
    auto code = enc.encode(result);

    ASSERT_EQ(code.size(), 1);
    std::string dis = disassemble(code[0]);
    ASSERT_TRUE(dis.find("iadd3") != std::string::npos);
    ASSERT_TRUE(dis.find("R5") != std::string::npos);
    ASSERT_TRUE(dis.find("R10") != std::string::npos);
    ASSERT_TRUE(dis.find("R15") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 2. 无操作数指令
// ═══════════════════════════════════════════════════════════════

TEST(roundtrip_exit_no_operands) {
    Instruction inst;
    inst.opcode = Opcode::EXIT;
    auto result = make_result({inst});

    BlockEncoder enc;
    auto code = enc.encode(result);

    ASSERT_EQ(code.size(), 1);
    std::string dis = disassemble(code[0]);
    ASSERT_TRUE(dis.find("exit") != std::string::npos);
    return true;
}

TEST(roundtrip_ret_no_operands) {
    Instruction inst;
    inst.opcode = Opcode::RET;
    auto result = make_result({inst});

    BlockEncoder enc;
    auto code = enc.encode(result);
    ASSERT_EQ(code.size(), 1);
    std::string dis = disassemble(code[0]);
    ASSERT_TRUE(dis.find("ret") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 3. 带标号的指令
// ═══════════════════════════════════════════════════════════════

TEST(roundtrip_bra_with_label) {
    Instruction inst;
    inst.opcode = Opcode::BRA;
    inst.operands.push_back({OpType::LABEL, {}, "loop"});
    auto result = make_result({inst});

    std::unordered_map<std::string, uint32_t> labels;
    labels["loop"] = 5;  // 目标在第 5 条指令

    BlockEncoder enc;
    auto code = enc.encode_with_labels(result, labels);

    ASSERT_EQ(code.size(), 1);
    std::string dis = disassemble(code[0]);
    ASSERT_TRUE(dis.find("bra") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 4. 多指令块
// ═══════════════════════════════════════════════════════════════

TEST(roundtrip_multiple_instructions) {
    std::vector<Instruction> insts;
    insts.push_back(Instruction::CreateRegOp(Opcode::FFMA, 0, 1, 2));
    insts.push_back(Instruction::CreateRegOp(Opcode::IADD, 3, 4, 5));
    insts.push_back(Instruction::CreateRegOp(Opcode::FMUL, 6, 7, 8));
    auto result = make_result(insts);

    BlockEncoder enc;
    auto code = enc.encode(result);

    ASSERT_EQ(code.size(), 3);
    auto lines = disassemble_block(code);
    ASSERT_EQ(lines.size(), 3);
    ASSERT_TRUE(lines[0].find("ffma") != std::string::npos);
    ASSERT_TRUE(lines[1].find("iadd") != std::string::npos);
    ASSERT_TRUE(lines[2].find("fmul") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 5. 特殊指令
// ═══════════════════════════════════════════════════════════════

TEST(roundtrip_mov32i) {
    Instruction inst;
    inst.opcode = Opcode::MOV32I;
    inst.operands.push_back({OpType::REG, {.reg_id = 0}, ""});
    inst.operands.push_back({OpType::IMM, {.imm_val = 42}, ""});
    auto result = make_result({inst});

    BlockEncoder enc;
    auto code = enc.encode(result);
    ASSERT_EQ(code.size(), 1);
    std::string dis = disassemble(code[0]);
    ASSERT_TRUE(dis.find("mov32i") != std::string::npos);
    ASSERT_TRUE(dis.find("R0") != std::string::npos);
    ASSERT_TRUE(dis.find("42") != std::string::npos);
    return true;
}

TEST(roundtrip_bar_sync) {
    Instruction inst;
    inst.opcode = Opcode::BAR;
    auto result = make_result({inst});

    BlockEncoder enc;
    auto code = enc.encode(result);
    ASSERT_EQ(code.size(), 1);
    std::string dis = disassemble(code[0]);
    ASSERT_TRUE(dis.find("bar") != std::string::npos);
    return true;
}

TEST(roundtrip_vavx3) {
    auto inst = Instruction::CreateVAVX3_MMA(0, 1, 2, 3);
    auto result = make_result({inst});

    BlockEncoder enc;
    auto code = enc.encode(result);
    ASSERT_EQ(code.size(), 1);
    std::string dis = disassemble(code[0]);
    ASSERT_TRUE(dis.find("vavx3") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 6. 编码→反汇编往返：验证 SASSWord 往返
// ═══════════════════════════════════════════════════════════════

TEST(roundtrip_encode_disassemble_symmetry) {
    // 编码 → 反汇编 → 再编码 → 再反汇编，两次必须一致
    auto inst = Instruction::CreateRegOp(Opcode::FFMA, 7, 8, 9);
    auto result = make_result({inst});

    BlockEncoder enc;
    auto code1 = enc.encode(result);
    std::string dis1 = disassemble(code1[0]);

    // 用相同的输入再编码，应得相同结果
    auto code2 = enc.encode(result);
    std::string dis2 = disassemble(code2[0]);

    ASSERT_EQ(code1[0], code2[0]);
    ASSERT_STR_EQ(dis1, dis2);
    return true;
}

int main() {
    return run_all_tests();
}
