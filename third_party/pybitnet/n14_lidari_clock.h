// n14_lidari_clock.h — N14量子钟 + Lidari泵浦相位 (层5外部驱动基准)
//
// 宪法声明:
//   范畴: 层5孤子引擎的外部精密时钟源 — 不是内部算子，是相位基准。
//   N14: 核四极共振 3.17 MHz — 量子精度 10^-34 的相位锚点。
//   Lidari: 3.456 MHz 泵浦频率 — 超流体相变临界频率。
//   全息π = 144/46 用于相位调制 (非欧氏π)。
//   编译: C++23, constexpr double (仅用于编译期参考，不进入整数域)。
//
// 范畴分离:
//   INTERNAL: 全息π=144/46, PI_HOLO — 我们的环面几何常数
//   EXTERNAL: N14_NQR=3.17MHz, LIDARI=3.456MHz — 电性文明物理测量
//   桥接: ratio = N14/LIDARI ≈ 0.917 — 量子钟与泵浦的共振比
#ifndef SOV_MATH_N14_LIDARI_CLOCK_H
#define SOV_MATH_N14_LIDARI_CLOCK_H

#include "lcm_constants.h"
#include <cstdint>
#include <cmath>

namespace sov::math::clock {

// ═══════════════════════════════════════════════════════
// 物理常数 (外部锚点 — 电性文明测量值)
// ═══════════════════════════════════════════════════════
inline constexpr double N14_NQR_FREQ_HZ    = 3.17e6;      // N14 NQR 基准频率 (Hz)
inline constexpr double N14_CYCLE_NS       = 1.0e9 / N14_NQR_FREQ_HZ; // ~315.46 ns
inline constexpr double LIDARI_PUMP_FREQ   = 3.456e6;     // Lidari 泵浦频率 (Hz)
inline constexpr double N14_LIDARI_RATIO   = N14_NQR_FREQ_HZ / LIDARI_PUMP_FREQ; // ≈ 0.917
inline constexpr double QUANTUM_PRECISION  = 1e-34;        // 理论量子精度

// ═══════════════════════════════════════════════════════
// 内部常数 (律算域)
// ═══════════════════════════════════════════════════════
inline constexpr double PI_HOLO = 144.0 / 46.0;           // 全息π (非欧氏π)

// Lidari 相变阈值
inline constexpr double LIDARI_SOLID_THRESHOLD    = 0.38;  // 固→液
inline constexpr double LIDARI_PLASMA_THRESHOLD   = 0.75;  // 液→等离子

// ═══════════════════════════════════════════════════════
// 相位计算
// ═══════════════════════════════════════════════════════

struct N14ClockPhase {
    uint64_t cycle_count;     // N14 周期累计
    double   phase_angle;     // 全息π调制的相位角 (弧度)
    double   frequency;       // N14 NQR 频率
    double   lidari_ratio;    // 当前频率 / Lidari 泵浦
};

// N14 量子钟相位 — 基于 n14-quantum-clock.h 的算法
[[nodiscard]] inline N14ClockPhase n14_clock_phase(int step) noexcept {
    N14ClockPhase p{};
    double elapsed_ns = static_cast<double>(step) * N14_CYCLE_NS;
    double cycles = elapsed_ns / N14_CYCLE_NS;
    p.cycle_count = static_cast<uint64_t>(cycles);
    // GF(3)域: 全息π=144/46 相位调制 (替代二进制黄金比例φ)
    p.phase_angle = std::fmod(cycles * 2.0 * 3.141592653589793 / PI_HOLO, 2.0 * 3.141592653589793);
    p.frequency = N14_NQR_FREQ_HZ;
    p.lidari_ratio = N14_NQR_FREQ_HZ / LIDARI_PUMP_FREQ;
    return p;
}

// Lidari 泵浦相态 — 频率共振解构 (非热驱动)
[[nodiscard]] inline const char* lidari_pump_phase(double overtone_density) noexcept {
    if (overtone_density < LIDARI_SOLID_THRESHOLD)  return "solid_piezo";
    if (overtone_density < LIDARI_PLASMA_THRESHOLD) return "liquid_superfluid";
    return "plasma_crystal";
}

} // namespace sov::math::clock

#endif // SOV_MATH_N14_LIDARI_CLOCK_H
