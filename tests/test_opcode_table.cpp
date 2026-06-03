/* ============================================================================
 * HunTian SASS Assembler — OpcodeTable 单元测试 (红灯)
 *
 * 验证所有 55 条指令的 opcode→名称映射和操作数个数。
 * ============================================================================ */

#include "test_common.h"
#include "sass/opcode_table.h"
#include "sass/instruction.h"

using namespace sass;

// ═══════════════════════════════════════════════════════════════
// 1. 整数算术指令 (15 条)
// ═══════════════════════════════════════════════════════════════

TEST(opcode_name_integer) {
    ASSERT_STR_EQ(opcode_name(Opcode::IADD),    "iadd");
    ASSERT_STR_EQ(opcode_name(Opcode::IADD3),   "iadd3");
    ASSERT_STR_EQ(opcode_name(Opcode::IADD_X),  "iadd.x");
    ASSERT_STR_EQ(opcode_name(Opcode::ISCADD),  "iscadd");
    ASSERT_STR_EQ(opcode_name(Opcode::ISUB),    "isub");
    ASSERT_STR_EQ(opcode_name(Opcode::IMAD),    "imad");
    ASSERT_STR_EQ(opcode_name(Opcode::IMUL),    "imul");
    ASSERT_STR_EQ(opcode_name(Opcode::IMNMX),   "imnmx");
    ASSERT_STR_EQ(opcode_name(Opcode::SHL),     "shl");
    ASSERT_STR_EQ(opcode_name(Opcode::SHR),     "shr");
    ASSERT_STR_EQ(opcode_name(Opcode::BFE),     "bfe");
    ASSERT_STR_EQ(opcode_name(Opcode::BFI),     "bfi");
    ASSERT_STR_EQ(opcode_name(Opcode::LOP3),    "lop3");
    ASSERT_STR_EQ(opcode_name(Opcode::LOP32I),  "lop32i");
    ASSERT_STR_EQ(opcode_name(Opcode::BREV),    "brev");
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 2. 浮点指令 (7 条)
// ═══════════════════════════════════════════════════════════════

TEST(opcode_name_float) {
    ASSERT_STR_EQ(opcode_name(Opcode::FADD),   "fadd");
    ASSERT_STR_EQ(opcode_name(Opcode::FMUL),   "fmul");
    ASSERT_STR_EQ(opcode_name(Opcode::FSET),   "fset");
    ASSERT_STR_EQ(opcode_name(Opcode::FMNMX),  "fmnmx");
    ASSERT_STR_EQ(opcode_name(Opcode::FSEL),   "fsel");
    ASSERT_STR_EQ(opcode_name(Opcode::FRCP),   "frcp");
    ASSERT_STR_EQ(opcode_name(Opcode::FSQRT),  "fsqrt");
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 3. XMAD + FFMA 指令 (4 条)
// ═══════════════════════════════════════════════════════════════

TEST(opcode_name_xmad_ffma) {
    ASSERT_STR_EQ(opcode_name(Opcode::FFMA),     "ffma");
    ASSERT_STR_EQ(opcode_name(Opcode::XMAD),     "xmad");
    ASSERT_STR_EQ(opcode_name(Opcode::XMAD_MRG), "xmad.mrg");
    ASSERT_STR_EQ(opcode_name(Opcode::XMAD_PSL), "xmad.psl");
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 4. 内存指令 (5 条)
// ═══════════════════════════════════════════════════════════════

TEST(opcode_name_memory) {
    ASSERT_STR_EQ(opcode_name(Opcode::LDG),  "ldg");
    ASSERT_STR_EQ(opcode_name(Opcode::LDS),  "lds");
    ASSERT_STR_EQ(opcode_name(Opcode::STG),  "stg");
    ASSERT_STR_EQ(opcode_name(Opcode::STS),  "sts");
    ASSERT_STR_EQ(opcode_name(Opcode::LDC),  "ldc");
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 5. 控制流指令 (7 条)
// ═══════════════════════════════════════════════════════════════

TEST(opcode_name_control_flow) {
    ASSERT_STR_EQ(opcode_name(Opcode::BRA),   "bra");
    ASSERT_STR_EQ(opcode_name(Opcode::BRX),   "brx");
    ASSERT_STR_EQ(opcode_name(Opcode::CAL),   "cal");
    ASSERT_STR_EQ(opcode_name(Opcode::RET),   "ret");
    ASSERT_STR_EQ(opcode_name(Opcode::EXIT),  "exit");
    ASSERT_STR_EQ(opcode_name(Opcode::KILL),  "kill");
    ASSERT_STR_EQ(opcode_name(Opcode::YIELD), "yield");
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 6. 同步/屏障 (3 条)
// ═══════════════════════════════════════════════════════════════

TEST(opcode_name_sync) {
    ASSERT_STR_EQ(opcode_name(Opcode::BAR),    "bar");
    ASSERT_STR_EQ(opcode_name(Opcode::DEPBAR), "depbar");
    ASSERT_STR_EQ(opcode_name(Opcode::MEMBAR), "membar");
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 7. 数据移动 + 转换 + 比较 (10 条)
// ═══════════════════════════════════════════════════════════════

TEST(opcode_name_data_move) {
    ASSERT_STR_EQ(opcode_name(Opcode::MOV),    "mov");
    ASSERT_STR_EQ(opcode_name(Opcode::MOV32I), "mov32i");
    ASSERT_STR_EQ(opcode_name(Opcode::SEL),    "sel");
    ASSERT_STR_EQ(opcode_name(Opcode::S2R),    "s2r");
    ASSERT_STR_EQ(opcode_name(Opcode::CVT),    "cvt");
    ASSERT_STR_EQ(opcode_name(Opcode::ISET),   "iset");
    ASSERT_STR_EQ(opcode_name(Opcode::ISETP),  "isetp");
    ASSERT_STR_EQ(opcode_name(Opcode::FSETP),  "fsetp");
    ASSERT_STR_EQ(opcode_name(Opcode::SETP),   "setp");
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 8. VAVX3 指令 (8 条)
// ═══════════════════════════════════════════════════════════════

TEST(opcode_name_vavx3) {
    ASSERT_STR_EQ(opcode_name(Opcode::VAVX3_ADD_512),   "vavx3.add");
    ASSERT_STR_EQ(opcode_name(Opcode::VAVX3_MUL_512),   "vavx3.mul");
    ASSERT_STR_EQ(opcode_name(Opcode::VAVX3_MAD_512),   "vavx3.mad");
    ASSERT_STR_EQ(opcode_name(Opcode::VAVX3_MMA_512),   "vavx3.mma");
    ASSERT_STR_EQ(opcode_name(Opcode::VAVX3_GEOM),      "vavx3.geom");
    ASSERT_STR_EQ(opcode_name(Opcode::VAVX3_SHUFFLE),   "vavx3.shuffle");
    ASSERT_STR_EQ(opcode_name(Opcode::VAVX3_BRAID),     "vavx3.braid");
    ASSERT_STR_EQ(opcode_name(Opcode::VAVX3_LAPLACIAN), "vavx3.laplacian");
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 9. 三进制指令 (4 条)
// ═══════════════════════════════════════════════════════════════

TEST(opcode_name_ternary) {
    ASSERT_STR_EQ(opcode_name(Opcode::TMAD),    "tmad");
    ASSERT_STR_EQ(opcode_name(Opcode::TMUL),    "tmul");
    ASSERT_STR_EQ(opcode_name(Opcode::TCONV),   "tconv");
    ASSERT_STR_EQ(opcode_name(Opcode::TRYTE_OP), "tryte");
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 10. 未知 opcode
// ═══════════════════════════════════════════════════════════════

TEST(opcode_name_unknown) {
    ASSERT_STR_EQ(opcode_name(Opcode::UNKNOWN), "unknown");
    ASSERT_STR_EQ(opcode_name(static_cast<Opcode>(0xEE)), "unknown");
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 11. 操作数个数验证
// ═══════════════════════════════════════════════════════════════

TEST(operand_count_zero) {
    // 无操作数指令
    ASSERT_EQ(operand_count(Opcode::EXIT), 0);
    ASSERT_EQ(operand_count(Opcode::RET), 0);
    ASSERT_EQ(operand_count(Opcode::KILL), 0);
    ASSERT_EQ(operand_count(Opcode::YIELD), 0);
    ASSERT_EQ(operand_count(Opcode::BAR), 0);
    ASSERT_EQ(operand_count(Opcode::DEPBAR), 0);
    ASSERT_EQ(operand_count(Opcode::MEMBAR), 0);
    return true;
}

TEST(operand_count_one) {
    // 单操作数 (分支目标)
    ASSERT_EQ(operand_count(Opcode::BRA), 1);
    ASSERT_EQ(operand_count(Opcode::BRX), 1);
    ASSERT_EQ(operand_count(Opcode::CAL), 1);
    return true;
}

TEST(operand_count_two) {
    // 双操作数指令
    ASSERT_EQ(operand_count(Opcode::MOV), 2);
    ASSERT_EQ(operand_count(Opcode::MOV32I), 2);
    ASSERT_EQ(operand_count(Opcode::LDG), 2);
    ASSERT_EQ(operand_count(Opcode::STG), 2);
    ASSERT_EQ(operand_count(Opcode::ISUB), 2);
    ASSERT_EQ(operand_count(Opcode::FADD), 2);
    return true;
}

TEST(operand_count_three) {
    // 三操作数指令 (默认)
    ASSERT_EQ(operand_count(Opcode::FFMA), 3);
    ASSERT_EQ(operand_count(Opcode::IADD), 3);
    ASSERT_EQ(operand_count(Opcode::LOP3), 3);
    ASSERT_EQ(operand_count(Opcode::VAVX3_MMA_512), 3);
    ASSERT_EQ(operand_count(Opcode::TMAD), 3);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 12. 操作数字符串化 (依赖 OperandCodec)
// ═══════════════════════════════════════════════════════════════

TEST(operand_str_register) {
    // R5 → encode_op_reg(5) → operand_str → "R5"
    uint16_t enc = encode_op_reg(5);
    std::string s = operand_str(enc, 0, Opcode::FFMA);
    ASSERT_STR_EQ(s, "R5");
    return true;
}

TEST(operand_str_immediate) {
    uint16_t enc = encode_op_imm(42);
    std::string s = operand_str(enc, 0, Opcode::MOV32I);
    ASSERT_STR_EQ(s, "42");
    return true;
}

TEST(operand_str_immediate_negative) {
    uint16_t enc = encode_op_imm(-10);
    std::string s = operand_str(enc, 0, Opcode::MOV32I);
    ASSERT_STR_EQ(s, "-10");
    return true;
}

TEST(operand_str_memory_positive) {
    uint16_t enc = encode_op_mem(3, 16);
    std::string s = operand_str(enc, 0, Opcode::LDG);
    ASSERT_STR_EQ(s, "[R3 + 16]");
    return true;
}

TEST(operand_str_memory_negative) {
    uint16_t enc = encode_op_mem(2, -8);
    std::string s = operand_str(enc, 0, Opcode::LDG);
    ASSERT_STR_EQ(s, "[R2 - 8]");
    return true;
}

int main() {
    return run_all_tests();
}
