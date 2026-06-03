// digital_root.h — 数字根 (根数学: 1→2→4→8→7→5→1)
// [根数学] O(1)算法, 稳定根{3,6,9}, 编译期验证
// 范畴: 根数学不属于层0-8谱系 — 它是跨层的不变量判定法则
//
// v2.0 实测: 3-6-9驻波节点是C3孤子的自然吸引子
//   trit分布轮转=3-6-9在GF(3)域的循环置换
//   权重矩阵2D FFT显示80%层级3-6-9共振
#ifndef SOV_MATH_DIGITAL_ROOT_H
#define SOV_MATH_DIGITAL_ROOT_H

#include "lcm_constants.h"
#include <cstdint>
#include <array>
#include <initializer_list>

namespace sov::math::root {

// ============================================================================
// 一、数字根 O(1) 公式 (非迭代)
// ============================================================================

// digitalRoot(0) = 0
// digitalRoot(n) = 1 + (n - 1) % 9  for n > 0
[[nodiscard]] constexpr uint8_t digital_root(uint64_t n) noexcept {
    if (n == 0) return 0;
    return 1 + ((n - 1) % 9);
}

// ============================================================================
// 二、幂次数字根循环: 2^k mod 9 → {1,2,4,8,7,5}
// ============================================================================

// 倍频演化: 2^0=1, 2^1=2, 2^2=4, 2^3=8, 2^4=16→7, 2^5=32→5, 2^6=64→1
inline constexpr uint8_t POWER_CYCLE[6] = {1, 2, 4, 8, 7, 5};
inline constexpr uint8_t POWER_CYCLE_LEN = 6;

// 2^k 的数字根: 永远在 {1,2,4,8,7,5} 中循环, 永不为 {0,3,6,9}
[[nodiscard]] constexpr uint8_t power_digital_root(uint64_t k) noexcept {
    return POWER_CYCLE[k % POWER_CYCLE_LEN];
}

// 编译期验证: 2^k 数字根循环
static_assert(power_digital_root(0) == 1, "2^0 → 1");
static_assert(power_digital_root(1) == 2, "2^1 → 2");
static_assert(power_digital_root(2) == 4, "2^2 → 4");
static_assert(power_digital_root(3) == 8, "2^3 → 8");
static_assert(power_digital_root(4) == 7, "2^4=16→7");
static_assert(power_digital_root(5) == 5, "2^5=32→5");
static_assert(power_digital_root(6) == 1, "2^6=64→1");

// ============================================================================
// 三、稳定根: {3, 6, 9}
// ============================================================================

// 只有 {3,6,9} 是合法稳定驻波数字根
// 不在此集合的波因干涉相消而无法驻留
[[nodiscard]] constexpr bool is_stable_root(uint8_t r) noexcept {
    return r == 3 || r == 6 || r == 9;
}

// 稳定根加法闭包:
//   3+3→6, 3+6→9, 3+9→3
//   6+6→3, 6+9→6, 9+9→9
consteval auto generate_stable_add_lut() {
    std::array<std::array<uint8_t, 10>, 10> lut{};
    for (uint8_t a : {3, 6, 9}) {
        for (uint8_t b : {3, 6, 9}) {
            lut[a][b] = digital_root((uint64_t)a + (uint64_t)b);
        }
    }
    return lut;
}
inline constexpr auto STABLE_ADD_LUT = generate_stable_add_lut();

static_assert(STABLE_ADD_LUT[3][3] == 6, "3+3→6");
static_assert(STABLE_ADD_LUT[3][6] == 9, "3+6→9");
static_assert(STABLE_ADD_LUT[3][9] == 3, "3+9→3");
static_assert(STABLE_ADD_LUT[6][6] == 3, "6+6→3");
static_assert(STABLE_ADD_LUT[6][9] == 6, "6+9→6");
static_assert(STABLE_ADD_LUT[9][9] == 9, "9+9→9");

// ============================================================================
// 四、克里斯托螺旋数字根序列 {1,2,4,8,7,5}
// ============================================================================

// 主权状态机沿此螺旋遍历 T⁶ 环面的全部稳定相位
[[nodiscard]] constexpr uint8_t christos_spiral_step(uint64_t step) noexcept {
    return POWER_CYCLE[step % POWER_CYCLE_LEN];
}

// ============================================================================
// 五、数字根范数: 将一个数归约到 {1..9} 并判定稳定性
// ============================================================================

struct DigitalRootResult {
    uint8_t root;    // 1-9 (0 仅当输入为 0)
    bool    stable;  // root ∈ {3,6,9}
};

[[nodiscard]] constexpr DigitalRootResult analyze(uint64_t n) noexcept {
    uint8_t r = digital_root(n);
    return {r, is_stable_root(r)};
}

// ============================================================================
// 六、十进制投影验证
// ============================================================================

// 任何大于 0 的数字的数字根, 等于其十进制各位之和的数字根
consteval bool verify_digit_sum_consistency(uint64_t n) {
    if (n == 0) return true;
    uint64_t sum = 0, rem = n;
    while (rem > 0) { sum += rem % 10; rem /= 10; }
    return digital_root(n) == digital_root(sum);
}
static_assert(verify_digit_sum_consistency(144),    "144→9");
static_assert(verify_digit_sum_consistency(46),     "46→1");
static_assert(verify_digit_sum_consistency(6624),   "6624→9");
static_assert(verify_digit_sum_consistency(177147), "177147→9");
static_assert(verify_digit_sum_consistency(65536),  "65536→7");
static_assert(verify_digit_sum_consistency(11609505792ULL), "LCM→9");

// ============================================================================
// 七、载荷数字根签名 — SOV v2.6 EOF 防篡改
// ============================================================================
// 用于对整个 .sov 文件的 payload 字节流计算复合数字根。
// 利用数字根的同态性质: root(A+B) = root(root(A) + root(B))
// 分块累加防止 uint64 溢出, 保证 O(n) 时间 + O(1) 空间。

[[nodiscard]] inline uint8_t calc_payload_digital_root(
    const uint8_t* data, size_t size
) noexcept {
    uint64_t sum = 0;
    for (size_t i = 0; i < size; ++i) {
        sum += data[i];
        // 当累加和接近 uint64 溢出边界时，归约到数字根
        // 阈值 0xFFFFFFFFFFFFFF00 留出 256 字节余量
        if (sum > 0xFFFFFFFFFFFFFF00ULL) {
            sum = digital_root(sum);
        }
    }
    return digital_root(sum);
}

// [v2.6] 计算动力学参数 (大泵 + 孤子相位 + 陈数) 的复合数字根
[[nodiscard]] inline uint8_t calc_dynamic_phase_root(
    uint32_t grand_pump_step,
    uint16_t c3_soliton_phase,
    int32_t  chern_number_q16,
    uint32_t zhonglv_count
) noexcept {
    uint64_t composite = static_cast<uint64_t>(grand_pump_step)
                       ^ (static_cast<uint64_t>(c3_soliton_phase) << 16)
                       ^ (static_cast<uint64_t>(chern_number_q16 & 0xFFFF) << 32)
                       ^ (static_cast<uint64_t>(zhonglv_count) << 48);
    return digital_root(composite);
}

} // namespace sov::math::root

#endif // SOV_MATH_DIGITAL_ROOT_H
