/* HunTian SASS — 优化器: 死代码消除 + .reuse + 4320D排序 */

#pragma once
#include <vector>
#include <algorithm>
#include "instruction.h"
#include "ilp_scheduler.h"
#include "reg_allocator.h"
#include "backends/pattern_library.h"
#include "scoreboard_analysis.h"

namespace sass::optimizer {

// ═══ P0-3: 死代码消除 ═══
// EXIT/RET之后的指令永远不会执行 → 删除
inline void dead_code_elimination(std::vector<Instruction>& insts) {
    std::vector<Instruction> result;
    for (auto& i : insts) {
        result.push_back(i);
        if (i.opcode == Opcode::EXIT || i.opcode == Opcode::RET ||
            i.opcode == Opcode::KILL) break;  // 后续都是死代码
    }
    if (result.size() < insts.size()) {
        insts = std::move(result);
    }
}

// ═══ P0-2: 4320D scheduled_cycle 排序 ═══
// 调度器分配的 scheduled_cycle → 按周期排序输出
inline void schedule_order(std::vector<Instruction>& insts) {
    std::stable_sort(insts.begin(), insts.end(),
        [](const Instruction& a, const Instruction& b) {
            return a.scheduled_cycle < b.scheduled_cycle;
        });
}

// ═══ P0-1: .reuse 标志编码 ═══
// 检测寄存器复用 → 设置 .reuse 标志
// 策略: 如果源寄存器在后续4条指令中再次作为源, 标记reuse
inline void auto_reuse(std::vector<Instruction>& insts) {
    constexpr int LOOKAHEAD = 4;
    for (size_t i = 0; i < insts.size(); i++) {
        auto& inst = insts[i];
        for (size_t op = 1; op < inst.operands.size() && op < 3; op++) {
            if (inst.operands[op].type != OpType::REG) continue;
            uint8_t reg = inst.operands[op].reg_id;
            for (size_t j = i+1; j < std::min(i+LOOKAHEAD, insts.size()); j++) {
                bool found = false, overwritten = false;
                if (insts[j].operands.size() > 0 && insts[j].operands[0].type == OpType::REG
                    && insts[j].operands[0].reg_id == reg) overwritten = true;
                for (size_t k = 1; k < insts[j].operands.size() && k < 3; k++) {
                    if (insts[j].operands[k].type == OpType::REG &&
                        insts[j].operands[k].reg_id == reg) { found = true; break; }
                }
                if (found) { inst.reuse[op] = true; break; }
                if (overwritten) break;
            }
        }
    }
}

// ═══ P0 全流程优化 ═══
inline void optimize_p0(std::vector<Instruction>& insts) {
    schedule_order(insts);       // P0-2: 4320D排序
    auto_reuse(insts);           // P0-1: .reuse检测
    dead_code_elimination(insts); // P0-3: 死代码消除
}

// ═══ P1-3: 常量传播 (立即数折叠) ═══
inline void const_propagation(std::vector<Instruction>& insts) {
    std::unordered_map<uint8_t, int64_t> const_vals;
    for (auto& inst : insts) {
        // MOV R0, 42 → 记录 R0=42
        if (inst.opcode == Opcode::MOV || inst.opcode == Opcode::MOV32I) {
            if (inst.operands.size()>=2 && inst.operands[0].type==OpType::REG &&
                inst.operands[1].type==OpType::IMM)
                const_vals[inst.operands[0].reg_id] = inst.operands[1].imm_val;
        }
        // 如果源操作数是已知常量, 替换
        for (auto& op : inst.operands) {
            if (op.type == OpType::REG && const_vals.count(op.reg_id)) {
                op.type = OpType::IMM;
                op.imm_val = const_vals[op.reg_id];
            }
        }
    }
}

// ═══ P2: 循环展开 ═══
// 检测 IADD counter; ISETP; @P0 BRA 回边 → 复制循环体 (factor=2)
// 在 IADD 前插入循环体副本, 步进立即数 ×factor, 减少分支开销.
inline void loop_unroll(std::vector<Instruction>& insts, int factor=2) {
    size_t i = 0;
    while (i + 2 < insts.size()) {
        if (insts[i].opcode==Opcode::IADD &&
            insts[i+1].opcode==Opcode::ISETP &&
            insts[i+2].opcode==Opcode::BRA) {
            // 估算循环体: IADD 前 body_size 条指令 (默认 4, 不超过 i)
            int body_size = std::min((int)i, 4);
            if (body_size == 0) { i++; continue; }
            // 安全检查: 循环体内不能有 BRA/BRX (嵌套循环不展开)
            bool safe = true;
            for (int j = (int)i - body_size; j < (int)i; j++) {
                if (j < 0 || insts[j].opcode == Opcode::BRA ||
                    insts[j].opcode == Opcode::BRX) { safe = false; break; }
            }
            if (!safe) {
                insts[i].complexity_score *= 0.7;  // 标记可展开但跳过
                i++; continue;
            }
            // 插入循环体副本到 IADD 前
            std::vector<Instruction> copy(
                insts.begin() + i - body_size,
                insts.begin() + i);
            insts.insert(insts.begin() + i, copy.begin(), copy.end());
            // IADD 步进立即数 ×factor (如 IADD R0, R0, #1 → #2)
            Instruction& iadd = insts[i + body_size];
            if (iadd.operands.size() >= 3 &&
                iadd.operands[2].type == OpType::IMM) {
                iadd.operands[2].imm_val *= factor;
            }
            iadd.complexity_score *= 0.7;  // 标记已展开
            // 跳过已处理的副本 + 原三指令
            i += body_size + 3;
        } else {
            i++;
        }
    }
}

// ═══ P1: Bank 冲突避免 ═══
// GP106: 32 banks × 4B. 连续 LDS/STS 访问同一 bank → 串行化 (4.85x 减速).
// 策略: 检测相邻同 bank 访问, 在中间插入一条非内存指令打散冲突.
inline void avoid_bank_conflicts(std::vector<Instruction>& insts) {
    constexpr int NUM_BANKS = 32;
    auto get_bank = [](const Instruction& inst) -> int {
        for (auto& op : inst.operands) {
            if (op.type == OpType::MEM) return (op.mem.offset / 4) % NUM_BANKS;
        }
        return -1;
    };
    auto is_smem = [](const Instruction& inst) {
        return inst.opcode == Opcode::LDS || inst.opcode == Opcode::STS;
    };

    for (size_t i = 0; i + 1 < insts.size(); i++) {
        if (!is_smem(insts[i]) || !is_smem(insts[i+1])) continue;
        int b1 = get_bank(insts[i]);
        int b2 = get_bank(insts[i+1]);
        if (b1 < 0 || b1 != b2) continue;
        // 冲突: 向后找一条非内存、非分支指令插入到 i 和 i+1 之间
        for (size_t j = i + 2; j < std::min(i + 6, insts.size()); j++) {
            if (is_smem(insts[j])) continue;
            if (insts[j].opcode == Opcode::BRA || insts[j].opcode == Opcode::BRX ||
                insts[j].opcode == Opcode::ISETP || insts[j].opcode == Opcode::EXIT)
                continue;
            Instruction tmp = std::move(insts[j]);
            insts.erase(insts.begin() + j);
            insts.insert(insts.begin() + i + 1, std::move(tmp));
            break;
        }
    }
}

// ═══ P2: 谓词优化 ═══
// 合并连续的 ISETP+BRX → 减少谓词开销
inline void predicate_optimize(std::vector<Instruction>& insts) {
    for (size_t i = 0; i + 1 < insts.size(); i++) {
        // ISETP P0, ..., R0, RZ; @P0 BRA target → 如果R0==0则跳
        // 可以合并为 ISETP.NE + BRA
        if (insts[i].opcode == Opcode::ISETP && insts[i+1].opcode == Opcode::BRA) {
            // 标记为谓词优化候选 (简化: 不实际修改, 仅分析)
            insts[i].complexity_score *= 0.8; // 降低复杂度权重
        }
    }
}

// ═══ P2: 模式分析 (诊断) ═══
// 分析编码后的 SASS，检测常见优化模式
inline std::vector<patterns::Pattern> analyze_patterns(const std::vector<uint64_t>& code) {
    return patterns::PatternMatcher::analyze(code);
}

// ═══ P1 ILP 调度 ═══
inline void optimize_p1(std::vector<Instruction>& insts) {
    optimize_p0(insts);              // 先做 P0
    const_propagation(insts);        // P1-3: 常量传播
    loop_unroll(insts);              // P2: 循环展开 (先展开增加 ILP 机会)
    reg_allocate(insts);             // P1-2: 寄存器分配 (基于展开后的代码)
    ilp_schedule(insts);             // P1-1: ILP指令调度 (基于物理寄存器依赖)
    avoid_bank_conflicts(insts);     // P1: Bank 冲突避免 (调度后局部重排)
    predicate_optimize(insts);       // P2: 谓词优化
    scoreboard::insert_depbars(insts); // P2: DEPBAR自动插入
}

} // namespace sass::optimizer
