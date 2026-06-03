/* ============================================================================
 * HunTian SASS 汇编器 — 语法分析器实现
 * ============================================================================ */

#include "sass/parser.h"
#include <cctype>
#include <sstream>
#include <algorithm>

namespace sass {

// ─── 操作码映射表 ───
Opcode opcode_from_string(const std::string& s) {
    static const struct { const char* name; Opcode op; } map[] = {
        // 整数
        {"iadd",   Opcode::IADD},
        {"iadd3",  Opcode::IADD3},
        {"iadd.x", Opcode::IADD_X},
        {"iscadd", Opcode::ISCADD},
        {"isub",   Opcode::ISUB},
        {"imad",   Opcode::IMAD},
        {"imul",   Opcode::IMUL},
        {"imnmx",  Opcode::IMNMX},
        {"shl",    Opcode::SHL},
        {"shr",    Opcode::SHR},
        {"bfe",    Opcode::BFE},
        {"bfi",    Opcode::BFI},
        {"lop3",   Opcode::LOP3},
        {"lop32i", Opcode::LOP32I},
        {"brev",   Opcode::BREV},

        // 浮点
        {"fadd",   Opcode::FADD},
        {"fmul",   Opcode::FMUL},
        {"fset",   Opcode::FSET},
        {"fsel",   Opcode::FSEL},
        {"frcp",   Opcode::FRCP},
        {"fsqrt",  Opcode::FSQRT},
        {"fmnmx",  Opcode::FMNMX},

        // XMAD + FFMA
        {"ffma",   Opcode::FFMA},
        {"xmad",   Opcode::XMAD},
        {"xmad.mrg", Opcode::XMAD_MRG},
        {"xmad.psl", Opcode::XMAD_PSL},

        // 内存
        {"ldg",    Opcode::LDG},
        {"lds",    Opcode::LDS},
        {"stg",    Opcode::STG},
        {"sts",    Opcode::STS},
        {"ldc",    Opcode::LDC},

        // 控制流
        {"bra",    Opcode::BRA},
        {"brx",    Opcode::BRX},
        {"cal",    Opcode::CAL},
        {"ret",    Opcode::RET},
        {"exit",   Opcode::EXIT},
        {"kill",   Opcode::KILL},
        {"yield",  Opcode::YIELD},

        // 同步
        {"bar",     Opcode::BAR},
        {"depbar",  Opcode::DEPBAR},
        {"membar",  Opcode::MEMBAR},

        // 数据移动
        {"mov",    Opcode::MOV},
        {"mov32i", Opcode::MOV32I},
        {"sel",    Opcode::SEL},
        {"s2r",    Opcode::S2R},

        // 转换
        {"cvt",    Opcode::CVT},

        // 比较/谓词
        {"iset",   Opcode::ISET},
        {"isetp",  Opcode::ISETP},
        {"fsetp",  Opcode::FSETP},
        {"setp",   Opcode::SETP},

        // HunTian VAVX3
        {"vavx3.add", Opcode::VAVX3_ADD_512},
        {"vavx3.mul", Opcode::VAVX3_MUL_512},
        {"vavx3.mad", Opcode::VAVX3_MAD_512},
        {"vavx3.mma", Opcode::VAVX3_MMA_512},
        {"vavx3.geom", Opcode::VAVX3_GEOM},
        {"vavx3.shuffle", Opcode::VAVX3_SHUFFLE},
        {"vavx3.braid", Opcode::VAVX3_BRAID},
        {"vavx3.laplacian", Opcode::VAVX3_LAPLACIAN},

        // HunTian 三进制
        {"tmad",   Opcode::TMAD},
        {"tmul",   Opcode::TMUL},
        {"tconv",  Opcode::TCONV},
        {"tryte",  Opcode::TRYTE_OP},
    };
    for (const auto& e : map) {
        if (s == e.name) return e.op;
    }
    return Opcode::UNKNOWN;
}

const char* opcode_to_string(Opcode op) {
    switch (op) {
        case Opcode::FFMA: return "ffma";
        case Opcode::XMAD: return "xmad";
        case Opcode::IADD: return "iadd";
        case Opcode::IMAD: return "imad";
        case Opcode::LDG:  return "ldg";
        case Opcode::LDS:  return "lds";
        case Opcode::STG:  return "stg";
        case Opcode::STS:  return "sts";
        case Opcode::BRA:  return "bra";
        case Opcode::EXIT: return "exit";
        case Opcode::VAVX3_ADD_512: return "vavx3.add";
        case Opcode::VAVX3_MMA_512: return "vavx3.mma";
        case Opcode::VAVX3_GEOM: return "vavx3.geom";
        case Opcode::TMAD: return "tmad";
        case Opcode::TMUL: return "tmul";
        case Opcode::TCONV: return "tconv";
        default: return "unknown";
    }
}

// ─── Parser 实现 ───
Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens)), pos_(0) {}

Parser::Parser(Lexer& lexer)
    : tokens_(lexer.tokenize_all()), pos_(0) {}

const Token& Parser::current() const {
    return tokens_[pos_ < tokens_.size() ? pos_ : tokens_.size() - 1];
}

const Token& Parser::peek_next() const {
    return tokens_[std::min(pos_ + 1, tokens_.size() - 1)];
}

void Parser::advance() {
    if (pos_ < tokens_.size()) pos_++;
}

