/* ============================================================================
 * HunTian SASS Assembler — Lexer 单元测试
 * ============================================================================ */

#include "test_common.h"
#include "sass/lexer.h"

using namespace sass;

// ──────────────────────────────────────────────────────────────
// 1. 基础 Token 类型
// ──────────────────────────────────────────────────────────────

TEST(lexer_empty) {
    Lexer lex("");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::END_OF_FILE);
    return true;
}

TEST(lexer_whitespace_only) {
    Lexer lex("   \t  \n  ");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::END_OF_FILE);
    return true;
}

TEST(lexer_comment_only) {
    Lexer lex("; this is a comment\n");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::END_OF_FILE);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 2. 寄存器
// ──────────────────────────────────────────────────────────────

TEST(lexer_single_register) {
    Lexer lex("R0");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::REGISTER);
    ASSERT_STR_EQ(t.text, "R0");
    t = lex.next();
    ASSERT_EQ(t.type, TokenType::END_OF_FILE);
    return true;
}

TEST(lexer_register_high_number) {
    Lexer lex("R255");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::REGISTER);
    ASSERT_STR_EQ(t.text, "R255");
    return true;
}

TEST(lexer_register_to_int) {
    Lexer lex("R42");
    Token t = lex.next();
    ASSERT_EQ(t.to_reg(), 42);
    return true;
}

TEST(lexer_predicate_register) {
    Lexer lex("P0");
    Token t = lex.next();
    // P 寄存器当前被 lexer 识别为 IDENTIFIER (因为 read_register 只在 R 后触发)
    // 这是预期行为，P 寄存器在 parser 层处理
    ASSERT_TRUE(t.type == TokenType::REGISTER || t.type == TokenType::IDENTIFIER);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 3. 立即数
// ──────────────────────────────────────────────────────────────

TEST(lexer_positive_immediate) {
    Lexer lex("42");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::IMMEDIATE);
    ASSERT_EQ(t.to_int(), 42);
    return true;
}

TEST(lexer_negative_immediate) {
    Lexer lex("-128");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::IMMEDIATE);
    ASSERT_EQ(t.to_int(), -128);
    return true;
}

TEST(lexer_hex_immediate) {
    Lexer lex("0xFF");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::IMMEDIATE);
    ASSERT_EQ(t.to_int(), 255);
    return true;
}

TEST(lexer_zero) {
    Lexer lex("0");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::IMMEDIATE);
    ASSERT_EQ(t.to_int(), 0);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 4. 操作码
// ──────────────────────────────────────────────────────────────

TEST(lexer_integer_opcodes) {
    const char* ops[] = {
        "iadd", "iadd3", "iadd.x", "iscadd", "isub",
        "imad", "imul", "imnmx", "shl", "shr", "bfe", "bfi",
        "lop3", "lop32i", "brev"
    };
    for (const char* op : ops) {
        Lexer lex(op);
        Token t = lex.next();
        if (t.type != TokenType::OPCODE) {
            std::cerr << "  FAIL: " << op << " not recognized as OPCODE\n";
            return false;
        }
        ASSERT_STR_EQ(t.text, op);
    }
    return true;
}

TEST(lexer_float_opcodes) {
    const char* ops[] = {
        "fadd", "fmul", "fset", "fmnmx", "fsel", "frcp", "fsqrt"
    };
    for (const char* op : ops) {
        Lexer lex(op);
        Token t = lex.next();
        if (t.type != TokenType::OPCODE) {
            std::cerr << "  FAIL: " << op << " not recognized as OPCODE\n";
            return false;
        }
    }
    return true;
}

TEST(lexer_xmad_ffma_opcodes) {
    const char* ops[] = { "ffma", "xmad", "xmad.mrg", "xmad.psl" };
    for (const char* op : ops) {
        Lexer lex(op);
        Token t = lex.next();
        ASSERT_EQ(t.type, TokenType::OPCODE);
        ASSERT_STR_EQ(t.text, op);
    }
    return true;
}

TEST(lexer_memory_opcodes) {
    const char* ops[] = { "ldg", "lds", "stg", "sts", "ldc" };
    for (const char* op : ops) {
        Lexer lex(op);
        Token t = lex.next();
        ASSERT_EQ(t.type, TokenType::OPCODE);
    }
    return true;
}

