/* HunTian SASS — ILP 指令调度器 (Scoreboard)
 *
 * Pascal warp 调度: 最多 2 指令/周期/warp
 * 目标: 最大化独立指令间的距离 (延迟隐藏)
 *
 * 方法:
 *   1. 构建寄存器依赖图
 *   2. 贪心调度: 优先发射操作数就绪的指令
 *   3. 在依赖指令间插入独立指令
 */

#pragma once
#include <vector>
#include <deque>
#include <algorithm>
#include <unordered_set>
#include "instruction.h"

namespace sass::optimizer {

// ─── ILP 调度器 ───
class ILPScheduler {
    struct RegState {
        int last_writer = -1;   // 最后写入该寄存器的指令索引
        int last_reader = -1;   // 最后读取该寄存器的指令索引
    };

    std::vector<RegState> regs;  // 256个寄存器状态
    // ─── GP106 硬件实测参数 (来自 /data/rtl-sdr/ptx_gp106) ───
    int fp_latency   = 4;        // FMA: 4 cycles (硬件拓扑确认)
    int int_latency  = 4;        // INT: 4 cycles (共享FP/INT流水线)
    int mem_latency  = 350;      // Global: 300-500 cycles (L2 dependent)
    int smem_latency = 8;        // Shared: 5-10 cycles
    int issue_width  = 2;        // 2 instr/cycle/warp-scheduler
    int schedulers   = 4;        // 4 warp schedulers/SM
    int max_regs     = 64;       // >64 regs: 153x slowdown (实测)

public:
    ILPScheduler() : regs(256) {}

    // 计算两条指令间的依赖距离
    int dependency_distance(const Instruction& a, const Instruction& b) const {
        // 如果 b 读取了 a 写入的寄存器 → RAW 依赖
        for (auto& op_b : b.operands) {
            if (op_b.type != OpType::REG) continue;
            for (auto& op_a : a.operands) {
                if (op_a.type == OpType::REG && op_a.reg_id == op_b.reg_id)
                    return fp_latency;  // GP106: FMA=4 cycles
            }
        }
        return 0;  // 无依赖
    }

    // 主要调度算法
    void schedule(std::vector<Instruction>& insts) {
        if (insts.size() < 3) return;  // 太小, 不需要调度

        std::vector<Instruction> result;
        std::deque<size_t> ready;  // 就绪队列
        std::vector<bool> scheduled(insts.size(), false);
        std::vector<int> issue_cycle(insts.size(), -1);

        int cycle = 0;
        size_t scheduled_count = 0;

        while (scheduled_count < insts.size()) {
            // 找出所有操作数就绪的指令
            for (size_t i = 0; i < insts.size(); i++) {
                if (scheduled[i]) continue;
                if (operands_ready(insts[i], cycle)) {
                    ready.push_back(i);
                }
            }

            // 发射最多 issue_width 条指令
            int issued = 0;
            while (!ready.empty() && issued < issue_width) {
                size_t idx = ready.front(); ready.pop_front();
                if (scheduled[idx]) continue;

                result.push_back(insts[idx]);
                issue_cycle[idx] = cycle;
                update_register_state(insts[idx], cycle);
                scheduled[idx] = true;
                scheduled_count++;
                issued++;
            }

            if (issued == 0 && scheduled_count < insts.size()) {
                // 无就绪指令, 强制发射第一条未调度的
                for (size_t i = 0; i < insts.size(); i++) {
                    if (!scheduled[i]) {
                        result.push_back(insts[i]);
                        issue_cycle[i] = cycle;
                        update_register_state(insts[i], cycle);
                        scheduled[i] = true;
                        scheduled_count++;
                        break;
                    }
                }
            }
            cycle++;
        }

        insts = std::move(result);
    }

private:
    bool operands_ready(const Instruction& inst, int cycle) const {
        for (auto& op : inst.operands) {
            if (op.type != OpType::REG) continue;
            const auto& rs = regs[op.reg_id];
            // 如果最近写入在 latency 周期内 → 未就绪
            if (rs.last_writer >= 0 && (cycle - rs.last_writer) < fp_latency)
                return false;
        }
        return true;
    }

    void update_register_state(const Instruction& inst, int cycle) {
        // 写入寄存器 (dst)
        if (!inst.operands.empty() && inst.operands[0].type == OpType::REG) {
            uint8_t rd = inst.operands[0].reg_id;
            regs[rd].last_writer = cycle;
        }
        // 读取寄存器 (src)
        for (size_t i = 1; i < inst.operands.size(); i++) {
            if (inst.operands[i].type == OpType::REG) {
                regs[inst.operands[i].reg_id].last_reader = cycle;
            }
        }
    }
};

// ─── 便捷接口 ───
inline void ilp_schedule(std::vector<Instruction>& insts) {
    ILPScheduler sched;
    sched.schedule(insts);
}

} // namespace sass::optimizer