bool Parser::match(TokenType type) {
    if (current().type == type) { advance(); return true; }
    return false;
}

bool Parser::consume_expected(TokenType type, const std::string& msg) {
    if (current().type == type) { advance(); return true; }
    return false;
}

bool Parser::check_label_def() {
    // 检查模式: IDENTIFIER ":"（标号定义）
    // 注意：我们已经在 lexer 中用 LABEL_DEF 处理了 ":"，但标号名在前一个 IDENTIFIER 里
    // 更准确的方式：当前 token 是 IDENTIFIER，下一个是 ":"
    if (current().type == TokenType::IDENTIFIER && peek_next().type == TokenType::LABEL_DEF) {
        return true;
    }
    return false;
}

ParseResult Parser::parse_all() {
    ParseResult result;

    while (pos_ < tokens_.size() && current().type != TokenType::END_OF_FILE) {
        // 跳过无效 token
        if (current().type == TokenType::INVALID) {
            advance();
            continue;
        }

        // 伪指令: .func xxx
        if (current().type == TokenType::DOT) {
            parse_directive(result.instructions);
            continue;
        }

        // 标号定义: xxx: （IDENTIFIER 后跟 LABEL_DEF）
        if (check_label_def()) {
            std::string label_name = current().text;
            symbol_table_[label_name] = static_cast<uint32_t>(result.instructions.size());
            advance(); // 跳过 IDENTIFIER
            advance(); // 跳过 LABEL_DEF
            continue;
        }

        // 指令
        if (current().type == TokenType::OPCODE) {
            if (!parse_instruction(result.instructions)) {
                result.ok = false;
                result.error = "Failed to parse instruction at line " +
                               std::to_string(current().line);
                return result;
            }
            continue;
        }

        // 未知 token，跳过
        advance();
    }

    // 第二遍：解析标号引用（BRA 等）
    result.labels = symbol_table_;
    result.ok = true;
    return result;
}

void Parser::parse_directive(std::vector<Instruction>& out) {
    // 格式: .func name 或 .endfunc
    advance(); // 跳过 DOT
    if (current().type == TokenType::IDENTIFIER) {
        std::string dir = current().text;
        advance();

        if (dir == "func" || dir == "function") {
            // .func name → 创建伪指令
            // 下一个 token 是函数名
        }
    }
    // 其他伪指令暂时忽略
}

bool Parser::parse_instruction(std::vector<Instruction>& out) {
    // 操作码
    std::string op_str = current().text;
    Opcode op = opcode_from_string(op_str);
    advance();

    Instruction inst;
    inst.opcode = op;
    inst.complexity_score = 1.0;

    // 解析操作数
    parse_operands(inst);

    out.push_back(inst);
    return true;
}

void Parser::parse_operands(Instruction& inst) {
    // 格式: dst, src1, src2 或 dst, [mem] 等
    // 先尝试读取第一个操作数
    if (current().type == TokenType::REGISTER ||
        current().type == TokenType::IMMEDIATE ||
        current().type == TokenType::LBRACKET ||
        current().type == TokenType::IDENTIFIER) {
        inst.operands.push_back(parse_operand());

        // 后续操作数以逗号分隔
        while (current().type == TokenType::COMMA) {
            advance(); // 跳过逗号
            if (current().type == TokenType::REGISTER ||
                current().type == TokenType::IMMEDIATE ||
                current().type == TokenType::LBRACKET ||
                current().type == TokenType::IDENTIFIER) {
                inst.operands.push_back(parse_operand());
            } else {
                break;
            }
        }
    }
}

Operand Parser::parse_operand() {
    if (current().type == TokenType::LBRACKET) return parse_memory();
    if (current().type == TokenType::REGISTER) return parse_register();
    if (current().type == TokenType::IMMEDIATE) return parse_immediate();
    if (current().type == TokenType::IDENTIFIER) {
        // 标号引用
        std::string name = current().text;
        advance();
        Operand op;
        op.type = OpType::LABEL;
        op.label_name = name;
        return op;
    }
    // fallback
    advance();
    Operand op;
    op.type = OpType::REG;
    op.reg_id = 0;
    return op;
}

Operand Parser::parse_register() {
    Operand op;
    op.type = OpType::REG;

    std::string text = current().text;
    if (text.size() > 1) {
        if (text[0] == 'R' || text[0] == 'P') {
            op.reg_id = static_cast<uint8_t>(std::stoul(text.substr(1)));
        }
    }
    // 检查是否为 VAVX3 寄存器 (V0-V255)
    // 目前所有都视为 REG
    advance();
    return op;
}

Operand Parser::parse_immediate() {
    Operand op;
    op.type = OpType::IMM;
    op.imm_val = current().to_int();
    advance();
    return op;
}

Operand Parser::parse_memory() {
    // 格式: [Rbase + offset]
    advance(); // 跳过 [
    Operand op;
    op.type = OpType::MEM;

    // 基址寄存器
    if (current().type == TokenType::REGISTER) {
        op.mem.base = current().to_reg();
        advance();
    }

    // + offset
    if (current().type == TokenType::PLUS) {
        advance(); // 跳过 +
    }

    if (current().type == TokenType::IMMEDIATE) {
        op.mem.offset = static_cast<int16_t>(current().to_int());
        advance();
    }

    // ]
    if (current().type == TokenType::RBRACKET) {
        advance();
    }

    return op;
}

} // namespace sass