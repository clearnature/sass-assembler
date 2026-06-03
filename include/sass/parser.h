/* ============================================================================
 * HunTian SASS 汇编器 — 语法分析器
 * ============================================================================ */

#ifndef SASS_PARSER_H
#define SASS_PARSER_H

#include "lexer.h"
#include "instruction.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

namespace sass {

// ─── 操作码映射 ───
Opcode opcode_from_string(const std::string& s);
const char* opcode_to_string(Opcode op);

// ─── 解析结果 ───
struct ParseResult {
    std::vector<Instruction> instructions;
    std::unordered_map<std::string, uint32_t> labels;  // 标号名 → 指令序号
    std::string function_name;
    std::string error;
    bool ok = true;
};

// ─── Parser ───
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Parser(Lexer& lexer);

    // 主入口
    ParseResult parse_all();

private:
    std::vector<Token> tokens_;
    size_t pos_;
    std::unordered_map<std::string, uint32_t> symbol_table_;

    // 操作
    const Token& current() const;
    const Token& peek_next() const;
    void advance();
    bool match(TokenType type);
    bool consume_expected(TokenType type, const std::string& msg);

    // 解析器件
    void parse_directive(std::vector<Instruction>& out);
    bool parse_instruction(std::vector<Instruction>& out);
    void parse_operands(Instruction& inst);
    Operand parse_operand();
    Operand parse_register();
    Operand parse_immediate();
    Operand parse_memory();

    // 标号定义
    bool check_label_def();
};

} // namespace sass

#endif // SASS_PARSER_H