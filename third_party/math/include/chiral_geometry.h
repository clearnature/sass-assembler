// gf3_conjugacy.h — 层3: 手征共轭 (Chirality Conjugacy)
//
// 宪法声明:
//   手征共轭 = C3左旋(+1) ↔ C3右旋(+2) 的交换
//   这是主权状态机在 S²/A₄ 纤维上的手性对偶
//   代数实现: ω ↔ ω² (复共轭), T1 ↔ T2 (GF(3)加法群自同构)
//   几何本源: 扭量在 T⁶ 环面上的 Christoffel 平行移动
//   范畴: 层3 五行模数区 — 手性离合器的动力学
#ifndef SOV_MATH_GF3_CONJUGACY_H
#define SOV_MATH_GF3_CONJUGACY_H

#include "gf3_types.h"
#include "fixed_complex.h"
#include <cstdint>

namespace sov::math::chiral {

// ═══════════════════════════════════════════════════════════════════════
// 一、手征共轭: 左旋(+1) ↔ 右旋(+2)
// ═══════════════════════════════════════════════════════════════════════

// [层3] [手性] C3 左旋: T0→T1→T2→T0 (顺时针, CW)
[[nodiscard]] constexpr uint8_t c3_cw(uint8_t t) noexcept {
    return (t + 1) % 3;
}

// [层3] [手性] C3 右旋: T0→T2→T1→T0 (逆时针, CCW)
[[nodiscard]] constexpr uint8_t c3_ccw(uint8_t t) noexcept {
    return (t + 2) % 3;
}

// [层3] [手性] 手征共轭: 交换左右旋方向
// CW ↔ CCW: cw(ccw(t)) = t, ccw(cw(t)) = t
static_assert(c3_cw(c3_ccw(0)) == 0, "CW∘CCW = id");
static_assert(c3_ccw(c3_cw(0)) == 0, "CCW∘CW = id");

// [层3] [手性] 共轭元素对: T1(T0→T1是CW第一步) ↔ T2(T0→T2是CCW第一步)
// 在GF(3)加法群中: T1 + T2 = 0 (互为加法逆元)
// 这是手征对偶在代数层(层1 GF(3))的投影
[[nodiscard]] constexpr uint8_t chiral_conj(uint8_t t) noexcept {
    constexpr uint8_t CHIRAL_CONJ[3] = {0, 2, 1};  // T0自共轭, T1↔T2
    return CHIRAL_CONJ[t];
}

// [层3] [手性] 自共轭元: 只有 T0 (空寂态) 是自共轭的
// T1和T2互为手征共轭对 — 它们代表相反的手性方向
[[nodiscard]] constexpr bool is_chiral_self_conj(uint8_t t) noexcept {
    return t == 0;
}

// ═══════════════════════════════════════════════════════════════════════
// 二、Z[ω] 复共轭: ω(CW生成元) ↔ ω²(CCW生成元)
// ═══════════════════════════════════════════════════════════════════════

// [层3] [Z[ω]] Q16Complex 以标准基 {1, i} 存储: z = re + im·i
// ω  = -1/2 + i√3/2 = (-32768, 56753) Q16
// ω² = -1/2 - i√3/2 = (-32768, -56753) Q16
// 复共轭: z* = re - im·i, 自动交换 ω↔ω²
[[nodiscard]] constexpr fixed_complex::Q16Complex conj(
    fixed_complex::Q16Complex z
) noexcept {
    return {z.re, (int32_t)(-z.im)};  // 标准复共轭: (a+bi)* = a-bi
}

// [层3] [Z[ω]] 验证: conj(ω) = ω²
static_assert(fixed_complex::OMEGA_IM_Q16 == 56753,  "ω虚部 = +√3/2");
static_assert(fixed_complex::OMEGA2_IM_Q16 == -56753, "ω²虚部 = -√3/2");

// [层3] [Z[ω]] 范数: N(z) = |z|² = re² + im²
[[nodiscard]] constexpr int32_t norm(fixed_complex::Q16Complex z) noexcept {
    int64_t re2 = ((int64_t)z.re * z.re) >> 16;
    int64_t im2 = ((int64_t)z.im * z.im) >> 16;
    return (int32_t)(re2 + im2);
}

// [层3] [Z[ω]] Q16截断: (-32768)²>>16=16384, 56753²>>16≈49147, 和=65531
// 与理论值65536差5 = Q16截断容忍, ω和ω²范数相等(手征对称)
static_assert(norm(fixed_complex::Q16Complex::omega())  >= 65530, "N(ω)≥1-ε");
static_assert(norm(fixed_complex::Q16Complex::omega2()) >= 65530, "N(ω²)≥1-ε");

// ═══════════════════════════════════════════════════════════════════════
// 三、五行振幅中的手征共轭
// ═══════════════════════════════════════════════════════════════════════

// [层3] [五行] 相克(ω) ↔ 克制(ω²) = 手征共轭对
static_assert(fixed_complex::AMP_OVERCOME.re == fixed_complex::AMP_OVERCOME2.re,
    "相克与克制 实部相等 (手征对称)");
static_assert(fixed_complex::AMP_OVERCOME.im > 0 && fixed_complex::AMP_OVERCOME2.im < 0,
    "相克(CW)与克制(CCW) 虚部相反 = 手征共轭");

// [层3] [五行] 相生(+1) = 手征单态 (实数, 自共轭)
static_assert(fixed_complex::AMP_GENERATE.im == 0,
    "相生振幅是实的, 无手征倾向");

// ═══════════════════════════════════════════════════════════════════════
// 四、手性离合器状态 (层3 五行相变动力学)
// ═══════════════════════════════════════════════════════════════════════

// [层3] [手性] 离合器耦合状态
enum class ChiralCoupling : uint8_t {
    IDLE       = 0,  // 空转: 无手征倾向 (a=0, 火行)
    MESHED     = 1,  // 啮合: 左右旋对偶互嵌 (a=1, 土行)
    HALF_LINK  = 2,  // 半联动: 手征振幅不对称 (a=3, 金行)
    SLIPPING   = 3,  // 打滑: 反射对称丢失 (a=4, 水行)
    DECOUPLED  = 4,  // 分离: 手征完全分离 (a=6, 木行)
};

// [层3] [手性] 环向幂次 a → 离合器状态
[[nodiscard]] constexpr ChiralCoupling coupling_from_power(int a) noexcept {
    if (a == 0) return ChiralCoupling::IDLE;
    if (a == 1) return ChiralCoupling::MESHED;
    if (a == 3) return ChiralCoupling::HALF_LINK;
    if (a == 4) return ChiralCoupling::SLIPPING;
    return ChiralCoupling::DECOUPLED;  // a>=6
}

// [层3] [手性] 手征共轭在五行相生链中的传播
// 火(a=0,无手征)→土(a=1,对偶)→金(a=3,不对称)→水(a=4,丢失)→木(a=6,分离)
[[nodiscard]] constexpr bool is_chiral_separated(int a) noexcept {
    return a >= 4;  // a≥4 手征完全分离
}

} // namespace sov::math::chiral

#endif // SOV_MATH_GF3_CONJUGACY_H
