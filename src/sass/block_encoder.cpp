/* ============================================================================
 * HunTian SASS 汇编器 — 块级编码器
 *
 * SASSWord 64 位编码格式:
 *   [63:56] opcode    [55:48] cycle
 *   [47:40] flags     [39:32] 操作数 3
 *   [31:16] 操作数 2  [15:0]  操作数 1
 *
 * 每个操作数 16 位编码:
 *   [15:14] 类型标签
 *     00 → REG        [13:8]=0   [7:0]=reg_id
 *     01 → IMM        [13:0]=有符号 14 位立即数
 *     10 → MEM(b)     [13:8]=base [7:0]=偏移
 *     11 → MEM(s)     [13:0]=有符号 14 位偏移 (stg 模式)
 * ============================================================================ */

#include "sass_types.h"
#include "operand_codec.h"
#include "opcode_table.h"
#include "reuse_detector.h"
#include <iostream>
#include <map>
#include <algorithm>
#include <cstring>

namespace sass {

// ══════════════════════════════════════════════════════════════════════
// BlockEncoder
// ══════════════════════════════════════════════════════════════════════

std::vector<SASSWord> BlockEncoder::encode(const ManifoldResult& result) {
    return encode_with_labels(result, {});
}

std::vector<SASSWord> BlockEncoder::encode_with_labels(
    const ManifoldResult& result,
    const std::unordered_map<std::string, uint32_t>& labels)
{
    std::vector<SASSWord> code;
    code.reserve(result.optimized_sequence.size());

    // ─── 自动插入 .reuse 标志 ───
    std::vector<Instruction> seq = result.optimized_sequence;
    auto_insert_reuse(seq);

    uint32_t cycle = 0;
    for (size_t i = 0; i < seq.size(); ++i) {
        const auto& inst = seq[i];

        if (can_fuse(seq, i)) {
            code.push_back(encode_vavx3(seq, i, cycle));
            i += 7;
        } else {
            code.push_back(encode_std(inst, cycle, i, labels, code.size()));
        }
        cycle++;
    }
    return code;
}

// ─── 融合判断 ───

bool BlockEncoder::can_fuse(const std::vector<Instruction>& seq, size_t idx) {
    if (idx + 7 >= seq.size()) return false;
    Opcode base = seq[idx].opcode;
    for (size_t i = idx; i < idx + 8; ++i) {
        if (seq[i].opcode != base) return false;
    }
    return true;
}

SASSWord BlockEncoder::encode_vavx3(const std::vector<Instruction>& seq,
                                     size_t start, uint32_t cycle) {
    uint8_t op = 0xF5;
    switch (seq[start].opcode) {
        case Opcode::FFMA: op = 0xF1; break;
        case Opcode::XMAD: op = 0xF2; break;
        default: break;
    }
    SASSWord w = 0;
    w |= static_cast<uint64_t>(op) << 56;
    w |= static_cast<uint64_t>(8) << 48;
    w |= static_cast<uint64_t>(cycle & 0xFF) << 40;

    // 编码 8 条指令的寄存器掩码
    uint64_t reg_mask = 0;
    for (size_t i = 0; i < 8 && (start + i) < seq.size(); ++i) {
        for (const auto& opnd : seq[start + i].operands) {
            if (opnd.type == OpType::REG)
                reg_mask |= (1ULL << opnd.reg_id);
        }
    }
    w |= reg_mask & 0xFFFFFFFFFFULL;
    return w;
}

SASSWord BlockEncoder::encode_std(
    const Instruction& inst, uint32_t cycle,
    size_t inst_idx, const std::unordered_map<std::string, uint32_t>& labels,
    size_t current_pc)
{
    SASSWord w = 0;
    w |= static_cast<uint64_t>(static_cast<uint16_t>(inst.opcode)) << 56;
    w |= static_cast<uint64_t>(cycle & 0xFF) << 48;

    // .reuse 标志编码到 flags 字段 [47:40]
    // bit 0 = operand 0 reuse, bit 1 = operand 1 reuse, bit 2 = operand 2 reuse
    uint8_t flags = 0;
    for (size_t i = 0; i < 3 && i < inst.operands.size(); ++i) {
        if (inst.reuse[i]) flags |= (1 << i);
    }
    w |= static_cast<uint64_t>(flags) << 40;

    // 编码最多 3 个操作数
    for (size_t i = 0; i < inst.operands.size() && i < 3; ++i) {
        const auto& opnd = inst.operands[i];
        uint16_t encoded = 0;

        switch (opnd.type) {
            case OpType::REG:
                encoded = encode_op_reg(opnd.reg_id);
                break;

            case OpType::IMM:
                encoded = encode_op_imm(static_cast<int16_t>(opnd.imm_val));
                break;

            case OpType::MEM:
                // stg [base + offset], src → 纯偏移模式
                // ldg dst, [base + offset] → base+offset 模式
                if (i == 0 && inst.opcode == Opcode::STG) {
                    // 目标地址在 operand 1，纯偏移
                    encoded = encode_op_mems(static_cast<int16_t>(opnd.mem.offset));
                } else {
                    encoded = encode_op_mem(opnd.mem.base, opnd.mem.offset);
                }
                break;

            case OpType::LABEL: {
                // 解析标号为 PC 相对偏移
                int16_t offset = 0;
                auto it = labels.find(opnd.label_name);
                if (it != labels.end()) {
                    int32_t target_inst = static_cast<int32_t>(it->second);
                    int32_t current_inst = static_cast<int32_t>(inst_idx);
                    offset = static_cast<int16_t>((target_inst - current_inst) * 8);
                }
                encoded = encode_op_imm(offset);
                break;
            }

            default:
                encoded = encode_op_reg(0);
                break;
        }

        // 放到对应的 16 位槽位
        w |= static_cast<uint64_t>(encoded) << (16 * i);
    }

    return w;
}

} // namespace sass