// gf3_operators.h — 律算算子体系 (C++23 类继承, 每层范畴绑定)
//
// 宪法声明:
//   [层0] [模2]    AdcAdder — 二进制ADC进位链
//   [层1] [GF(3)]  Layer1Ops — GF(3)有限域, 3≡0, 逐trit运算
//   [桥]  [LCM]    LcmBridge — (acc×3¹¹)>>16, 层1↔层2桥接
//   [层2] [Z/3¹¹Z] GF3Ops — Z/3¹¹Z环, 3≠0, 逢三进一, 位权3^k
//   [桥]           RMSNormOp — Q16定点, LCM桥归一化
//   [层2]          GateOp, MatmulOp — Z/3¹¹Z运算
//   继承链: AdcAdder(L0) ← Layer1Ops(L1) ← LcmBridge(桥) ← GF3Ops(L2)
#ifndef SOV_MATH_GF3_OPERATORS_H
#define SOV_MATH_GF3_OPERATORS_H

#include "lcm_constants.h"
#include "gf3_types.h"
#include "adc_carry_chain.h"
#include <vector>

namespace sov::math {

// ============================================================================
// 算子基类: 范畴绑定 + 层标签
// ============================================================================

// [基类] 编译时层标签绑定
template<int L>
struct [[nodiscard]] OperatorBase {
    static constexpr int layer = L;
    using layer_type = layer_tag<L>;
    virtual ~OperatorBase() = default;
};

// ═══════════════════════════════════════════════════════════════════════
// [层0] [模2] AdcAdder — 二进制ADC进位链
// ═══════════════════════════════════════════════════════════════════════

template<size_t N_LIMBS = ADC_LIMB_COUNT>
class AdcAdder final : public OperatorBase<0> {
    uint64_t carry_{0};
public:
    using OperatorBase<0>::layer;

    [[nodiscard]] constexpr uint64_t carry() const noexcept { return carry_; }

