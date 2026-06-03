/* ============================================================================
 * HunTian SASS 汇编器 — Assembler Facade 实现
 * ============================================================================ */

#include "assembler.h"

namespace sass {

std::vector<SASSWord> Assembler::assemble(const std::string& source) {
    auto tokens = tokenize(source);
    auto result = parse(tokens);

    if (!result.ok || result.instructions.empty()) {
        return {};
    }

    auto& instructions = result.instructions;

    // optimize() 内部已通过 ManifoldScheduler 执行完整的 4320D 调度
    auto opt_result = optimize(instructions);
    return encode(opt_result, result.labels);
}

std::vector<Token> Assembler::tokenize(const std::string& source) {
    Lexer lexer(source);
    return lexer.tokenize_all();
}

ParseResult Assembler::parse(const std::vector<Token>& tokens) {
    Parser parser(tokens);
    return parser.parse_all();
}

ManifoldResult Assembler::optimize(const std::vector<Instruction>& instructions) {
    if (scheduler_) return scheduler_->optimize(instructions);
    ManifoldScheduler scheduler;
    return scheduler.optimize(instructions);
}

std::vector<SASSWord> Assembler::encode(
    const ManifoldResult& result,
    const std::unordered_map<std::string, uint32_t>& labels)
{
    if (encoder_) return encoder_->encode_with_labels(result, labels);
    BlockEncoder encoder;
    return encoder.encode_with_labels(result, labels);
}

} // namespace sass
