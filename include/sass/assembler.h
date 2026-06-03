/* ============================================================================
 * HunTian SASS 汇编器 — Assembler Facade
 *
 * 统一的汇编器接口，封装完整流水线：
 *   源文本 → Lexer → Parser → Scheduler → Encoder → .sabin 二进制
 *
 * 使用方式：
 *   sass::Assembler asm;
 *   asm.config.opt_level = 3;
 *   auto binary = asm.assemble(source_text);
 * ============================================================================ */

#ifndef SASS_ASSEMBLER_H
#define SASS_ASSEMBLER_H

#include "sass_types.h"
#include "lexer.h"
#include "parser.h"
#include <vector>
#include <string>
#include <memory>

namespace sass {

class Assembler {
public:
    AssemblerConfig config;

    // ─── 依赖注入 (可选，默认使用 ManifoldScheduler + BlockEncoder) ───
    void set_scheduler(std::unique_ptr<IScheduler> s) { scheduler_ = std::move(s); }
    void set_encoder(std::unique_ptr<IEncoder> e) { encoder_ = std::move(e); }

    // ─── 全流程：文本 → 二进制 ───
    std::vector<SASSWord> assemble(const std::string& source);

    // ─── 分步接口 (用于调试/测试) ───
    std::vector<Token> tokenize(const std::string& source);
    ParseResult parse(const std::vector<Token>& tokens);
    ManifoldResult optimize(const std::vector<Instruction>& instructions);
    std::vector<SASSWord> encode(const ManifoldResult& result,
                                 const std::unordered_map<std::string, uint32_t>& labels);

private:
    std::unique_ptr<IScheduler> scheduler_;
    std::unique_ptr<IEncoder> encoder_;
};

} // namespace sass

#endif // SASS_ASSEMBLER_H