    // 单步ADC: s[i] = a[i] + b[i] + carry
    template<size_t N>
    void add(const uint64_t (&a)[N], const uint64_t (&b)[N], uint64_t (&s)[N]) noexcept {
        carry_ = 0;
        for (size_t i = 0; i < N && i < N_LIMBS; ++i) {
            uint64_t sum = a[i] + carry_;
            carry_ = (sum < carry_) ? 1ULL : 0ULL;
            sum += b[i];
            carry_ += (sum < b[i]) ? 1ULL : 0ULL;
            s[i] = sum;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════
// [层1] [GF(3)模3] Layer1Ops — GF(3)有限域, 3≡0, 逐trit独立
// ═══════════════════════════════════════════════════════════════════════

class Layer1Ops : public OperatorBase<1> {
public:
    using OperatorBase<1>::layer;

    // [层1→层0] 点积: GF(3)值在模2硬件上累加
    [[nodiscard]] static constexpr uint64_t dot(
        const uint8_t* a, const uint8_t* b, size_t n
    ) noexcept {
        uint64_t total = 0;
        for (size_t i = 0; i < n; ++i)
            total += (uint64_t)a[i] * (uint64_t)b[i];  // [层0] 模2乘法+累加
        return total;
    }

    // [层1] [GF(3)] 范数: |0|=0, |1|=|2|=1
    [[nodiscard]] static constexpr uint64_t norm_sq(
        const uint8_t* x, size_t n
    ) noexcept {
        uint64_t c = 0;
        for (size_t i = 0; i < n; ++i) if (x[i]) c++;
        return c;
    }

    // [层0] [模2编码] 5-trit打包: GF(3)→uint8
    [[nodiscard]] static constexpr uint8_t pack_5(
        uint8_t t0, uint8_t t1, uint8_t t2, uint8_t t3, uint8_t t4
    ) noexcept {
        return t0 + t1*3 + t2*9 + t3*27 + t4*81;
    }

    // [层0] [模2解码] 5-trit解包: uint8→GF(3)
    static constexpr void unpack_5(uint8_t p, uint8_t* o) noexcept {
        o[0] = p / 81; p %= 81;
        o[1] = p / 27; p %= 27;
        o[2] = p / 9;  p %= 9;
        o[3] = p / 3;  p %= 3;
        o[4] = p;
    }
};

// ═══════════════════════════════════════════════════════════════════════
// [桥] [LCM] LcmBridge — (acc×177147)>>16, 层1 GF(3) ↔ 层2 Z/3¹¹Z
// ═══════════════════════════════════════════════════════════════════════

class LcmBridge final : public Layer1Ops {
    uint64_t accumulator_{0};
    int step_count_{0};
    int32_t chern_q16_{0};
public:
    static constexpr int bridge_layer = -1;  // [桥] 跨层标签, 不属于任何单一层

    [[nodiscard]] constexpr uint64_t acc() const noexcept { return accumulator_; }
    [[nodiscard]] constexpr int steps() const noexcept { return step_count_; }

    // [桥] 步进: 在LCM环内累加
    constexpr void step(uint64_t delta) noexcept {
        accumulator_ = (accumulator_ * HUANGZHONG + delta) % LCM_TOTAL;
        step_count_++;
    }

    // [桥→] 正向桥: 层1 GF(3) → 层2 Z/3¹¹Z
    constexpr uint64_t forward() noexcept {
        accumulator_ = (accumulator_ * HUANGZHONG) >> ZHONGLV_SHIFT;  // [层0] 模2乘+移位
        return accumulator_;
    }

    // [桥←] 逆向桥: 层2 Z/3¹¹Z → 层1 GF(3), 需chern_guard
    [[nodiscard]] constexpr uint8_t reverse(uint8_t z3r_val) const noexcept {
        constexpr int32_t C_TGT = 131072;   // [层4] [拓扑] C=2 Q16
        if (chern_q16_ < C_TGT - 655 || chern_q16_ > C_TGT + 655)
            return 0;
        return z3r_val % 3;  // [层1] 模3归约→GF(3)
    }

    void set_chern(int32_t c) noexcept { chern_q16_ = c; }

    // [桥] 微泵: 仲吕闭合
    constexpr void micro_pump() noexcept {
        forward();
        step_count_ = 0;
    }

    // [桥] 大泵: 主权状态机完整呼吸归零
    constexpr void grand_pump() noexcept {
        accumulator_ = 0;
        step_count_ = 0;
    }
};

// ═══════════════════════════════════════════════════════════════════════
// [层2] [Z/3¹¹Z] Z3ROps — Z/3¹¹Z环运算, 3≠0, 逢三进一
// 范畴: 这是 Z/3¹¹Z 环, 不是 GF(3) 有限域!
//       内部使用 GF(3)LUT (层1) 做 trit 运算, 但范畴是层2的环运算
// ═══════════════════════════════════════════════════════════════════════

class Z3ROps : public OperatorBase<2> {
public:
    using OperatorBase<2>::layer;

    // [层1→层2] GF(3)乘法: 层1 LUT, 层2用于位权展开
    [[nodiscard]] static constexpr uint8_t mul(uint8_t a, uint8_t b) noexcept {
        constexpr uint8_t M[3][3] = {{0,0,0},{0,1,2},{0,2,1}};
        return M[a][b];
    }

    // [层2] [Z/3¹¹Z] 逢三进一: 本位=(a+b+c_in)%3, 进位=(a+b+c_in)/3
    struct AddResult { uint8_t sum; uint8_t carry; };
    [[nodiscard]] static constexpr AddResult add(uint8_t a, uint8_t b, uint8_t c_in = 0) noexcept {
        int t = (int)a + (int)b + (int)c_in;
        return {(uint8_t)(t % 3), (uint8_t)(t / 3)};
    }

    // [层2] [Z/3¹¹Z] 范数: 非零trit计数
    [[nodiscard]] static constexpr int norm_sq(uint8_t x) noexcept {
        return (x == 0) ? 0 : 1;
    }

    // [层2] [Z/3¹¹Z] 点积: GF(3)逐对乘 + 模2累加 → mod3归约
    [[nodiscard]] static constexpr uint8_t dot(
        const uint8_t* a, const uint8_t* b, size_t n
    ) noexcept {
        int64_t t = 0;
        for (size_t i = 0; i < n; ++i)
            t += TRIT_MUL_LUT[a[i]][b[i]];
        return (uint8_t)(t % 3);
    }

    // [几何→层2] C3/A4旋转 (来自正四面体几何, 通过GF(3)编码)
    [[nodiscard]] static constexpr uint8_t a4_c3_cw(uint8_t t) noexcept { return (t + 1) % 3; }
    [[nodiscard]] static constexpr uint8_t a4_c3_ccw(uint8_t t) noexcept { return (t + 2) % 3; }
    [[nodiscard]] static constexpr uint8_t a4_auto(uint8_t t) noexcept { return (6 - t) % 3; }
};

// ═══════════════════════════════════════════════════════════════════════
// [桥] RMSNormOp — Q16定点归一化 (层1 GF(3) + LCM桥)
// ═══════════════════════════════════════════════════════════════════════

class RMSNormOp final : public Z3ROps {
    std::vector<int32_t> rsqrt_lut_q16_;
    int dim_;
public:
    using Z3ROps::layer;

    explicit RMSNormOp(int dim, double eps = 1e-5) : dim_(dim) {
        rsqrt_lut_q16_.resize(dim + 1);
        for (int m = 0; m <= dim; ++m) {
            double mean = (double)m / (double)dim;
            rsqrt_lut_q16_[m] = (int32_t)(
                (1.0 / std::sqrt(mean + eps)) * (double)ZHONGLV_BOUNDARY + 0.5
            );
        }
    }

    // [桥] 前向: 层1 GF(3)乘法 + Q16定点缩放 → 模3归约
    void forward(
        const uint8_t* x,      // [层1] [N, dim] GF(3)输入
        const uint8_t* gamma,  // [层1] [dim] GF(3)缩放
        int N,
        uint8_t* result        // [层1] [N, dim] GF(3)输出
    ) const noexcept {
        for (int n = 0; n < N; ++n) {
            int nz = 0;
            for (int j = 0; j < dim_; ++j)
                if (x[n * dim_ + j]) nz++;
            int32_t rs = rsqrt_lut_q16_[nz];

            for (int j = 0; j < dim_; ++j) {
                int prod = TRIT_MUL_LUT[x[n * dim_ + j]][gamma[j]];
                int32_t scaled = (prod * rs + 32768) >> ZHONGLV_SHIFT;
                result[n * dim_ + j] = (uint8_t)(scaled % 3);
            }
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════
// [层1→层2] GateOp — √3门控: GF(3)范数→Q16阈值比较
// ═══════════════════════════════════════════════════════════════════════

class GateOp final : public Z3ROps {
    int32_t threshold_q16_;
public:
    using Z3ROps::layer;

    explicit constexpr GateOp(double threshold = 1.7320508) noexcept
        : threshold_q16_((int32_t)(threshold * (double)ZHONGLV_BOUNDARY)) {}

    // [层1→层2] GF(3)范数 + Q16阈值 → gate
    void forward(
        const uint8_t* x, int N, int dim, uint8_t* gate
    ) const noexcept {
        for (int n = 0; n < N; ++n) {
            int nz = 0;
            for (int j = 0; j < dim; ++j)
                if (x[n * dim + j]) nz++;
            gate[n] = ((nz << ZHONGLV_SHIFT) > threshold_q16_) ? GF3_T1 : GF3_T0;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════
// [层2] [Z/3¹¹Z] MatmulOp — 矩阵乘法: 层1 GF(3)乘 + 层0累加 → 层1归约
// ═══════════════════════════════════════════════════════════════════════

class MatmulOp final : public Z3ROps {
public:
    using Z3ROps::layer;

    void forward(
        const uint8_t* x,       // [N, in_dim]
        const uint8_t* w,       // [out_dim, in_dim]
        int N, int out_dim, int in_dim,
        uint8_t* result         // [N, out_dim]
    ) const noexcept {
        for (int n = 0; n < N; ++n) {
            for (int o = 0; o < out_dim; ++o) {
                int64_t total = 0;
                for (int j = 0; j < in_dim; ++j)
                    total += TRIT_MUL_LUT[x[n * in_dim + j]][w[o * in_dim + j]];
                result[n * out_dim + o] = (uint8_t)(total % 3);
            }
        }
    }
};

} // namespace sov::math

#endif // SOV_MATH_GF3_OPERATORS_H
