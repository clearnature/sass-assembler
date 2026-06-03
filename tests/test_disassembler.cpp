/* ============================================================================
 * HunTian SASS Assembler — Disassembler 独立单元测试
 *
 * 编码已知 SASSWord → 验证反汇编文本与预期一致。
 * 覆盖全部指令类别。
 * ============================================================================ */

#include "test_common.h"
#include "sass/disassembler.h"
#include "sass/instruction.h"
#include "sass/operand_codec.h"

using namespace sass;

// ─── 辅助：构造 SASSWord ───
static SASSWord make_word(Opcode op, uint16_t op1 = 0, uint16_t op2 = 0, uint16_t op3 = 0) {
    SASSWord w = 0;
    w |= static_cast<uint64_t>(static_cast<uint16_t>(op)) << 56;
    w |= static_cast<uint64_t>(op1);
    w |= static_cast<uint64_t>(op2) << 16;
    w |= static_cast<uint64_t>(op3) << 32;
    return w;
}

// ═══════════════════════════════════════════════════════════════
// 1. 无操作数指令
// ═══════════════════════════════════════════════════════════════

TEST(disasm_exit) {
    auto w = make_word(Opcode::EXIT);
    ASSERT_TRUE(disassemble(w).find("exit") != std::string::npos);
    return true;
}
TEST(disasm_ret) {
    ASSERT_TRUE(disassemble(make_word(Opcode::RET)).find("ret") != std::string::npos);
    return true;
}
TEST(disasm_bar) {
    ASSERT_TRUE(disassemble(make_word(Opcode::BAR)).find("bar") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 2. 三操作数寄存器指令
// ═══════════════════════════════════════════════════════════════

TEST(disasm_ffma_regs) {
    auto w = make_word(Opcode::FFMA, encode_op_reg(0), encode_op_reg(1), encode_op_reg(2));
    std::string d = disassemble(w);
    ASSERT_TRUE(d.find("ffma") != std::string::npos);
    ASSERT_TRUE(d.find("R0") != std::string::npos);
    ASSERT_TRUE(d.find("R1") != std::string::npos);
    ASSERT_TRUE(d.find("R2") != std::string::npos);
    return true;
}

TEST(disasm_iadd_regs) {
    auto w = make_word(Opcode::IADD, encode_op_reg(5), encode_op_reg(10), encode_op_reg(15));
    std::string d = disassemble(w);
    ASSERT_TRUE(d.find("iadd") != std::string::npos);
    ASSERT_TRUE(d.find("R5") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 3. 双操作数指令
// ═══════════════════════════════════════════════════════════════

TEST(disasm_mov32i) {
    auto w = make_word(Opcode::MOV32I, encode_op_reg(0), encode_op_imm(42));
    std::string d = disassemble(w);
    ASSERT_TRUE(d.find("mov32i") != std::string::npos);
    ASSERT_TRUE(d.find("42") != std::string::npos);
    return true;
}

TEST(disasm_ldg_mem) {
    auto w = make_word(Opcode::LDG, encode_op_reg(1), encode_op_mem(2, 16));
    std::string d = disassemble(w);
    ASSERT_TRUE(d.find("ldg") != std::string::npos);
    ASSERT_TRUE(d.find("R1") != std::string::npos);
    ASSERT_TRUE(d.find("R2") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 4. 分支指令
// ═══════════════════════════════════════════════════════════════

TEST(disasm_bra_imm) {
    auto w = make_word(Opcode::BRA, encode_op_imm(16));
    std::string d = disassemble(w);
    ASSERT_TRUE(d.find("bra") != std::string::npos);
    ASSERT_TRUE(d.find("16") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 5. 控制流 + 同步
// ═══════════════════════════════════════════════════════════════

TEST(disasm_kill)  { ASSERT_TRUE(disassemble(make_word(Opcode::KILL)).find("kill") != std::string::npos); return true; }
TEST(disasm_yield) { ASSERT_TRUE(disassemble(make_word(Opcode::YIELD)).find("yield") != std::string::npos); return true; }
TEST(disasm_membar){ ASSERT_TRUE(disassemble(make_word(Opcode::MEMBAR)).find("membar") != std::string::npos); return true; }

// ═══════════════════════════════════════════════════════════════
// 6. 浮点指令
// ═══════════════════════════════════════════════════════════════

TEST(disasm_fadd) { ASSERT_TRUE(disassemble(make_word(Opcode::FADD, encode_op_reg(0), encode_op_reg(1))).find("fadd") != std::string::npos); return true; }
TEST(disasm_fmul) { ASSERT_TRUE(disassemble(make_word(Opcode::FMUL, encode_op_reg(0), encode_op_reg(1))).find("fmul") != std::string::npos); return true; }
TEST(disasm_frcp) { ASSERT_TRUE(disassemble(make_word(Opcode::FRCP, encode_op_reg(0), encode_op_reg(1))).find("frcp") != std::string::npos); return true; }

// ═══════════════════════════════════════════════════════════════
// 7. XMAD 族
// ═══════════════════════════════════════════════════════════════

TEST(disasm_xmad)     { ASSERT_TRUE(disassemble(make_word(Opcode::XMAD, encode_op_reg(0), encode_op_reg(1), encode_op_reg(2))).find("xmad") != std::string::npos); return true; }
TEST(disasm_xmad_mrg) { ASSERT_TRUE(disassemble(make_word(Opcode::XMAD_MRG)).find("xmad.mrg") != std::string::npos); return true; }
TEST(disasm_xmad_psl) { ASSERT_TRUE(disassemble(make_word(Opcode::XMAD_PSL)).find("xmad.psl") != std::string::npos); return true; }

// ═══════════════════════════════════════════════════════════════
// 8. VAVX3 + 三进制
// ═══════════════════════════════════════════════════════════════

TEST(disasm_vavx3_mma) {
    auto w = make_word(Opcode::VAVX3_MMA_512);
    ASSERT_TRUE(disassemble(w).find("vavx3") != std::string::npos);
    return true;
}
TEST(disasm_tmad) {
    ASSERT_TRUE(disassemble(make_word(Opcode::TMAD)).find("tmad") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 9. 批量反汇编
// ═══════════════════════════════════════════════════════════════

TEST(disasm_block_multiple) {
    std::vector<SASSWord> code = {
        make_word(Opcode::FFMA, encode_op_reg(0), encode_op_reg(1), encode_op_reg(2)),
        make_word(Opcode::EXIT),
    };
    auto lines = disassemble_block(code);
    ASSERT_EQ(lines.size(), 2);
    ASSERT_TRUE(lines[0].find("ffma") != std::string::npos);
    ASSERT_TRUE(lines[1].find("exit") != std::string::npos);
    return true;
}

TEST(disasm_unknown_opcode) {
    // 0xEE 不是有效 opcode → "unknown"
    SASSWord w = 0xEEULL << 56;
    ASSERT_TRUE(disassemble(w).find("unknown") != std::string::npos);
    return true;
}

int main() {
    return run_all_tests();
}
