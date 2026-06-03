// fixed_complex.h — Q16.16 定点复数 (层3: 五行模数区)
// [层0] [模2Q16] 定点格式: 1位符号+15位整数+16位小数=int32_t
// [层3] [五行] omega=e^{2πi/3}, Z[omega]环中的相生/相克振幅
// 范畴: 五行干涉是层3的手性离合器相变动力学, omega是其在Z[omega]中的代数表示
#ifndef SOV_MATH_FIXED_COMPLEX_H
#define SOV_MATH_FIXED_COMPLEX_H

#include "lcm_constants.h"
#include <cstdint>
#include <array>

namespace sov::math::fixed_complex {

// ============================================================================
// 一、Q16.16 定点格式
// ============================================================================

// Q16.16: 1位符号 + 15位整数 + 16位小数 = int32_t
// 缩放因子: 2^16 = 65536
inline constexpr int32_t Q16_ONE  = 65536;   // 1.0
inline constexpr int32_t Q16_HALF = 32768;   // 0.5
inline constexpr int32_t Q16_ZERO = 0;

// Q16 定点乘法: (a * b) >> 16
[[nodiscard]] constexpr int32_t q16_mul(int32_t a, int32_t b) noexcept {
    return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}

// Q16 定点加法
[[nodiscard]] constexpr int32_t q16_add(int32_t a, int32_t b) noexcept {
    return a + b;
}

// ============================================================================
// 二、omega = e^{2πi/3} = -1/2 + i√3/2  (Q16.16 定点)
// ============================================================================

// omega 是三次单位根: ω³ = 1, 1 + ω + ω² = 0
// 实部: -1/2 → -32768
// 虚部: +√3/2 → +28316 (√3×65536/2 = 113506/2 = 56753)
inline constexpr int32_t OMEGA_RE_Q16 = -32768;   // -0.5 × 65536
inline constexpr int32_t OMEGA_IM_Q16 =  56753;   // +√3/2 × 65536

// omega² = -1/2 - i√3/2
inline constexpr int32_t OMEGA2_RE_Q16 = -32768;  // -0.5 × 65536
inline constexpr int32_t OMEGA2_IM_Q16 = -56753;  // -√3/2 × 65536

// 验证: ω + ω² + 1 = 0 (编译期)
static_assert(OMEGA_RE_Q16 + OMEGA2_RE_Q16 + Q16_ONE == 0,
    "ω + ω² + 1 = 0 (实部)");
static_assert(OMEGA_IM_Q16 + OMEGA2_IM_Q16 == 0,
    "ω + ω² + 1 = 0 (虚部)");

// ============================================================================
// 三、定点复数结构
// ============================================================================

struct Q16Complex {
    int32_t re;  // 实部 Q16.16
    int32_t im;  // 虚部 Q16.16

    [[nodiscard]] static constexpr Q16Complex zero() noexcept { return {0, 0}; }
    [[nodiscard]] static constexpr Q16Complex one()  noexcept { return {Q16_ONE, 0}; }
    [[nodiscard]] static constexpr Q16Complex omega() noexcept { return {OMEGA_RE_Q16, OMEGA_IM_Q16}; }
    [[nodiscard]] static constexpr Q16Complex omega2() noexcept { return {OMEGA2_RE_Q16, OMEGA2_IM_Q16}; }

    // 加法
    [[nodiscard]] constexpr Q16Complex operator+(Q16Complex other) const noexcept {
        return {re + other.re, im + other.im};
    }

    // 乘法: (a+bi)(c+di) = (ac-bd) + (ad+bc)i
    [[nodiscard]] constexpr Q16Complex operator*(Q16Complex other) const noexcept {
        return {
            q16_mul(re, other.re) - q16_mul(im, other.im),
            q16_mul(re, other.im) + q16_mul(im, other.re)
        };
    }