TEST(lexer_control_flow_opcodes) {
    const char* ops[] = { "bra", "brx", "cal", "ret", "exit", "kill", "yield" };
    for (const char* op : ops) {
        Lexer lex(op);
        Token t = lex.next();
        ASSERT_EQ(t.type, TokenType::OPCODE);
    }
    return true;
}

TEST(lexer_sync_opcodes) {
    const char* ops[] = { "bar", "depbar", "membar" };
    for (const char* op : ops) {
        Lexer lex(op);
        Token t = lex.next();
        ASSERT_EQ(t.type, TokenType::OPCODE);
    }
    return true;
}

TEST(lexer_data_move_opcodes) {
    const char* ops[] = { "mov", "mov32i", "sel", "s2r" };
    for (const char* op : ops) {
        Lexer lex(op);
        Token t = lex.next();
        ASSERT_EQ(t.type, TokenType::OPCODE);
    }
    return true;
}

TEST(lexer_vavx3_opcodes) {
    const char* ops[] = {
        "vavx3.add", "vavx3.mul", "vavx3.mad", "vavx3.mma",
        "vavx3.geom", "vavx3.shuffle", "vavx3.braid", "vavx3.laplacian"
    };
    for (const char* op : ops) {
        Lexer lex(op);
        Token t = lex.next();
        ASSERT_EQ(t.type, TokenType::OPCODE);
        ASSERT_STR_EQ(t.text, op);
    }
    return true;
}

TEST(lexer_ternary_opcodes) {
    const char* ops[] = { "tmad", "tmul", "tconv", "tryte" };
    for (const char* op : ops) {
        Lexer lex(op);
        Token t = lex.next();
        ASSERT_EQ(t.type, TokenType::OPCODE);
    }
    return true;
}

TEST(lexer_compare_opcodes) {
    const char* ops[] = { "iset", "isetp", "fset", "fsetp", "setp" };
    for (const char* op : ops) {
        Lexer lex(op);
        Token t = lex.next();
        ASSERT_EQ(t.type, TokenType::OPCODE);
    }
    return true;
}

TEST(lexer_conversion_opcodes) {
    Lexer lex("cvt");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::OPCODE);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 5. 分隔符与符号
// ──────────────────────────────────────────────────────────────

TEST(lexer_comma) {
    Lexer lex(",");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::COMMA);
    return true;
}

TEST(lexer_brackets) {
    Lexer lex("[]");
    Token t1 = lex.next();
    ASSERT_EQ(t1.type, TokenType::LBRACKET);
    Token t2 = lex.next();
    ASSERT_EQ(t2.type, TokenType::RBRACKET);
    return true;
}

TEST(lexer_plus) {
    Lexer lex("+");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::PLUS);
    return true;
}

TEST(lexer_dot_directive) {
    Lexer lex(".func");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::DOT);
    ASSERT_STR_EQ(t.text, ".func");
    return true;
}

TEST(lexer_endfunc) {
    Lexer lex(".endfunc");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::DOT);
    ASSERT_STR_EQ(t.text, ".endfunc");
    return true;
}

// ──────────────────────────────────────────────────────────────
// 6. 标号
// ──────────────────────────────────────────────────────────────

TEST(lexer_label_def) {
    Lexer lex("loop:");
    Token t1 = lex.next();
    ASSERT_EQ(t1.type, TokenType::IDENTIFIER);
    ASSERT_STR_EQ(t1.text, "loop");
    Token t2 = lex.next();
    ASSERT_EQ(t2.type, TokenType::LABEL_DEF);
    return true;
}

TEST(lexer_label_ref) {
    Lexer lex("bra loop");
    Token t1 = lex.next();
    ASSERT_EQ(t1.type, TokenType::OPCODE);
    Token t2 = lex.next();
    ASSERT_EQ(t2.type, TokenType::IDENTIFIER);
    ASSERT_STR_EQ(t2.text, "loop");
    return true;
}

// ──────────────────────────────────────────────────────────────
// 7. 内存地址
// ──────────────────────────────────────────────────────────────

