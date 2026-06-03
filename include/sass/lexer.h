/* ============================================================================
 * HunTian SASS 汇编器 — 词法分析器
 * ============================================================================ */

#ifndef SASS_LEXER_H
#define SASS_LEXER_H

#include <string>
#include <vector>
#include <cstdint>

namespace sass {

// ─── Token 类型 ───
enum class TokenType : uint8_t {
    // 指令
    OPCODE,

    // 操作数
    REGISTER,      // R0-R255
    IMMEDIATE,     // 立即数
    LABEL_DEF,     // 标号定义  loop:
    LABEL_REF,     // 标号引用  bra loop

    // 内存地址
    LBRACKET,      // [
    RBRACKET,      // ]
    PLUS,          // +

    // 分隔符
    COMMA,         // ,
    SEMICOLON,     // ;

    // 伪指令
    DOT,           // .
    IDENTIFIER,    // func, endfunc, reg

    // 注释与空白
    COMMENT,       // ; 注释

    // 文件结构
    END_OF_FILE,
    INVALID
};

// ─── Token 结构 ───
struct Token {
    TokenType type;
    std::string text;
    uint32_t line;
    uint32_t column;

    // 辅助转换
    int64_t to_int() const;
    uint8_t to_reg() const;
};

// ─── Lexer ───
class Lexer {
public:
    explicit Lexer(std::string source);

    // 逐个取 token
    Token next();
    // 预览下一个 token（不移除）
    const Token& peek();
    // 消耗当前 token
    void consume();

    // 取所有 token（调试用）
    std::vector<Token> tokenize_all();

    // 错误
    bool has_error() const { return !error_.empty(); }
    const std::string& error() const { return error_; }

private:
    std::string source_;
    size_t pos_;
    uint32_t line_;
    uint32_t column_;
    Token peeked_;
    bool has_peeked_;
    std::string error_;

    char current() const;
    char advance();
    void skip_whitespace();
    void skip_line_comment();
    Token read_register();
    Token read_number();
    Token read_identifier_or_opcode();
    Token make_token(TokenType type, std::string text);
};

} // namespace sass

#endif // SASS_LEXER_H