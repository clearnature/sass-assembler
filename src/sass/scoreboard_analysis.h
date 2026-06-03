/* HunTian SASS — Scoreboard 延迟分析 + DEPBAR 自动插入
 *
 * Pascal (64-bit): DEPBAR 显式指令管理依赖
 * Volta+ (128-bit): scoreboard bits[115:110] 嵌入控制字
 *
 * 策略: 分析 RAW 依赖 → 计算最小延迟周期 → 插入 DEPBAR / 设 scoreboard
 */

#pragma once
#include <vector>
#include <cstdint>
#include "instruction.h"

namespace sass::scoreboard {

// ─── 依赖分析 ───
struct DepInfo {
    int producer_idx;    // 生产者指令索引
    int consumer_idx;    // 消费者指令索引
    uint8_t reg_id;      // 依赖的寄存器
    int min_cycles;      // 最小延迟周期 (FMA=4, LDG=~350)
};

inline std::vector<DepInfo> analyze_dependencies(const std::vector<Instruction>& insts) {
    std::vector<DepInfo> deps;
    // 追踪每个寄存器的最后写入
    int last_writer[256];
    for (int i=0;i<256;i++) last_writer[i]=-1;

    for (size_t i=0;i<insts.size();i++) {
        // 检查源操作数: 如果有 RAW 依赖
        for (size_t op_idx=1;op_idx<insts[i].operands.size();op_idx++) {
            if (insts[i].operands[op_idx].type!=OpType::REG) continue;
            uint8_t r=insts[i].operands[op_idx].reg_id;
            if (last_writer[r]>=0) {
                int latency=4;  // default FMA
                if (insts[last_writer[r]].opcode==Opcode::LDG||
                    insts[last_writer[r]].opcode==Opcode::LDC) latency=350;
                deps.push_back({last_writer[r],(int)i,r,latency});
            }
        }
        // 更新最后写入
        if (!insts[i].operands.empty()&&insts[i].operands[0].type==OpType::REG)
            last_writer[insts[i].operands[0].reg_id]=(int)i;
    }
    return deps;
}

// ─── DEPBAR 插入 (Pascal) ───
// 在依赖距离 > 可隐藏延迟时插入 DEPBAR
inline void insert_depbars(std::vector<Instruction>& insts) {
    auto deps=analyze_dependencies(insts);
    // 按 consumer 位置逆序处理 (避免索引偏移)
    std::sort(deps.begin(),deps.end(),[](auto&a,auto&b){return a.consumer_idx>b.consumer_idx;});

    for (auto& dep : deps) {
        int distance=dep.consumer_idx-dep.producer_idx;
        // 如果依赖距离太近 (< 最小延迟), 需要隔离
        if (distance < dep.min_cycles/2) {
            // 插入 DEPBAR 在消费者之前
            Instruction depbar; depbar.opcode=Opcode::DEPBAR;
            insts.insert(insts.begin()+dep.consumer_idx, depbar);
        }
    }
}

// ─── Scoreboard 报告 ───
struct ScoreboardReport {
    int total_deps;
    int critical_deps;    // 需要 DEPBAR 的依赖
    int max_distance;     // 最大依赖距离
    double avg_distance;
};

inline ScoreboardReport analyze(const std::vector<Instruction>& insts) {
    auto deps=analyze_dependencies(insts);
    ScoreboardReport r{0,0,0,0.0};
    r.total_deps=(int)deps.size();
    double sum=0;
    for (auto& d : deps) {
        int dist=d.consumer_idx-d.producer_idx;
        if (dist<d.min_cycles/2) r.critical_deps++;
        r.max_distance=std::max(r.max_distance,dist);
        sum+=dist;
    }
    if (!deps.empty()) r.avg_distance=sum/deps.size();
    return r;
}

} // namespace sass::scoreboard