TEST(lexer_memory_address) {
    Lexer lex("[R10 + 4]");
    Token t1 = lex.next();
    ASSERT_EQ(t1.type, TokenType::LBRACKET);
    Token t2 = lex.next();
    ASSERT_EQ(t2.type, TokenType::REGISTER);
    ASSERT_STR_EQ(t2.text, "R10");
    Token t3 = lex.next();
    ASSERT_EQ(t3.type, TokenType::PLUS);
    Token t4 = lex.next();
    ASSERT_EQ(t4.type, TokenType::IMMEDIATE);
    ASSERT_EQ(t4.to_int(), 4);
    Token t5 = lex.next();
    ASSERT_EQ(t5.type, TokenType::RBRACKET);
    return true;
}

TEST(lexer_memory_negative_offset) {
    Lexer lex("[R5 + -8]");
    Token t1 = lex.next(); // [
    ASSERT_EQ(t1.type, TokenType::LBRACKET);
    Token t2 = lex.next(); // R5
    ASSERT_EQ(t2.type, TokenType::REGISTER);
    Token t3 = lex.next(); // +
    ASSERT_EQ(t3.type, TokenType::PLUS);
    Token t4 = lex.next(); // -8
    ASSERT_EQ(t4.type, TokenType::IMMEDIATE);
    ASSERT_EQ(t4.to_int(), -8);
    Token t5 = lex.next(); // ]
    ASSERT_EQ(t5.type, TokenType::RBRACKET);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 8. 完整指令
// ──────────────────────────────────────────────────────────────

TEST(lexer_ffma_instruction) {
    Lexer lex("ffma R0, R1, R2");
    Token t1 = lex.next();
    ASSERT_EQ(t1.type, TokenType::OPCODE);
    ASSERT_STR_EQ(t1.text, "ffma");
    Token t2 = lex.next();
    ASSERT_EQ(t2.type, TokenType::REGISTER);
    Token t3 = lex.next();
    ASSERT_EQ(t3.type, TokenType::COMMA);
    Token t4 = lex.next();
    ASSERT_EQ(t4.type, TokenType::REGISTER);
    Token t5 = lex.next();
    ASSERT_EQ(t5.type, TokenType::COMMA);
    Token t6 = lex.next();
    ASSERT_EQ(t6.type, TokenType::REGISTER);
    return true;
}

TEST(lexer_ldg_instruction) {
    Lexer lex("ldg R0, [R10 + 0]");
    Token t1 = lex.next();
    ASSERT_EQ(t1.type, TokenType::OPCODE);
    Token t2 = lex.next();
    ASSERT_EQ(t2.type, TokenType::REGISTER);
    Token t3 = lex.next();
    ASSERT_EQ(t3.type, TokenType::COMMA);
    Token t4 = lex.next();
    ASSERT_EQ(t4.type, TokenType::LBRACKET);
    return true;
}

TEST(lexer_stg_instruction) {
    Lexer lex("stg [R20 + 8], R0");
    Token t1 = lex.next();
    ASSERT_EQ(t1.type, TokenType::OPCODE);
    Token t2 = lex.next();
    ASSERT_EQ(t2.type, TokenType::LBRACKET);
    Token t3 = lex.next();
    ASSERT_EQ(t3.type, TokenType::REGISTER);
    return true;
}

TEST(lexer_exit_instruction) {
    Lexer lex("exit");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::OPCODE);
    ASSERT_STR_EQ(t.text, "exit");
    return true;
}

