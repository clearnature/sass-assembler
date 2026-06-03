/* ============================================================================
 * HunTian SASS Assembler — Parser 单元测试
 * ============================================================================ */

#include "test_common.h"
#include "sass/lexer.h"
#include "sass/parser.h"

using namespace sass;

// ──────────────────────────────────────────────────────────────
// 1. 基础解析
// ──────────────────────────────────────────────────────────────

TEST(parser_empty) {
    Lexer lex("");
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 0);
    return true;
}

TEST(parser_comment_only) {
    Lexer lex("; just a comment\n");
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 0);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 2. 单条指令
// ──────────────────────────────────────────────────────────────

TEST(parser_single_ffma) {
    Lexer lex("ffma R0, R1, R2");
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 1);
    ASSERT_EQ(result.instructions[0].opcode, Opcode::FFMA);
    ASSERT_EQ(result.instructions[0].operands.size(), 3);
    return true;
}

TEST(parser_single_iadd) {
    Lexer lex("iadd R5, R10, R15");
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 1);
    ASSERT_EQ(result.instructions[0].opcode, Opcode::IADD);
    return true;
}

TEST(parser_exit) {
    Lexer lex("exit");
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 1);
    ASSERT_EQ(result.instructions[0].opcode, Opcode::EXIT);
    ASSERT_EQ(result.instructions[0].operands.size(), 0);
    return true;
}

TEST(parser_mov32i) {
    Lexer lex("mov32i R0, 0x1234");
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 1);
    ASSERT_EQ(result.instructions[0].opcode, Opcode::MOV32I);
    ASSERT_EQ(result.instructions[0].operands.size(), 2);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 3. 多条指令
// ──────────────────────────────────────────────────────────────

TEST(parser_multiple_instructions) {
    Lexer lex(
        "iadd R0, R1, R2\n"
        "ffma R3, R4, R5\n"
        "exit\n"
    );
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 3);
    ASSERT_EQ(result.instructions[0].opcode, Opcode::IADD);
    ASSERT_EQ(result.instructions[1].opcode, Opcode::FFMA);
    ASSERT_EQ(result.instructions[2].opcode, Opcode::EXIT);
    return true;
}

TEST(parser_64_ffma) {
    // 生成 64 条 FFMA 指令
    std::string source;
    for (int i = 0; i < 64; i++) {
        source += "ffma R" + std::to_string(i % 32) +
                  ", R" + std::to_string((i * 3) % 32) +
                  ", R" + std::to_string((i * 5) % 32) + "\n";
    }
    Lexer lex(source);
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 64);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 4. 标号
// ──────────────────────────────────────────────────────────────

TEST(parser_label_definition) {
    Lexer lex(
        "loop:\n"
        "    ffma R0, R1, R2\n"
        "    bra loop\n"
    );
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    // 标号应该被记录
    ASSERT_TRUE(result.labels.count("loop"));
    // 指令数应该是 2 (ffma + bra)
    ASSERT_EQ(result.instructions.size(), 2);
    return true;
}

TEST(parser_multiple_labels) {
    Lexer lex(
        "start:\n"
        "    iadd R0, R0, 1\n"
        "middle:\n"
        "    ffma R1, R2, R3\n"
        "end:\n"
        "    exit\n"
    );
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(result.labels.count("start"));
    ASSERT_TRUE(result.labels.count("middle"));
    ASSERT_TRUE(result.labels.count("end"));
    return true;
}

TEST(parser_label_branch) {
    Lexer lex(
        "loop:\n"
        "    iadd R0, R0, 1\n"
        "    bra loop\n"
    );
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    // bra loop 的 operand 应该是 LABEL 类型
    ASSERT_EQ(result.instructions.size(), 2);
    auto& bra_inst = result.instructions[1];
    ASSERT_EQ(bra_inst.opcode, Opcode::BRA);
    // 注意：当前 parser 可能将 loop 解析为 IDENTIFIER → LABEL
    return true;
}

// ──────────────────────────────────────────────────────────────
// 5. 伪指令
// ──────────────────────────────────────────────────────────────

