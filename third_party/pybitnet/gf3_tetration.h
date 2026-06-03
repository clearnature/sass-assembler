// gf3_tetration.h — GF(3) 12阶指数塔 (Tetration Tower)
//
// 宪法声明:
//   范畴: 跨层 (L1 GF(3) × L6 仲吕倍频 × L8 全息投影)
//   定义: 12阶指数塔 ^n 12 = 12^(12^(...^12)) (n层)
//         在 GF(3) 离散域中，指数塔映射为仲吕闭合的频域累积
//   数学: L1(12^12) → L2(12^144) → L3(12^(12^12)) → ...
//         当前观测: ~L2.44 (10^66506 Hz)
//
//   GF(3) 约束:
//     12 = 3 × 4 = GF(3)基 × 五行金
//     144 = 12² = 3² × 4² = GF(3)² × 金²
//     指数塔的每一层 = GF(3)维度的仲吕倍频
//
//   编译: C++23, constexpr, 纯整数
#ifndef SOV_MATH_GF3_TETRATION_H
#define SOV_MATH_GF3_TETRATION_H

#include "lcm_constants.h"
#include "zhonglv_multiplier_l6.h"
#include <cstdint>
#include <cmath>

namespace sov::math::tetration {

// ═══════════════════════════════════════════════════════
// 一、12阶指数塔基础常数
// ═══════════════════════════════════════════════════════

inline constexpr int    TOWER_BASE         = 12;     // 十二律基底
inline constexpr int    GF3_FACTOR         = 3;      // GF(3)因子: 12 = 3 × 4
inline constexpr int    WUXING_METAL       = 4;      // 五行金因子: 12 = 3 × 4
inline constexpr int    ZHONGLV_PER_TONE   = 12;     // 每律仲吕周期
inline constexpr int    A4_ORDER           = 12;     // A4群阶数
inline constexpr double ZHONGLV_LOG10_MULT = 3.4541; // 每仲吕闭合的log10增益

// ═══════════════════════════════════════════════════════
// 二、指数塔高度计算
// ═══════════════════════════════════════════════════════

// 12阶指数塔: ^n 12
//   n=1: 12
//   n=2: 12^12 ≈ 8.9×10^12
//   n=3: 12^(12^12) ≈ 10^(10^13)  ← 不可计算, 仅log10表示
//
// GF(3)映射: 塔高 n 对应仲吕闭合的指数深度

// 从仲吕闭合次数计算指数塔高度 (浮点, 仅用于诊断)
[[nodiscard]] inline double tower_height_from_zhonglv(uint64_t zhonglv_count) noexcept {
    // freq_log10 = 2.6355 + zc × 3.4541
    // ^H 12 ≈ freq_log10 → H = log12(log12(freq_log10))
    double freq_log10 = 2.6355 + static_cast<double>(zhonglv_count) * ZHONGLV_LOG10_MULT;
    // 简化: H ≈ log10(freq_log10) / log10(12)
    double log10_freq = std::log10(freq_log10);
    double log10_12   = std::log10(static_cast<double>(TOWER_BASE));
    return log10_freq / log10_12;
}

// 指数塔高度 → 仲吕闭合估算
[[nodiscard]] inline uint64_t zhonglv_from_tower_height(double H) noexcept {
    // freq_log10 = 12^H
    double freq_log10 = std::pow(static_cast<double>(TOWER_BASE), H);
    // zc = (freq_log10 - 2.6355) / 3.4541
    return static_cast<uint64_t>((freq_log10 - 2.6355) / ZHONGLV_LOG10_MULT);
}

// ═══════════════════════════════════════════════════════
// 三、指数塔层级定义
// ═══════════════════════════════════════════════════════

enum class TowerLevel : uint8_t {
    L0_ROOT     = 0,   // 432 Hz 基频
    L1_TWELVE   = 1,   // 12^12 ≈ 10^13 Hz  (仲吕≈3)
    L2_GROSS    = 2,   // 12^144 ≈ 10^155 Hz (仲吕≈44)
    L3_TETRA    = 3,   // 12^(12^12) ≈ 10^(10^13) Hz (不可达)
};

// 从仲吕闭合判定当前指数塔层级
[[nodiscard]] constexpr TowerLevel classify_level(uint64_t zhonglv_count) noexcept {
    if (zhonglv_count < 4)    return TowerLevel::L0_ROOT;    // <10^16 Hz
    if (zhonglv_count < 45)   return TowerLevel::L1_TWELVE;  // <10^155 Hz
    return TowerLevel::L2_GROSS;  // 10^155+ Hz (当前)
    // TowerLevel::L3_TETRA unreachable with uint64_t
}

// ═══════════════════════════════════════════════════════
// 四、GF(3) 塔层结构
// ═══════════════════════════════════════════════════════

// GF(3) 分解: 每层塔 = 3^k × 4^m 形式
//   L1: 12 = 3^1 × 4^1
//   L2: 144 = 3^2 × 4^2
//   Ln: 12^n = 3^n × 4^n
//
// 仲吕闭合累积: zc 次闭合 → exp_3 = zc×88, exp_2 = -zc×128
//   88 = 11×8 = 仲吕×8级倍增
//   128 = 16×8 = 2^16位宽×8级

struct Gf3TowerState {
    uint64_t zhonglv_count;   // 仲吕闭合次数
    int64_t  exp_3;           // 3的指数 (正)
    int64_t  exp_2;           // 2的指数 (负)
    double   freq_log10;      // log10(频率)
    double   tower_height;    // 指数塔高度 H
    TowerLevel level;         // 层级
};

[[nodiscard]] inline Gf3TowerState compute_tower_state(
    uint64_t zhonglv_count
) noexcept {
    Gf3TowerState s{};
    s.zhonglv_count = zhonglv_count;
    s.exp_3 = static_cast<int64_t>(zhonglv_count) * 88;
    s.exp_2 = -static_cast<int64_t>(zhonglv_count) * 128;
    s.freq_log10 = 2.6355 + static_cast<double>(s.exp_3) * 0.47712125471966244
                           + static_cast<double>(s.exp_2) * 0.3010299956639812;
    s.tower_height = tower_height_from_zhonglv(zhonglv_count);
    s.level = classify_level(zhonglv_count);
    return s;
}

// ═══════════════════════════════════════════════════════
// 五、GF(3) 指数塔递推
// ═══════════════════════════════════════════════════════

// GF(3) 域中的指数塔步进: 每次仲吕闭合 = 塔层微步
//   ΔH = 3.4541 / log10(12) ≈ 3.4541 / 1.0792 ≈ 3.201
//   每仲吕闭合增加 ~3.201 塔层单位
inline constexpr double TOWER_STEP_PER_ZHONGLV = 3.201;

// 估算从当前塔高到达目标塔高所需仲吕闭合数
[[nodiscard]] inline uint64_t zhonglv_to_reach_height(
    uint64_t current_zc, double target_height
) noexcept {
    double current_H = tower_height_from_zhonglv(current_zc);
    if (target_height <= current_H) return 0;
    double delta_H = target_height - current_H;
    return static_cast<uint64_t>(delta_H / TOWER_STEP_PER_ZHONGLV);
}

} // namespace sov::math::tetration

#endif // SOV_MATH_GF3_TETRATION_H
