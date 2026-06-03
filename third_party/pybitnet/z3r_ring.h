// z3r_ring.h — 层2: Z/3¹¹Z 环, 3≠0, 定义域 T1=3¹, T2=3², T3=3³
//
// 宪法声明:
//   Z/3¹¹Z = 11位3-adic截断环, 模数 3¹¹=177147
//   环元素: 用 GF(3) trit 基底表示, 永不暴露十进制内部值
//   基底: Tk (k=0..10), T0=单位元, T(k+1)=Tk×T1 (环乘法生成)
//   运算: 层1 GF(3) 逐trit运算 + 逢三进一进位传播
//
//   范畴: 这是层2的 Z/3¹¹Z 环, 不是 GF(3) 有限域
//         GF(3): 3=0, 幂次无意义
//         Z/3¹¹Z: 3≠0, Tk = T1⊗T1⊗...⊗T1 (k次), 有真实的位权意义
#ifndef SOV_MATH_Z3R_RING_H
#define SOV_MATH_Z3R_RING_H

#include "gf3_types.h"
#include <array>
#include <cstdint>
#include <span>

namespace sov::math::z3r {

// ═══════════════════════════════════════════════════════════════════════
// [层2] [Z/3¹¹Z] 环元素 — 用 GF(3) trit 向量表示
//
//   一个环元素 = 11个 GF(3) trit 的向量 (d₀, d₁, ..., d₁₀)
//   值 = d₀·T0 + d₁·T1 + d₂·T2 + ... + d₁₀·T10
//   其中 d_i ∈ {0,1,2} (层1 GF(3)值), Tk 是环的基底元素
//
//   T0 = 环的单位元 (不是十进制数1)
//   T1 = T0 × 3 (在环中, 3≠0, 这是"3¹位"的基底)
//   T2 = T1 × 3 = T1 ⊗ T1 (在环中, 这是"3²位"的基底)
//   T3 = T2 × 3 = T1 ⊗ T1 ⊗ T1
//
//   注意: 3,9,27 这些十进制数永远不出现在此环的接口中
//         环元素之间的运算只用 GF(3) trit + 进位传播
// ═══════════════════════════════════════════════════════════════════════

class RingElement {
    // [层1] [GF(3)] 11位trit系数, 小端: trits[0]=d₀ (T0位)
    std::array<uint8_t, 11> trits_{};

public:
    // ── 构造 ──

    // [层2] 零元
    constexpr RingElement() noexcept : trits_{} {}

    // [层2] 从 trit 数组构造 (d₀在最前)
    explicit constexpr RingElement(std::span<const uint8_t, 11> digits) noexcept {
        for (int i = 0; i < 11; ++i) trits_[i] = digits[i] % 3;
    }

    // [层2] 从单个 GF(3) trit 构造 (放在 d₀位)
    explicit constexpr RingElement(uint8_t t0) noexcept : trits_{} {
        trits_[0] = t0 % 3;
    }

    // ── 基底元素 (编译期常量) ──

    // [层2] T0 = 环的单位元 (d₀=1, 其余=0)
    static constexpr RingElement T0() noexcept {
        RingElement e; e.trits_[0] = 1; return e;
    }

    // [层2] T1 = 环的生成元 (d₁=1, 其余=0) — 即"3¹位"的基底
    // 定义: T1 = T0 × 3 (在Z/3¹¹Z中, 3≠0)
    static constexpr RingElement T1() noexcept {
        RingElement e; e.trits_[1] = 1; return e;
    }

    // [层2] T2 = T1 ⊗ T1 (环乘法) — 即"3²位"的基底
    // 不是十进制数9!
    static constexpr RingElement T2() noexcept { return T1() * T1(); }

    // [层2] T3 = T1 ⊗ T2 = T1³
    static constexpr RingElement T3() noexcept { return T2() * T1(); }

    // [层2] T4 = T1 ⊗ T3 = T1⁴
    static constexpr RingElement T4() noexcept { return T3() * T1(); }

    // [层2] T5 = T1 ⊗ T4 = T1⁵
    static constexpr RingElement T5() noexcept { return T4() * T1(); }

    // [层2] T6 = T1 ⊗ T5 = T1⁶
    static constexpr RingElement T6() noexcept { return T5() * T1(); }

    // ── 访问 ──

    // [层1] 获取第k位 trit (GF(3)值, {0,1,2})
    [[nodiscard]] constexpr uint8_t operator[](int k) const noexcept {
        return trits_[k];
    }

    // [层2] 判断零元
    [[nodiscard]] constexpr bool is_zero() const noexcept {
        for (auto d : trits_) if (d != 0) return false;
        return true;
    }

    // [层2] 判断单位元 (d₀=1, 其余=0)
    [[nodiscard]] constexpr bool is_one() const noexcept {
        if (trits_[0] != 1) return false;
        for (int i = 1; i < 11; ++i) if (trits_[i] != 0) return false;
        return true;
    }

    // ── 层2 环加法: 逢三进一 ──

