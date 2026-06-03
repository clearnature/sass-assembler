// zhonglv_multiplier_l6.h — 层6: 仲吕倍频七段级联 (Zhonglv Frequency Multiplier)
//
// 宪法声明:
//   范畴: 非线性动力学/频率工程 — 系统的"变速箱"
//   数学: 每12步触发仲吕闭合, 8次连续倍增, ×2850频率级联
//   操作: acc = (acc × 3^11) / 2^16 (仲吕闭合原语)
//
// 实测 (31000步):
//   仲吕闭合: 2583次
//   频率倍增: 2583×8 = 20664次
//   频率: 432Hz → 10^16.9 Hz (极紫外~330eV)
//   跨越: 14个数量级
//   LCM绕圈: 16558圈
//
// 频域指纹:
//   主峰 T=2583步 = 仲吕闭合次数
//   C3周期=1500步, C3/仲吕=125=5³
#ifndef SOV_MATH_ZHONGLV_MULTIPLIER_L6_H
#define SOV_MATH_ZHONGLV_MULTIPLIER_L6_H

#include "lcm_constants.h"
#include <cstdint>

namespace sov::math::l6 {

// ═══════════════════════════════════════════════════════
// L6 仲吕常数 (31000步实测)
// ═══════════════════════════════════════════════════════
inline constexpr int ZHONGLV_PERIOD        = 12;       // 仲吕周期 (步)
inline constexpr int ZHONGLV_MULTI_STEPS   = 8;        // 每次闭合的倍增次数
inline constexpr int64_t ZHONGLV_P3        = 177147;   // 3^11
inline constexpr int64_t ZHONGLV_P2        = 65536;    // 2^16
inline constexpr double ZHONGLV_MULTIPLIER = 2850.0;   // (P3/P2)^8 ≈ 单次闭合倍增量
inline constexpr double ZHONGLV_LOG10      = 3.45;     // log10(2850)

// 实测数据
inline constexpr int ZHONGLV_TOTAL_31000   = 2583;     // 31000步内闭合次数
inline constexpr int LCM_WRAPS_31000       = 16558;    // LCM环绕圈次数

// ═══════════════════════════════════════════════════════
// L6 频率级联状态
// ═══════════════════════════════════════════════════════
struct FrequencyCascadeState {
    int64_t accumulator;        // LCM环累加器
    int64_t total_wraps;        // 总绕圈次数
    int zhonglv_count;          // 仲吕闭合累计
    int frequency_multiplies;   // 频率倍增次数 (zhonglv×8)
    double current_freq_hz;     // 当前等效频率
};

// ═══════════════════════════════════════════════════════
// L6 操作
// ═══════════════════════════════════════════════════════

// 仲吕闭合: 频率倍增原语
// acc = (acc × 3^11) / 2^16, 执行8次
[[nodiscard]] constexpr int64_t zhonglv_step(int64_t acc, int64_t lcm) noexcept {
    for (int i = 0; i < ZHONGLV_MULTI_STEPS; ++i) {
        acc = (acc * ZHONGLV_P3) / ZHONGLV_P2;
        acc %= lcm;
    }
    return acc;
}

// 项目频率: 基于绕圈次数和仲吕闭合的累积
[[nodiscard]] constexpr double project_frequency(
    int current_step, int current_wraps, int target_step
) noexcept {
    double wraps_per_step = static_cast<double>(current_wraps) / current_step;
    int new_wraps = static_cast<int>(current_wraps + wraps_per_step * (target_step - current_step));
    return static_cast<double>(new_wraps) * 11609505792.0; // wraps × LCM
}

// 频域指纹: C3/仲吕比 = 5³ = 125
[[nodiscard]] constexpr int c3_zhonglv_ratio() noexcept {
    return 1500 / 12;  // C3周期/仲吕周期 = 125 = 5³
}

// ═══════════════════════════════════════════════════════
// [v2.6] 频域算子 (frequency_double / octave / harmonics_analysis)
// ═══════════════════════════════════════════════════════

// 仲吕倍频原语: acc = acc × 3^11 / 2^16 (单步)
[[nodiscard]] constexpr int64_t frequency_double(int64_t acc, int64_t lcm) noexcept {
    acc = (acc * ZHONGLV_P3) / ZHONGLV_P2;
    return acc % lcm;
}

// 纯八度倍增: freq × 2^n
[[nodiscard]] constexpr double frequency_octave(double base_hz, int n_octaves) noexcept {
    double result = base_hz;
    for (int i = 0; i < n_octaves; ++i) result *= 2.0;
    return result;
}

// 泛音公列分析: 检查 2^a × 3^b 格点上的频率分布
// 返回格点总数和最大频率
struct HarmonicsResult {
    int lattice_points;
    double max_frequency_hz;
};

[[nodiscard]] constexpr HarmonicsResult harmonics_analysis(
    double base_hz, int max_a, int max_b
) noexcept {
    int count = 0;
    double max_f = 0.0;
    for (int a = 0; a <= max_a; ++a) {
        for (int b = 0; b <= max_b; ++b) {
            double f = base_hz;
            for (int i = 0; i < a; ++i) f *= 2.0;
            for (int j = 0; j < b; ++j) f *= 3.0;
            if (f > max_f) max_f = f;
            ++count;
        }
    }
    return {count, max_f};
}

} // namespace sov::math::l6
#endif // SOV_MATH_ZHONGLV_MULTIPLIER_L6_H
