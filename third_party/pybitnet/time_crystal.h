// time_crystal.h — 时间晶体离散相位 (Time Crystal Discrete Phase)
//
// 宪法声明:
//   范畴: L5孤子引擎的时序离散化 — C3周期的时间量子化
//   数学: 主权状态机的时间不是连续流, 而是3个离散相位点 {0, 500, 1000}
//         对应 C3 1500步周期上的三个本征态。
//   物理: 时间晶体 = 不消耗能量的周期性运动 —
//         孤子在 C3 轮转中经历 0→500→1000 三个离散相位,
//         这是拓扑保护的, 不受连续统时间影响。
//   编译: C++23, constexpr, 纯整数。
#ifndef SOV_MATH_TIME_CRYSTAL_H
#define SOV_MATH_TIME_CRYSTAL_H

#include "lcm_constants.h"
#include "nayin_soliton_l5.h"
#include <cstdint>
#include <cmath>

namespace sov::math::l5 {

// ═══════════════════════════════════════════════════════
// 时间晶体离散相位: {0, 500, 1000} (C3 1500步周期)
// ═══════════════════════════════════════════════════════
inline constexpr int TIME_CRYSTAL_PHASES[3] = {0, 500, 1000};

// 返回当前步数对应的三个离散相位点之一
[[nodiscard]] constexpr int time_crystal_phase(int step) noexcept {
    int c3_step = step % C3_CYCLE_STEPS;  // [0, 1499]
    // 量化到最近的离散相位点
    int best = 0;
    int best_dist = C3_CYCLE_STEPS;
    for (int p : TIME_CRYSTAL_PHASES) {
        int dist = (c3_step > p) ? (c3_step - p) : (p - c3_step);
        // 处理环面回绕: |1499 - 0| 在环面上距离为1
        int wrap_dist = C3_CYCLE_STEPS - dist;
        if (wrap_dist < dist) dist = wrap_dist;
        if (dist < best_dist) { best_dist = dist; best = p; }
    }
    return best;
}

// 将连续统输入映射到最近的离散相位点
[[nodiscard]] constexpr int route_to_phase(int step) noexcept {
    return time_crystal_phase(step);
}

// 离散相位跳跃: current → next phase in {0, 500, 1000}
[[nodiscard]] constexpr int phase_jump(int current_phase) noexcept {
    if (current_phase == 0)   return 500;
    if (current_phase == 500) return 1000;
    return 0;  // 1000 → 0 (wrap)
}

} // namespace sov::math::l5

#endif // SOV_MATH_TIME_CRYSTAL_H
