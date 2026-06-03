/* ============================================================================
 * HunTian SASS 汇编器 — 词法分析器实现
 * ============================================================================ */

#include "sass/lexer.h"
#include <cctype>
#include <sstream>

namespace sass {

// ─── 操作码表 ───
static bool is_opcode(const std::string& s) {
    static const char* opcodes[] = {
        // 整数
        "iadd", "iadd3", "iadd.x", "iscadd", "isub",
        "imad", "imul", "imnmx",
        "shl", "shr", "bfe", "bfi", "lop3", "lop32i", "brev",
        // 浮点
        "fadd", "fmul", "fset", "fmnmx", "fsel", "frcp", "fsqrt",
        // XMAD + FFMA
        "ffma", "xmad", "xmad.mrg", "xmad.psl",
        // 内存
        "ldg", "lds", "stg", "sts", "ldc",
        // 控制流
        "bra", "brx", "cal", "ret", "exit", "kill", "yield",
        // 同步
        "bar", "depbar", "membar",
        // 数据移动
        "mov", "mov32i", "sel", "s2r",
        // 转换
        "cvt",
        // 比较/谓词
        "iset", "isetp", "fset", "fsetp", "setp",
        // HunTian VAVX3
        "vavx3.add", "vavx3.mul", "vavx3.mad", "vavx3.mma",
        "vavx3.geom", "vavx3.shuffle", "vavx3.braid", "vavx3.laplacian",
        // HunTian 三进制
        "tmad", "tmul", "tconv", "tryte"
    };
    for (const char* op : opcodes) {
        if (s == op) return true;
    }
    return false;
}

// ─── Token 辅助转换 ───
int64_t Token::to_int() const {
    if (type == TokenType::IMMEDIATE) {
        if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
            return std::stoull(text, nullptr, 16);
        return std::stoll(text);
    }
    return 0;
}

uint8_t Token::to_reg() const {
    if (type == TokenType::REGISTER && text.size() > 1 && text[0] == 'R') {
        return static_cast<uint8_t>(std::stoul(text.substr(1)));
    }
    return 0;
}

// ─── Lexer 实现 ───
Lexer::Lexer(std::string source)
    : source_(std::move(source)), pos_(0), line_(1), column_(1),
      has_peeked_(false) {}

char Lexer::current() const {
    return pos_ < source_.size() ? source_[pos_] : '\0';
}

char Lexer::advance() {
    char c = current();
    if (c == '\n') { line_++; column_ = 1; }
    else { column_++; }
    pos_++;
    return c;
}

Token Lexer::make_token(TokenType type, std::string text) {
    return {type, std::move(text), line_, column_};
}

void Lexer::skip_whitespace() {
    while (current() && (current() == ' ' || current() == '\t')) {
        advance();
    }
}

void Lexer::skip_line_comment() {
    while (current() && current() != '\n') advance();
}

Token Lexer::read_register() {
    std::string text = "R";
    advance(); // skip 'R'
    while (current() && std::isdigit(current())) {
        text += advance();
    }
    return make_token(TokenType::REGISTER, text);
}

Token Lexer::read_number() {
    std::string text;
    if (current() == '-' || current() == '+') text += advance();
    // hex?
    if (current() == '0') {
        text += advance();
        if (current() == 'x' || current() == 'X') {
            text += advance();
            while (current() && (std::isxdigit(current()))) text += advance();
            return make_token(TokenType::IMMEDIATE, text);
        }
    }
    while (current() && std::isdigit(current())) text += advance();
    return make_token(TokenType::IMMEDIATE, text);
}

Token Lexer::read_identifier_or_opcode() {
    std::string text;
    while (current() && (std::isalnum(current()) || current() == '_' || current() == '.')) {
        text += advance();
    }
    if (is_opcode(text)) return make_token(TokenType::OPCODE, text);
    return make_token(TokenType::IDENTIFIER, text);
}

Token Lexer::next() {
    if (has_peeked_) {
        has_peeked_ = false;
        return peeked_;
    }

    // 跳过空白
    skip_whitespace();

    // 换行
    if (current() == '\n') {
        advance();
        // 跳过空行
        skip_whitespace();
        if (current() == '\n') return next();
        // 检查是否到达注释
        if (current() == ';') { skip_line_comment(); return next(); }
        // 检查是否有内容
        if (current() == '\0') return make_token(TokenType::END_OF_FILE, "");
        // 有内容，递归取下一个 token
        return next();
    }

    if (current() == '\0')
        return make_token(TokenType::END_OF_FILE, "");

    // 注释
    if (current() == ';') {
        skip_line_comment();
        return next(); // 跳过整行，继续
    }

    // 逗号
    if (current() == ',') { advance(); return make_token(TokenType::COMMA, ","); }

    // 左括号
    if (current() == '[') { advance(); return make_token(TokenType::LBRACKET, "["); }

    // 右括号
    if (current() == ']') { advance(); return make_token(TokenType::RBRACKET, "]"); }

    // 加号（但 - 可能是立即数的一部分）
    if (current() == '+') {
        advance();
        // 后退检查：如果前面是 [，则这是地址偏移的 +，否则是正数
        return make_token(TokenType::PLUS, "+");
    }

    // 减号或负数
    if (current() == '-') {
        advance();
        // 如果是数字开头，作为负数立即数
        if (std::isdigit(current())) {
            // 需要重新读取
            std::string text = "-";
            while (current() && std::isdigit(current())) text += advance();
            return make_token(TokenType::IMMEDIATE, text);
        }
        return make_token(TokenType::IMMEDIATE, "-");
    }

    // 数字
    if (std::isdigit(current())) return read_number();

    // 点号（伪指令）
    if (current() == '.') {
        std::string text = ".";
        advance();
        while (current() && std::isalpha(current())) text += advance();
        return make_token(TokenType::DOT, text);
    }

    // 寄存器 R...
    if (current() == 'R' || current() == 'P') {
        // 检查后面是否为数字
        char next_c = pos_ + 1 < source_.size() ? source_[pos_ + 1] : '\0';
        if (std::isdigit(next_c)) return read_register();
    }

    // 标识符或操作码
    if (std::isalpha(current()) || current() == '_') {
        return read_identifier_or_opcode();
    }

    // 其他字符
    if (current() == ':') {
        advance();
        return make_token(TokenType::LABEL_DEF, ":");
    }

    return make_token(TokenType::INVALID, std::string(1, advance()));
}

const Token& Lexer::peek() {
    if (!has_peeked_) {
        peeked_ = next();
        has_peeked_ = true;
    }
    return peeked_;
}

void Lexer::consume() {
    if (!has_peeked_) next();
    has_peeked_ = false;
}

std::vector<Token> Lexer::tokenize_all() {
    std::vector<Token> tokens;
    Token t;
    do {
        t = next();
        tokens.push_back(t);
    } while (t.type != TokenType::END_OF_FILE);
    return tokens;
}

} // namespace sass