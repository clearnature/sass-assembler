/* ============================================================================
 * HunTian SASS 汇编器 — 4320D 流形调度器
 *
 * 使用几何引擎对指令序列进行全局调度优化：
 *   1. 能量场投影 (4320 槽位环面)
 *   2. 离散拉普拉斯曲率 (simd_kernels.h)
 *   3. Yamabe 流平滑扩散 (simd_kernels.h)
 *   4. 能量排序 → 优化的指令发射顺序
 * ============================================================================ */

#include "sass_types.h"
#include "wrapper.h"
#include "simd_kernels.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace sass {

ManifoldResult ManifoldScheduler::optimize(const std::span<const Instruction>& block) {
    ManifoldResult res;
    res.block_id = next_id_++;

    // ─── 1. 构建 4320D 能量场 ───
    std::vector<double> energy(4320, 0.0);
    std::vector<size_t> slot_map;
    slot_map.reserve(block.size());

    uint64_t seed = 1;
    for (size_t i = 0; i < block.size(); ++i) {
        double phase = block[i].complexity_score * 0.618;
        uint32_t slot = manifold_slot(seed, block[i].complexity_score, phase);
        seed = void_spin_4320(seed + i * 7);
        slot_map.push_back(slot);
        energy[slot] += block[i].complexity_score;
    }

    // ─── 2. 计算流形曲率 ───
    std::vector<double> curvature(4320, 0.0);
    compute_laplacian_curvature(energy, curvature);

    // ─── 3. Yamabe 流平滑 ───
    std::vector<double> smoothed(4320, 0.0);
    std::vector<double> flow(4320, 0.0);
    double total_energy = 0.0;
    for (double e : energy) total_energy += e;

    compute_yamabe_flow(curvature, flow);
    compute_yamabe_smoothed(energy, flow, smoothed);

    // ─── 4. 提取优化序列 ───
    struct SlotInst { size_t slot; size_t index; double weight; };
    std::vector<SlotInst> order;
    order.reserve(block.size());
    for (size_t i = 0; i < block.size(); ++i) {
        order.push_back({slot_map[i], i, smoothed[slot_map[i]]});
    }

    std::sort(order.begin(), order.end(),
        [](const SlotInst& a, const SlotInst& b) {
            if (a.weight != b.weight) return a.weight > b.weight;
            return a.index < b.index;
        });

    // ─── 5. 构建输出 ───
    for (size_t i = 0; i < order.size(); ++i) {
        Instruction inst = block[order[i].index];
        inst.scheduled_cycle = static_cast<uint32_t>(order[i].slot / 64);
        res.optimized_sequence.push_back(inst);
    }

    double avg_curve = 0.0;
    for (double c : curvature) avg_curve += std::abs(c);
    res.bubble_score = (total_energy > 0) ? avg_curve / total_energy / 4320.0 : 1.0;

    return res;
}

} // namespace sass