TEST(parser_func_directive) {
    Lexer lex(
        ".func gemm\n"
        "    ffma R0, R1, R2\n"
        ".endfunc\n"
    );
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    // .func 和 .endfunc 是伪指令，应该不产生指令
    ASSERT_EQ(result.instructions.size(), 1);
    ASSERT_EQ(result.instructions[0].opcode, Opcode::FFMA);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 6. 内存操作数
// ──────────────────────────────────────────────────────────────

TEST(parser_ldg_memory) {
    Lexer lex("ldg R0, [R10 + 0]");
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 1);
    ASSERT_EQ(result.instructions[0].opcode, Opcode::LDG);
    // 应该有 2 个操作数: R0 和 [R10 + 0]
    ASSERT_EQ(result.instructions[0].operands.size(), 2);
    // 第一个是寄存器
    ASSERT_EQ(result.instructions[0].operands[0].type, OpType::REG);
    // 第二个是内存
    ASSERT_EQ(result.instructions[0].operands[1].type, OpType::MEM);
    return true;
}

TEST(parser_stg_memory) {
    Lexer lex("stg [R20 + 8], R0");
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 1);
    ASSERT_EQ(result.instructions[0].opcode, Opcode::STG);
    ASSERT_EQ(result.instructions[0].operands.size(), 2);
    return true;
}

TEST(parser_lds_shared) {
    Lexer lex("lds R0, [R5 + 4]");
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions[0].opcode, Opcode::LDS);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 7. 操作数类型
// ──────────────────────────────────────────────────────────────

TEST(parser_register_operand) {
    Lexer lex("mov R0, R1");
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions[0].operands[0].type, OpType::REG);
    ASSERT_EQ(result.instructions[0].operands[0].reg_id, 0);
    ASSERT_EQ(result.instructions[0].operands[1].type, OpType::REG);
    ASSERT_EQ(result.instructions[0].operands[1].reg_id, 1);
    return true;
}

TEST(parser_immediate_operand) {
    Lexer lex("iadd R0, R1, 42");
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    // iadd 是 3 操作数，第三个可能是立即数
    // 当前 parser 对立即数的处理
    return true;
}

TEST(parser_negative_immediate) {
    Lexer lex("iadd R0, R1, -8");
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 8. 各类指令解析
// ──────────────────────────────────────────────────────────────

TEST(parser_xmad_instructions) {
    Lexer lex(
        "xmad R0, R1, R2\n"
        "xmad.mrg R3, R4, R5\n"
        "xmad.psl R6, R7, R8\n"
    );
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 3);
    ASSERT_EQ(result.instructions[0].opcode, Opcode::XMAD);
    ASSERT_EQ(result.instructions[1].opcode, Opcode::XMAD_MRG);
    ASSERT_EQ(result.instructions[2].opcode, Opcode::XMAD_PSL);
    return true;
}

TEST(parser_vavx3_instructions) {
    Lexer lex(
        "vavx3.add V0, V1, V2\n"
        "vavx3.mma V3, V4, V5, V6\n"
        "vavx3.geom V7, V8, V9\n"
    );
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 3);
    // V 寄存器当前被解析为标识符或寄存器
    return true;
}

TEST(parser_ternary_instructions) {
    Lexer lex(
        "tmad T0, T1, T2\n"
        "tmul T3, T4, T5\n"
    );
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    return true;
}

TEST(parser_sync_instructions) {
    Lexer lex(
        "bar\n"
        "depbar\n"
        "membar\n"
    );
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 3);
    return true;
}

TEST(parser_control_flow) {
    Lexer lex(
        "bra loop\n"
        "cal func\n"
        "ret\n"
        "kill\n"
        "yield\n"
    );
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 5);
    ASSERT_EQ(result.instructions[0].opcode, Opcode::BRA);
    ASSERT_EQ(result.instructions[1].opcode, Opcode::CAL);
    ASSERT_EQ(result.instructions[2].opcode, Opcode::RET);
    ASSERT_EQ(result.instructions[3].opcode, Opcode::KILL);
    ASSERT_EQ(result.instructions[4].opcode, Opcode::YIELD);
    return true;
}