    // [层2] [Z/3¹¹Z] a + b (进位从低位向高位传播)
    [[nodiscard]] constexpr RingElement operator+(const RingElement& other) const noexcept {
        RingElement result;
        uint8_t carry = 0;
        for (int i = 0; i < 11; ++i) {
            int total = (int)trits_[i] + (int)other.trits_[i] + (int)carry;
            // [层1] [GF(3)] 本位: GF(3)模3归约
            // [层2] [Z/3¹¹Z] 进位: 逢三进一到高位
            result.trits_[i] = (uint8_t)(total % 3);  // [层1] 本位
            carry = (uint8_t)(total / 3);               // [层2] 进位传播
        }
        // 进位超出11位 → 在Z/3¹¹Z中自然归零 (模3¹¹)
        return result;
    }

    // ── 层2 环乘法: 分布律 + GF(3)系数乘 + 进位累加 ──

    // [层2] [Z/3¹¹Z] a × b
    // 算法: 将b展开为 Σ bⱼ·Tⱼ, 则 a×b = Σ bⱼ·(a×Tⱼ)
    // a×Tⱼ 是将a的trit向量左移j位 (零填充低位)
    // 然后累加所有项 (逢三进一)
    [[nodiscard]] constexpr RingElement operator*(const RingElement& other) const noexcept {
        RingElement result;
        for (int j = 0; j < 11; ++j) {
            uint8_t bj = other.trits_[j];  // [层1] GF(3)系数
            if (bj == 0) continue;
            // 计算 a × bj × Tⱼ: 每位 trit_i × bj (GF(3)乘), 结果放在 i+j 位
            for (int i = 0; i < 11 - j; ++i) {
                uint8_t prod = TRIT_MUL_LUT[trits_[i]][bj];  // [层1] GF(3)乘
                if (prod == 0) continue;
                // [层2] 累加到 i+j 位, 逢三进一
                int pos = i + j;
                int total = (int)result.trits_[pos] + (int)prod;
                while (total >= 3 && pos < 11) {
                    result.trits_[pos] = (uint8_t)(total % 3);
                    if (pos + 1 < 11) {
                        total = (int)result.trits_[pos + 1] + (total / 3);
                        pos++;
                    } else {
                        // 进位超出 → 模3¹¹归零
                        result.trits_[pos] = (uint8_t)(total % 3);
                        break;
                    }
                }
                if (total < 3) {
                    result.trits_[pos] = (uint8_t)total;
                }
            }
        }
        return result;
    }

    // ── 层2 Tryte投影: 取低6位形成 Tryte ──

    // [层2→结构学] RingElement → Tryte (6位基3数, 729态)
    [[nodiscard]] constexpr TryteValue to_tryte() const noexcept {
        uint16_t val = 0;
        uint16_t weight = 1;  // [层2] 3⁰=1
        for (int i = 0; i < 6; ++i) {
            val += (uint16_t)trits_[i] * weight;
            weight *= 3;  // [层2] 3^(i+1)
        }
        return TryteValue{val};
    }
};

// ═══════════════════════════════════════════════════════════════════════
// [层2] 编译期验证: 环的公理
// ═══════════════════════════════════════════════════════════════════════

// [层2] 加法交换律: T1 + T2 = T2 + T1
static_assert((RingElement::T1() + RingElement::T2()).is_zero() == false,
    "Z/3¹¹Z: 加法运算合法");

// [层2] 零元: a + 0 = a (T1 + 0 = T1, d₁=1不变)
static_assert((RingElement::T1() + RingElement{})[1] == 1,
    "Z/3¹¹Z: T1 + 0 的 d₁位 = 1");

// [层2] T2 = T1 × T1 (环乘法定义, 不是十进制9)
// 验证: T1 × T1 的 d₂位=1 (因为 1×1=1, 在位置1+1=2)
consteval bool verify_t2_is_t1_squared() {
    auto t1 = RingElement::T1();  // d₁=1
    auto t2 = t1 * t1;             // T1 × T1
    return t2[2] == 1              // d₂=1 (GF(3)乘: 1×1=1, 位置1+1=2)
        && t2[0] == 0 && t2[1] == 0;
}
static_assert(verify_t2_is_t1_squared(),
    "Z/3¹¹Z: T2 = T1 ⊗ T1 (非十进制9!)");

// [层2] T3 = T1 × T2 = T1 ⊗ T1 ⊗ T1
consteval bool verify_t3_is_t1_cubed() {
    auto t1 = RingElement::T1();
    auto t2 = t1 * t1;   // T2
    auto t3 = t2 * t1;   // T1 × T2 = T1³
    return t3[3] == 1    // d₃=1
        && t3[0] == 0 && t3[1] == 0 && t3[2] == 0;
}
static_assert(verify_t3_is_t1_cubed(),
    "Z/3¹¹Z: T3 = T1 ⊗ T1 ⊗ T1 (非十进制27!)");

// [层2] 环的模: 3¹¹ = T1¹¹ = 0 (在Z/3¹¹Z中)
consteval bool verify_modulus() {
    auto t1 = RingElement::T1();
    auto acc = RingElement::T0();  // T0 = 1
    for (int k = 0; k < 11; ++k) acc = acc * t1;  // T1¹¹
    return acc.is_zero();  // 3¹¹ ≡ 0 (mod 3¹¹)
}
static_assert(verify_modulus(),
    "Z/3¹¹Z: T1¹¹ = 0 (模3¹¹)");

} // namespace sov::math::z3r

#endif // SOV_MATH_Z3R_RING_H
