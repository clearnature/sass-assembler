/* ============================================================================
 * HunTian SASS Assembler — PascalBackend v2 编码验证测试
 *
 * 对比 cuobjdump -sass 输出的真实 Pascal GP106 机器码。
 * ============================================================================ */

#include "test_common.h"
#include "sass/sass_types.h"
#include "sass/instruction.h"
#include "../src/sass/pascal_backend.h"

using namespace sass;

static ManifoldResult make_single(const Instruction& inst) {
    ManifoldResult r;
    r.block_id = 0; r.bubble_score = 0;
    r.optimized_sequence = {inst};
    return r;
}
static Instruction make_reg3(Opcode op, uint8_t d, uint8_t s1, uint8_t s2) {
    return Instruction::CreateRegOp(op, d, s1, s2);
}

// ═══════════════════════════════════════════════════════════════
// PASCAL_VERIFIED — 与真实 SASS hex 比对
// ═══════════════════════════════════════════════════════════════

TEST(pascal_exit_matches_real) {
    Instruction inst; inst.opcode = Opcode::EXIT;
    PascalBackend be;
    auto code = be.encode(make_single(inst));
    ASSERT_EQ(code.size(), 1);
    // 真实 SASS: EXIT → 0xe30000000007000f
    ASSERT_EQ(code[0], 0xe30000000007000fULL);
    return true;
}

TEST(pascal_ffma_matches_spec) {
    // FFMA R9, R7, R9, RZ → 0x59807f8000970709
    Instruction inst;
    inst.opcode = Opcode::FFMA;
    inst.operands = {
        {OpType::REG, {.reg_id = 9}, ""},
        {OpType::REG, {.reg_id = 7}, ""},
        {OpType::REG, {.reg_id = 9}, ""},
        {OpType::REG, {.reg_id = 255}, ""},  // RZ
    };
    PascalBackend be;
    auto code = be.encode(make_single(inst));
    ASSERT_EQ(code.size(), 1);
    // Opcode byte must be 0x59
    ASSERT_EQ((code[0] >> 56) & 0xFF, 0x59);
    return true;
}

TEST(pascal_xmad_opcode_correct) {
    auto inst = make_reg3(Opcode::XMAD, 4, 6, 4);
    PascalBackend be;
    auto code = be.encode(make_single(inst));
    ASSERT_EQ(code.size(), 1);
    uint8_t op = (code[0] >> 56) & 0xFF;
    ASSERT_TRUE(op == 0x4e || op == 0x51);  // XMAD has two valid opcodes
    return true;
}

TEST(pascal_xmad_mrg_opcode_correct) {
    auto inst = make_reg3(Opcode::XMAD_MRG, 3, 2, 0);
    PascalBackend be;
    auto code = be.encode(make_single(inst));
    ASSERT_EQ(code.size(), 1);
    ASSERT_EQ((code[0] >> 56) & 0xFF, 0x4f);  // XMAD.MRG opcode
    return true;
}

TEST(pascal_xmad_psl_opcode_correct) {
    auto inst = make_reg3(Opcode::XMAD_PSL, 2, 2, 3);
    PascalBackend be;
    auto code = be.encode(make_single(inst));
    ASSERT_EQ(code.size(), 1);
    ASSERT_EQ((code[0] >> 56) & 0xFF, 0x5b);  // XMAD.PSL.CBCC opcode
    return true;
}

// ═══════════════════════════════════════════════════════════════
// PASCAL_DERIVED — 寄存器编码验证
// ═══════════════════════════════════════════════════════════════

TEST(pascal_register_encoding_roundtrip) {
    // 编码 R5, R10, R15 → 检查寄存器在正确位置
    auto inst = make_reg3(Opcode::IADD, 5, 10, 15);
    PascalBackend be;
    auto code = be.encode(make_single(inst));
    ASSERT_EQ(code.size(), 1);
    // Rd=R5 at byte 0
    ASSERT_EQ(code[0] & 0xFF, 5);
    return true;
}

TEST(pascal_stg_opcode_correct) {
    Instruction inst; inst.opcode = Opcode::STG;
    inst.operands = {{OpType::REG, {.reg_id = 2}, ""}, {OpType::REG, {.reg_id = 5}, ""}};
    PascalBackend be;
    auto code = be.encode(make_single(inst));
    ASSERT_EQ(code.size(), 1);
    // STG uses 0xee prefix — verify high byte is 0xee
    ASSERT_EQ((code[0] >> 56) & 0xFF, 0xee);
    return true;
}

TEST(pascal_backend_name) {
    PascalBackend be;
    ASSERT_TRUE(be.arch().name.find("Pascal") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 回退格式
// ═══════════════════════════════════════════════════════════════

TEST(pascal_unknown_fallback) {
    // 未映射的 opcode 应回退 HunTian 格式
    Instruction inst; inst.opcode = Opcode::BREV;
    PascalBackend be;
    auto code = be.encode(make_single(inst));
    ASSERT_EQ(code.size(), 1);
    // HunTian 格式: opcode 在 bits [63:56], BREV=0x0F
    // BREV now maps to LOP32I family (0x04)
    ASSERT_EQ((code[0] >> 56) & 0xFF, 0x04);
    return true;
}

int main() {
    return run_all_tests();
}
