/* ============================================================================
 * HunTian SASS Assembler — Assembler Facade 集成测试 (红灯)
 *
 * 端到端验证：.sass 文本 → assemble() → 反汇编 → 文本验证。
 * ============================================================================ */

#include "test_common.h"
#include "sass/assembler.h"
#include "sass/disassembler.h"

using namespace sass;

// ═══════════════════════════════════════════════════════════════
// 1. 基本流水线
// ═══════════════════════════════════════════════════════════════

TEST(assembler_empty_input) {
    Assembler asm_;
    auto code = asm_.assemble("");
    ASSERT_EQ(code.size(), 0);
    return true;
}

TEST(assembler_single_ffma) {
    Assembler asm_;
    auto code = asm_.assemble("ffma R0, R1, R2;");
    ASSERT_EQ(code.size(), 1);
    std::string dis = disassemble(code[0]);
    ASSERT_TRUE(dis.find("ffma") != std::string::npos);
    ASSERT_TRUE(dis.find("R0") != std::string::npos);
    return true;
}

TEST(assembler_single_exit) {
    Assembler asm_;
    auto code = asm_.assemble("exit;");
    ASSERT_EQ(code.size(), 1);
    std::string dis = disassemble(code[0]);
    ASSERT_TRUE(dis.find("exit") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 2. 多条指令
// ═══════════════════════════════════════════════════════════════

TEST(assembler_multiple_instructions) {
    Assembler asm_;
    std::string src =
        "ffma R0, R1, R2;\n"
        "iadd R3, R4, R5;\n"
        "fmul R6, R7, R8;\n";
    auto code = asm_.assemble(src);
    ASSERT_EQ(code.size(), 3);

    auto lines = disassemble_block(code);
    ASSERT_EQ(lines.size(), 3);
    ASSERT_TRUE(lines[0].find("ffma") != std::string::npos);
    ASSERT_TRUE(lines[1].find("iadd") != std::string::npos);
    ASSERT_TRUE(lines[2].find("fmul") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 3. 往返：汇编 → 反汇编 → 文本一致性
// ═══════════════════════════════════════════════════════════════

TEST(assembler_roundtrip_ffma) {
    Assembler asm_;
    std::string src = "ffma R7, R8, R9;";
    auto code = asm_.assemble(src);
    ASSERT_EQ(code.size(), 1);

    // 反汇编应包含相同的操作码和寄存器
    std::string dis = disassemble(code[0]);
    ASSERT_TRUE(dis.find("ffma") != std::string::npos);
    ASSERT_TRUE(dis.find("R7") != std::string::npos);
    ASSERT_TRUE(dis.find("R8") != std::string::npos);
    ASSERT_TRUE(dis.find("R9") != std::string::npos);
    return true;
}

TEST(assembler_roundtrip_mov32i) {
    Assembler asm_;
    std::string src = "mov32i R0, 42;";
    auto code = asm_.assemble(src);
    ASSERT_EQ(code.size(), 1);

    std::string dis = disassemble(code[0]);
    ASSERT_TRUE(dis.find("mov32i") != std::string::npos);
    ASSERT_TRUE(dis.find("R0") != std::string::npos);
    ASSERT_TRUE(dis.find("42") != std::string::npos);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 4. 确定性
// ═══════════════════════════════════════════════════════════════

TEST(assembler_deterministic) {
    std::string src = "ffma R0, R1, R2;\niadd R3, R4, R5;\n";

    Assembler asm1;
    auto code1 = asm1.assemble(src);

    Assembler asm2;
    auto code2 = asm2.assemble(src);

    ASSERT_EQ(code1.size(), code2.size());
    for (size_t i = 0; i < code1.size(); ++i)
        ASSERT_EQ(code1[i], code2[i]);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 5. 语法错误
// ═══════════════════════════════════════════════════════════════

TEST(assembler_invalid_input_returns_empty) {
    Assembler asm_;
    auto code = asm_.assemble("garbage_not_an_opcode;");
    ASSERT_EQ(code.size(), 0);
    return true;
}

int main() {
    return run_all_tests();
}