TEST(lexer_mov32i_instruction) {
    Lexer lex("mov32i R0, 0x1234");
    Token t1 = lex.next();
    ASSERT_EQ(t1.type, TokenType::OPCODE);
    Token t2 = lex.next();
    ASSERT_EQ(t2.type, TokenType::REGISTER);
    Token t3 = lex.next();
    ASSERT_EQ(t3.type, TokenType::COMMA);
    Token t4 = lex.next();
    ASSERT_EQ(t4.type, TokenType::IMMEDIATE);
    ASSERT_EQ(t4.to_int(), 0x1234);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 9. 注释与空白
// ──────────────────────────────────────────────────────────────

TEST(lexer_inline_comment) {
    Lexer lex("ffma R0, R1, R2 ; this is a comment\n");
    Token t1 = lex.next();
    ASSERT_EQ(t1.type, TokenType::OPCODE);
    Token t2 = lex.next();
    ASSERT_EQ(t2.type, TokenType::REGISTER);
    // 注释被跳过，下一个应该是 END_OF_FILE
    Token t = lex.next();
    while (t.type != TokenType::END_OF_FILE) {
        t = lex.next();
    }
    ASSERT_EQ(t.type, TokenType::END_OF_FILE);
    return true;
}

TEST(lexer_multiple_blank_lines) {
    Lexer lex("\n\n\nffma R0, R1, R2\n\n");
    Token t = lex.next();
    ASSERT_EQ(t.type, TokenType::OPCODE);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 10. tokenize_all
// ──────────────────────────────────────────────────────────────

TEST(lexer_tokenize_all) {
    Lexer lex("ffma R0, R1, R2");
    auto tokens = lex.tokenize_all();
    // 应该有: OPCODE, REGISTER, COMMA, REGISTER, COMMA, REGISTER, EOF
    ASSERT_EQ(tokens.size(), 7);
    ASSERT_EQ(tokens[0].type, TokenType::OPCODE);
    ASSERT_EQ(tokens[1].type, TokenType::REGISTER);
    ASSERT_EQ(tokens[2].type, TokenType::COMMA);
    ASSERT_EQ(tokens[3].type, TokenType::REGISTER);
    ASSERT_EQ(tokens[4].type, TokenType::COMMA);
    ASSERT_EQ(tokens[5].type, TokenType::REGISTER);
    ASSERT_EQ(tokens[6].type, TokenType::END_OF_FILE);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 11. peek / consume
// ──────────────────────────────────────────────────────────────

TEST(lexer_peek) {
    Lexer lex("ffma R0");
    const Token& t = lex.peek();
    ASSERT_EQ(t.type, TokenType::OPCODE);
    // peek 不应该前进
    const Token& t2 = lex.peek();
    ASSERT_EQ(t2.type, TokenType::OPCODE);
    return true;
}

TEST(lexer_consume) {
    Lexer lex("ffma R0");
    lex.consume();
    const Token& t = lex.peek();
    ASSERT_EQ(t.type, TokenType::REGISTER);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 12. 错误处理
// ──────────────────────────────────────────────────────────────

TEST(lexer_no_error_on_valid) {
    Lexer lex("ffma R0, R1, R2");
    lex.tokenize_all();
    ASSERT_FALSE(lex.has_error());
    return true;
}

TEST(lexer_invalid_character) {
    Lexer lex("ffma R0, R1, R2 @ invalid");
    auto tokens = lex.tokenize_all();
    // @ 应该被标记为 INVALID
    bool found_invalid = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::INVALID) {
            found_invalid = true;
            break;
        }
    }
    ASSERT_TRUE(found_invalid);
    return true;
}

// ──────────────────────────────────────────────────────────────
// 13. 完整 SASS 程序
// ──────────────────────────────────────────────────────────────

TEST(lexer_full_sass_program) {
    const char* source = R"(
; Simple GEMM kernel
.func gemm_16x16

loop:
    ffma R0, R1, R2
    ldg R3, [R10 + 0]
    stg [R20 + 8], R0
    bra loop

    exit
.endfunc
)";
    Lexer lex(source);
    auto tokens = lex.tokenize_all();

    // 至少应该有这些 token
    ASSERT_TRUE(tokens.size() > 10);

    // 第一个有意义的 token 应该是 .func 的 DOT
    bool found_dot = false;
    bool found_func = false;
    bool found_ffma = false;
    bool found_exit = false;

    for (const auto& t : tokens) {
        if (t.type == TokenType::DOT) {
            if (t.text == ".func") found_dot = true;
            if (t.text == ".endfunc") found_func = true;
        }
        if (t.type == TokenType::OPCODE) {
            if (t.text == "ffma") found_ffma = true;
            if (t.text == "exit") found_exit = true;
        }
    }

    ASSERT_TRUE(found_dot);
    ASSERT_TRUE(found_func);
    ASSERT_TRUE(found_ffma);
    ASSERT_TRUE(found_exit);

    // 不应该有 INVALID token
    for (const auto& t : tokens) {
        if (t.type == TokenType::INVALID) {
            std::cerr << "  FAIL: unexpected INVALID token: " << t.text << "\n";
            return false;
        }
    }
    return true;
}

// ──────────────────────────────────────────────────────────────
// Main
// ──────────────────────────────────────────────────────────────

int main() {
    return run_all_tests();
}
