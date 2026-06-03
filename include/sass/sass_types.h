/* ============================================================================
 * HunTian SASS 汇编器 — 公共类型定义
 * ============================================================================ */

#ifndef SASS_SASS_TYPES_H
#define SASS_SASS_TYPES_H

#include "instruction.h"
#include <vector>
#include <span>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace sass {

using SASSWord = uint64_t;

// ─── 4320D 流形调度结果 ───
struct ManifoldResult {
    uint64_t block_id;
    std::vector<Instruction> optimized_sequence;
    double bubble_score;
};

} // namespace sass

// 接口层依赖上述类型，必须在类型定义之后 include
#include "scheduler_interface.h"
#include "encoder_interface.h"

namespace sass {

// ─── 4320D 流形调度器（声明） ───
class ManifoldScheduler : public IScheduler {
public:
    ManifoldScheduler() : next_id_(0) {}
    ManifoldResult optimize(const std::span<const Instruction>& block) override;
private:
    uint64_t next_id_;
};

// ─── 块编码器（声明） ───
class BlockEncoder : public IEncoder {
public:
    BlockEncoder() = default;

    std::vector<SASSWord> encode(const ManifoldResult& result) override;
    std::vector<SASSWord> encode_with_labels(
        const ManifoldResult& result,
        const std::unordered_map<std::string, uint32_t>& labels) override;

private:
    bool can_fuse(const std::vector<Instruction>& seq, size_t idx);
    SASSWord encode_vavx3(const std::vector<Instruction>& seq, size_t start, uint32_t cycle);
    SASSWord encode_std(
        const Instruction& inst, uint32_t cycle,
        size_t inst_idx,
        const std::unordered_map<std::string, uint32_t>& labels,
        size_t current_pc);
};

// ─── SASS 程序（解析结果） ───
struct SASSProgram {
    std::vector<Instruction> instructions;
    std::unordered_map<std::string, uint32_t> labels;
    std::string function_name;
};

// ─── 汇编器配置 ───
struct AssemblerConfig {
    int opt_level = 3;
    bool use_4320d = false;
    bool use_fuse = false;
    bool verbose = false;
};

} // namespace sass

#endif // SASS_SASS_TYPES_H