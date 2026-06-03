/* ============================================================================
 * HunTian SASS 汇编器 — .reuse 自动检测器
 *
 * NVIDIA SASS .reuse 标志：告诉 GPU 寄存器值将被下一条指令复用，
 * 保持在 RF 缓存中避免重新加载。
 *
 * 策略：如果当前指令的源寄存器在后续 N 条指令中再次作为源使用，
 *       则为该源操作数设置 .reuse 标志。
 * ============================================================================ */

#ifndef SASS_REUSE_DETECTOR_H
#define SASS_REUSE_DETECTOR_H

#include "instruction.h"
#include <vector>
#include <cstddef>

namespace sass {

inline void auto_insert_reuse(std::vector<Instruction>& seq) {
    // 向后查看 4 条指令（典型的 warp 调度窗口）
    constexpr size_t LOOKAHEAD = 4;

    for (size_t i = 0; i < seq.size(); ++i) {
        auto& inst = seq[i];
        // 只处理寄存器操作数
        for (size_t op = 0; op < inst.operands.size() && op < 3; ++op) {
            const auto& operand = inst.operands[op];
            if (operand.type != OpType::REG) continue;
            // 跳过目标寄存器 (operand 0 通常是 dst)
            if (op == 0) continue;

            uint8_t reg = operand.reg_id;
            // 向后查找相同寄存器作为源
            for (size_t j = i + 1; j < std::min(i + LOOKAHEAD, seq.size()); ++j) {
                const auto& future = seq[j];
                bool found = false;
                for (size_t k = 1; k < future.operands.size() && k < 3; ++k) {
                    if (future.operands[k].type == OpType::REG &&
                        future.operands[k].reg_id == reg) {
                        found = true;
                        break;
                    }
                }
                // 如果后续指令使用了这个寄存器作为源，标记 reuse
                // 但如果中间有写入这个寄存器的指令，则不清除（已被覆盖）
                bool overwritten = false;
                if (future.operands.size() > 0 &&
                    future.operands[0].type == OpType::REG &&
                    future.operands[0].reg_id == reg) {
                    overwritten = true;
                }
                if (found) {
                    inst.reuse[op] = true;
                    break;
                }
                if (overwritten) break;  // 寄存器被覆盖，停止搜索
            }
        }
    }
}

} // namespace sass

#endif // SASS_REUSE_DETECTOR_H