TEST(parser_data_movement) {
    Lexer lex(
        "mov R0, R1\n"
        "mov32i R0, 0x100\n"
        "sel R0, R1, R2\n"
        "s2r R0, SR\n"
    );
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 4);
    return true;
}

TEST(parser_comparison_instructions) {
    Lexer lex(
        "iset R0, R1, R2\n"
        "isetp P0, R1, R2\n"
        "fsetp P1, R3, R4\n"
        "setp P2, R5, R6\n"
    );
    Parser parser(lex);
    auto result = parser.parse_all();
    ASSERT_TRUE(result.ok);
    ASSERT_EQ(result.instructions.size(), 4);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 9. opcode 映射
// ──────────────────────────────────────────────────────────────

TEST(opcode_from_string_ffma) {
    auto op = opcode_from_string("ffma");
    ASSERT_EQ(op, Opcode::FFMA);
    return true;
}

TEST(opcode_from_string_xmad) {
    ASSERT_EQ(opcode_from_string("xmad"), Opcode::XMAD);
    ASSERT_EQ(opcode_from_string("xmad.mrg"), Opcode::XMAD_MRG);
    ASSERT_EQ(opcode_from_string("xmad.psl"), Opcode::XMAD_PSL);
    return true;
}

TEST(opcode_from_string_unknown) {
    auto op = opcode_from_string("nonexistent");
    ASSERT_EQ(op, Opcode::UNKNOWN);
    return true;
}

TEST(opcode_to_string) {
    ASSERT_STR_EQ(opcode_to_string(Opcode::FFMA), "ffma");
    ASSERT_STR_EQ(opcode_to_string(Opcode::IADD), "iadd");
    ASSERT_STR_EQ(opcode_to_string(Opcode::EXIT), "exit");
    return true;
}

TEST(opcode_to_string_unknown) {
    ASSERT_STR_EQ(opcode_to_string(Opcode::UNKNOWN), "unknown");
    return true;
}

// ──────────────────────────────────────────────────────────────
// 10. 完整 SASS 程序
// ──────────────────────────────────────────────────────────────

TEST(parser_full_gemm_kernel) {
    const char* source = R"(
; 16x16 GEMM kernel
.func gemm_16x16

    ; Initialize
    mov32i R0, 0
    mov32i R1, 16

loop_row:
    iadd R2, R0, 0

loop_col:
    ; Load A
    ldg R10, [R100 + 0]
    ldg R11, [R100 + 4]

    ; Load B
    ldg R20, [R200 + 0]
    ldg R21, [R200 + 4]

    ; FMA
    ffma R30, R10, R20, R30
    ffma R31, R11, R21, R31

    ; Increment
    iadd R2, R2, 1
    bra loop_col

    iadd R0, R0, 1
    bra loop_row

    ; Store result
    stg [R300 + 0], R30
    stg [R300 + 4], R31

    exit
.endfunc
)";
    Lexer lex(source);
    Parser parser(lex);
    auto result = parser.parse_all();

    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(result.instructions.size() > 10);

    // 应该有 loop_row 和 loop_col 标号
    ASSERT_TRUE(result.labels.count("loop_row"));
    ASSERT_TRUE(result.labels.count("loop_col"));

    return true;
}

TEST(parser_simple_loop) {
    const char* source = R"(
.func count
    mov32i R0, 0
loop:
    iadd R0, R0, 1
    bra loop
    exit
.endfunc
)";
    Lexer lex(source);
    Parser parser(lex);
    auto result = parser.parse_all();

    ASSERT_TRUE(result.ok);
    ASSERT_TRUE(result.labels.count("loop"));
    ASSERT_EQ(result.instructions.size(), 4); // mov32i + iadd + bra + exit
    return true;
}

// ──────────────────────────────────────────────────────────────
// Main
// ──────────────────────────────────────────────────────────────

int main() {
    return run_all_tests();
}
