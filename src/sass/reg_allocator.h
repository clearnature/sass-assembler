/* HunTian SASS — 寄存器分配器 (线性扫描)
 *
 * Pascal: 最多 255 寄存器/线程 (R0-R254 + RZ)
 * 目标: 减少寄存器冲突, 避免溢出
 *
 * 方法:
 *   1. 计算活跃区间
 *   2. 线性扫描分配物理寄存器
 *   3. 检测并报告寄存器压力
 */

#pragma once
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <set>
#include "instruction.h"

namespace sass::optimizer {

class RegisterAllocator {
    // GP106 实测: >64 regs → 153x slowdown (from hardware topology)
    static constexpr int MAX_REGS = 48;  // 留16个给系统 (实测最优: 32-48)
    static constexpr int RZ = 255;

    struct LiveRange {
        int reg_id;       // 虚拟寄存器ID
        int first_use;    // 第一次使用的位置
        int last_use;     // 最后一次使用的位置
        int phys_reg;     // 分配的物理寄存器 (-1=未分配)
    };

public:
    // 重命名寄存器以减少冲突
    void allocate(std::vector<Instruction>& insts) {
        if (insts.size() < 8) return;  // 短序列寄存器压力低, 无需重命名

        // 1. 计算活跃区间
        std::unordered_map<int, LiveRange> ranges;
        for (size_t i = 0; i < insts.size(); i++) {
            for (auto& op : insts[i].operands) {
                if (op.type != OpType::REG || op.reg_id == RZ) continue;
                int r = op.reg_id;
                if (!ranges.count(r)) ranges[r] = {r, (int)i, (int)i, -1};
                ranges[r].last_use = (int)i;
            }
        }

        // 2. 线性扫描分配
        std::set<int> free_regs;
        for (int r = 0; r < MAX_REGS; r++) free_regs.insert(r);

        std::vector<LiveRange> sorted;
        for (auto& kv : ranges) sorted.push_back(kv.second);
        std::sort(sorted.begin(), sorted.end(),
            [](const LiveRange& a, const LiveRange& b) { return a.first_use < b.first_use; });

        // 活跃集合: <end_position, phys_reg>
        std::vector<std::pair<int,int>> active;

        for (auto& lr : sorted) {
            // 释放已结束的寄存器
            active.erase(std::remove_if(active.begin(), active.end(),
                [&](auto& a) { return a.first < lr.first_use; }), active.end());

            // 重新填充 free_regs
            free_regs.clear();
            for (int r = 0; r < MAX_REGS; r++) free_regs.insert(r);
            for (auto& a : active) free_regs.erase(a.second);

            if (!free_regs.empty()) {
                lr.phys_reg = *free_regs.begin();
                active.push_back({lr.last_use, lr.phys_reg});
            } else {
                // 溢出: 保持原寄存器
                lr.phys_reg = lr.reg_id % MAX_REGS;
            }
            ranges[lr.reg_id] = lr;
        }

        // 3. 应用重命名
        for (auto& inst : insts) {
            for (auto& op : inst.operands) {
                if (op.type == OpType::REG && op.reg_id != RZ) {
                    auto it = ranges.find(op.reg_id);
                    if (it != ranges.end() && it->second.phys_reg >= 0)
                        op.reg_id = (uint8_t)it->second.phys_reg;
                }
            }
        }
    }

    // 分析寄存器压力
    struct PressureReport {
        int max_live;           // 最大同时活跃寄存器
        int total_spills;       // 溢出次数
        double pressure_pct;    // 压力百分比 (max_live / MAX_REGS)
    };

    PressureReport analyze(const std::vector<Instruction>& insts) {
        PressureReport rpt = {0, 0, 0.0};
        int live = 0;
        std::unordered_map<int,int> last_use;

        // 计算每个寄存器的最后使用位置
        for (size_t i = 0; i < insts.size(); i++) {
            for (auto& op : insts[i].operands) {
                if (op.type == OpType::REG && op.reg_id != RZ)
                    last_use[op.reg_id] = (int)i;
            }
        }

        for (size_t i = 0; i < insts.size(); i++) {
            // 计数当前活跃的寄存器
            live = 0;
            for (auto& lu : last_use) {
                if (lu.second >= (int)i) live++;
                // 检查是否第一次出现
                bool first_seen = false;
                for (size_t j = 0; j <= i; j++) {
                    for (auto& op : insts[j].operands)
                        if (op.type==OpType::REG && op.reg_id==lu.first) {first_seen=true;break;}
                    if (first_seen) break;
                }
                if (!first_seen) live--;
            }
            if (live < 0) live = 0;
            rpt.max_live = std::max(rpt.max_live, live);
        }

        rpt.pressure_pct = (double)rpt.max_live / MAX_REGS * 100.0;
        return rpt;
    }
};

// ─── 便捷接口 ───
inline void reg_allocate(std::vector<Instruction>& insts) {
    RegisterAllocator ra;
    ra.allocate(insts);
}

inline RegisterAllocator::PressureReport reg_pressure(const std::vector<Instruction>& insts) {
    RegisterAllocator ra;
    return ra.analyze(insts);
}

} // namespace sass::optimizer