    // 范数平方: a² + b²
    [[nodiscard]] constexpr int32_t norm_sq() const noexcept {
        return q16_mul(re, re) + q16_mul(im, im);
    }
};

// 编译期: ω³ = 1
// Q16乘法有累积截断误差, 用代数恒等式验证: ω² + ω + 1 = 0
consteval bool verify_omega_cubed() {
    Q16Complex w = Q16Complex::omega();
    Q16Complex w2 = w * w;     // ω²
    Q16Complex sum = w2 + w + Q16Complex::one();
    // ω² + ω + 1 = 0, Q16容忍 ±16
    return (sum.re >= -16 && sum.re <= 16)
        && (sum.im >= -16 && sum.im <= 16);
}
static_assert(verify_omega_cubed(), "ω² + ω + 1 = 0 (Q16.16)");

// ============================================================================
// 四、五行干涉振幅 (相生/相克)
// ============================================================================

// 相生 Generate: +1 + 0i
inline constexpr Q16Complex AMP_GENERATE = Q16Complex::one();

// 相克 Overcome: ω = -0.5 + 0.866i
inline constexpr Q16Complex AMP_OVERCOME = Q16Complex::omega();

// 相克² Overcome²: ω² = -0.5 - 0.866i
inline constexpr Q16Complex AMP_OVERCOME2 = Q16Complex::omega2();

// 五行干涉表 [5][5]
// 五行: 木=0, 火=1, 土=2, 金=3, 水=4
// 相生→+1, 相克→ω, 克制→ω²
consteval auto generate_wuxing_table() {
    // 相生: 木→火, 火→土, 土→金, 金→水, 水→木
    // 相克: 木→土, 土→水, 水→火, 火→金, 金→木
    std::array<std::array<Q16Complex, 5>, 5> t{};

    // 初始化为 ω² (默认克制态)
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j)
            t[i][j] = AMP_OVERCOME2;

    // 对角线: +1 (自身)
    for (int i = 0; i < 5; ++i) t[i][i] = AMP_GENERATE;

    // 相生: (i, (i+1)%5) = +1
    t[0][1] = AMP_GENERATE;  // 木→火
    t[1][2] = AMP_GENERATE;  // 火→土
    t[2][3] = AMP_GENERATE;  // 土→金
    t[3][4] = AMP_GENERATE;  // 金→水
    t[4][0] = AMP_GENERATE;  // 水→木

    // 相克: (i, (i+2)%5) = ω
    t[0][2] = AMP_OVERCOME;  // 木克土
    t[1][3] = AMP_OVERCOME;  // 火克金
    t[2][4] = AMP_OVERCOME;  // 土克水
    t[3][0] = AMP_OVERCOME;  // 金克木
    t[4][1] = AMP_OVERCOME;  // 水克火

    return t;
}
inline constexpr auto WUXING_AMPLITUDE = generate_wuxing_table();

// 编译期: 相生+相克+克制 = 0 (1 + ω + ω² = 0)
static_assert((AMP_GENERATE + AMP_OVERCOME + AMP_OVERCOME2).re == 0,
    "1 + ω + ω² = 0 (实部)");

// ============================================================================
// ============================================================================
// 五、能隙 Δ = |ω - 1| = √3  (Q16.16)
// ============================================================================

// |ω - 1|² = (ω-1)(ω²-1) = 3
// 定点: √3 × 65536 = 113506 (定义在 lcm_constants.h)
// 此模块直接使用 sov::math::DELTA_Q16

// 验证: |ω - 1|² = 3
consteval bool verify_delta_via_omega() {
    int32_t diff_re = OMEGA_RE_Q16 - Q16_ONE;
    int32_t diff_im = OMEGA_IM_Q16;
    // norm_sq = diff_re²/65536 + diff_im²/65536 (Q16乘后>>16)
    int32_t nsq = (int32_t)(
        ((int64_t)diff_re * diff_re + (int64_t)diff_im * diff_im) >> 16
    );
    // 期望: nsq = 3 × 65536 = 196608, Q16误差 ±16
    return (nsq >= 196592 && nsq <= 196624);
}
static_assert(verify_delta_via_omega(), "|ω-1|² = 3 (Q16.16)");

} // namespace sov::math::fixed_complex

#endif // SOV_MATH_FIXED_COMPLEX_H
