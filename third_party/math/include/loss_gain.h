// loss_gain.h — 损益链 (层4: T⁶环面, 拓扑)
// [层4] [拓扑] 十二律长度格点序列 — 宪法固定, 非计算生成
// [层0] [模2] Sun/Yi操作 — 模2整数运算 (n×2/3, n×4/3)
// 范畴: 十二律损益链是S²/T⁶的几何拓扑结构, 不可用代数公式推导
//
// v2.0 实测: 损益链→泛音共振驱动, 非马尔可夫随机游走
//   C3孤子周期=1500步=12×125=十二律×5³
//   声子干涉取代能量函数, 驻波节点(tone%3==0)自然共振
#ifndef SOV_MATH_LOSS_GAIN_H
#define SOV_MATH_LOSS_GAIN_H

#include "lcm_constants.h"
#include <cstdint>

namespace sov::math::loss_gain {

// ============================================================================
// 一、十二律长度格点序列 (宪法固定, 不可替代)
// ============================================================================

// 黄钟基准: 81
// 损(Sun): 长度 × 2/3 (仅当 n×2 可被 3 整除)
// 益(Yi):  长度 × 4/3 (仅当 n×4 可被 3 整除)
// 十二阶损益链: Sun, Yi, Sun, Yi, Sun, Yi, Sun, Yi, Sun, Yi, Sun
inline constexpr uint64_t TWELVE_LENGTHS[12] = {
    81,   // 0  黄钟 (基准)
    54,   // 1  林钟 (损: 81×2/3)
    72,   // 2  太簇 (益: 54×4/3)
    48,   // 3  南吕 (损: 72×2/3)
    64,   // 4  姑洗 (益: 48×4/3)
    43,   // 5  应钟 ← 宪法截断 (代数sun(64)=42, 律制截断=43)
    57,   // 6  蕤宾 ← 宪法截断 (代数yi(43)=57.333, 律制截断=57)
    38,   // 7  大吕 (损: 57×2/3)
    51,   // 8  夷则 ← 宪法截断
    34,   // 9  夹钟 (损)
    45,   // 10 无射 ← 宪法截断
    30,   // 11 仲吕 (损: 45×2/3)
};

// 十二律名称
inline constexpr const char* TONE_NAMES[12] = {
    "黄钟", "林钟", "太簇", "南吕", "姑洗", "应钟",
    "蕤宾", "大吕", "夷则", "夹钟", "无射", "仲吕"
};

// ============================================================================
// 二、LCM 余数表 (十二律对 LCM 的余数)
// ============================================================================

// 每个长度格点值 × 黄钟(177147) / 仲吕边界(65536) 的商 — 即仲吕闭合后的余数
inline constexpr uint64_t LCM_REMAINDERS[12] = {
    177147,  // 0  黄钟
    118098,  // 1  林钟
    157464,  // 2  太簇
    104976,  // 3  南吕
    139968,  // 4  姑洗
    93312,   // 5  应钟
    124416,  // 6  蕤宾
    82944,   // 7  大吕
    110592,  // 8  夷则
    73728,   // 9  夹钟
    98304,   // 10 无射
    65536,   // 11 仲吕 — 触发闭合
};

// ═══════════════════════════════════════════════════════════════════════
// 损益操作 — 代数近似, 非宪法定义
// 警告: sun()/yi() 在不可通约点返回的值与宪法表 TWELVE_LENGTHS 不同!
//       始终使用 TWELVE_LENGTHS[] 宪法表, 不要用 sun()/yi() 推导律值!
// ═══════════════════════════════════════════════════════════════════════

// 损 (Sun): n × 2/3
// 仅当 n×2 能被 3 整除时合法
[[nodiscard]] constexpr bool can_sun(uint64_t n) noexcept {
    return (n * 2) % 3 == 0;
}

[[nodiscard]] constexpr uint64_t sun(uint64_t n) noexcept {
    return (n * 2) / 3;
}

// 益 (Yi): n × 4/3
// 仅当 n×4 能被 3 整除时合法
[[nodiscard]] constexpr bool can_yi(uint64_t n) noexcept {
    return (n * 4) % 3 == 0;
}

[[nodiscard]] constexpr uint64_t yi(uint64_t n) noexcept {
    return (n * 4) / 3;
}

// 编译期验证: TWELVE_LENGTHS 是宪法固定序列, 非计算生成
// sun()/yi() 仅用于理解和验证, 不替代宪法表
static_assert(TWELVE_LENGTHS[0] == 81,  "黄钟=81");
static_assert(TWELVE_LENGTHS[1] == 54,  "林钟=54 (损)");
static_assert(TWELVE_LENGTHS[2] == 72,  "太簇=72 (益)");
static_assert(TWELVE_LENGTHS[3] == 48,  "南吕=48 (损)");
static_assert(TWELVE_LENGTHS[4] == 64,  "姑洗=64 (益)");
static_assert(TWELVE_LENGTHS[5] == 43,  "应钟=43 ← 宪法截断, 非sun(64)=42");
static_assert(TWELVE_LENGTHS[6] == 57,  "蕤宾=57 ← 宪法截断");
static_assert(TWELVE_LENGTHS[7] == 38,  "大吕=38 (损)");
static_assert(TWELVE_LENGTHS[8] == 51,  "夷则=51 ← 宪法截断");
static_assert(TWELVE_LENGTHS[9] == 34,  "夹钟=34 (损)");
static_assert(TWELVE_LENGTHS[10] == 45, "无射=45 ← 宪法截断");
static_assert(TWELVE_LENGTHS[11] == 30, "仲吕=30 (损)");

// ============================================================================
// 四、损益链步进 (12步循环)
// ============================================================================

// 十二阶损益链: Sun, Yi, Sun, Yi, Sun, Yi, Sun, Yi, Sun, Yi, Sun
// 最后仲吕不交, 触发 LCM 闭合
enum class LossGain : uint8_t {
    SUN = 0,  // 损
    YI  = 1,  // 益
};

inline constexpr LossGain LOSS_GAIN_SEQUENCE[12] = {
    LossGain::SUN,  // 0
    LossGain::YI,   // 1
    LossGain::SUN,  // 2
    LossGain::YI,   // 3
    LossGain::SUN,  // 4
    LossGain::YI,   // 5
    LossGain::SUN,  // 6
    LossGain::YI,   // 7
    LossGain::SUN,  // 8
    LossGain::YI,   // 9
    LossGain::SUN,  // 10
    LossGain::SUN,  // 11 ← 仲吕不交
};

// 获取当前步的损益操作
[[nodiscard]] constexpr LossGain step_loss_gain(uint64_t step) noexcept {
    return LOSS_GAIN_SEQUENCE[step % 12];
}

// 获取当前步对应的十二律长度
[[nodiscard]] constexpr uint64_t step_length(uint64_t step) noexcept {
    return TWELVE_LENGTHS[step % 12];
}

// 获取当前步对应的 LCM 余数
[[nodiscard]] constexpr uint64_t step_lcm_remainder(uint64_t step) noexcept {
    return LCM_REMAINDERS[step % 12];
}

// ============================================================================
// 五、仲吕不交判定
// ============================================================================

// 仲吕位置: step % 12 == 11
// 此时极向(144域)与环向(46域)均无法归零 → 强制触发 LCM 闭合
[[nodiscard]] constexpr bool is_zhonglv_boundary(uint64_t step) noexcept {
    return (step % 12) == 11;
}

// 仲吕余数 = 65536 = 2^16 → 触发 (acc × 177147) >> 16
static_assert(LCM_REMAINDERS[11] == ZHONGLV_BOUNDARY,
    "仲吕余数 = 65536 = 2^16");

// ============================================================================
// 六、十二律频率比 (三分损益, 黄钟基准=177147)
// ============================================================================

inline constexpr uint64_t FREQ_RATIOS[12] = {
    177147,  // 黄钟
    118098,  // 林钟
    157464,  // 太簇
    104976,  // 南吕
    139968,  // 姑洗
    93312,   // 应钟
    124416,  // 蕤宾
    82944,   // 大吕
    110592,  // 夷则
    73728,   // 夹钟
    98304,   // 无射
    65536,   // 仲吕
};

// 隔八相生 (相位推进)
inline constexpr uint8_t PHASE_NEXT[12] = {
    1,   // 0  黄钟 → 林钟
    2,   // 1  林钟 → 太簇
    3,   // 2  太簇 → 南吕
    4,   // 3  南吕 → 姑洗
    5,   // 4  姑洗 → 应钟
    6,   // 5  应钟 → 蕤宾
    7,   // 6  蕤宾 → 大吕
    8,   // 7  大吕 → 夷则
    9,   // 8  夷则 → 夹钟
    10,  // 9  夹钟 → 无射
    11,  // 10 无射 → 仲吕
    0,   // 11 仲吕 → 黄钟 (仲吕闭合)
};

// ═══════════════════════════════════════════════════════
// [v2.6] 宪法级损益操作
// ═══════════════════════════════════════════════════════

// 宪法级损 (SUN): trit → (trit - 1) in C3 rotation (逆时针)
// T2→T1, T1→T0 (吸收!), T0→T2
[[nodiscard]] constexpr uint8_t constitutional_sun(uint8_t trit) noexcept {
    return (trit + 2) % 3;  // (trit - 1) mod 3
}

// 宪法级益 (YI): trit → (trit + 1) in C3 rotation (顺时针)
[[nodiscard]] constexpr uint8_t constitutional_yi(uint8_t trit) noexcept {
    return (trit + 1) % 3;
}

// 编译期验证: 损益互逆
static_assert(constitutional_sun(constitutional_yi(0)) == 0, "SUN∘YI = identity");
static_assert(constitutional_sun(1) == 0, "SUN(1)=0 — absorption");
static_assert(constitutional_yi(2) == 0, "YI(2)=0 — cycle closure");

} // namespace sov::math::loss_gain

#endif // SOV_MATH_LOSS_GAIN_H
