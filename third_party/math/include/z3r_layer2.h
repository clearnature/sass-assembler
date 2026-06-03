// z3r_layer2.h — 层2: Z/3¹¹Z 环, 3≠0, T1=3¹, T2=3², 逢三进一
//
// 宪法声明:
//   范畴: Z/3¹¹Z 环 = 11位3-adic截断环, 不是 GF(3) 有限域!
//   GF(3): 3≡0, 域, 每trit独立
//   Z/3¹¹Z: 3≠0, 环, 位权3^k, 进位传播
//   层2的 RingElement (z3r_ring.h) 是 Z/3¹¹Z 的唯一原生类型
//   本模块提供 RingElement 的批量运算 (向量/矩阵)
//   禁止在 Z/3¹¹Z 上下文直接使用 GF(3)LUT — 必须通过 RingElement 接口
#ifndef SOV_MATH_Z3R_LAYER2_H
#define SOV_MATH_Z3R_LAYER2_H

#include "z3r_ring.h"
#include <span>
#include <vector>

namespace sov::math::z3r {

// ═══════════════════════════════════════════════════════════════════════
// [层2] [Z/3¹¹Z] Tryte 投影: RingElement → 6位基3数
// ═══════════════════════════════════════════════════════════════════════

[[nodiscard]] inline TryteValue to_tryte(const RingElement& e) noexcept {
    return e.to_tryte();
}

// ═══════════════════════════════════════════════════════════════════════
// [层2] [Z/3¹¹Z] 向量加法: 逐元素 RingElement + RingElement
// ═══════════════════════════════════════════════════════════════════════

inline std::vector<RingElement> vector_add(
    std::span<const RingElement> a,
    std::span<const RingElement> b
) {
    size_t n = std::min(a.size(), b.size());
    std::vector<RingElement> result(n);
    for (size_t i = 0; i < n; ++i)
        result[i] = a[i] + b[i];  // [层2] RingElement::operator+
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
// [层2] [Z/3¹¹Z] 点积: Σ a[i] × b[i] (环乘 + 环加)
// ═══════════════════════════════════════════════════════════════════════

[[nodiscard]] inline RingElement dot_product(
    std::span<const RingElement> a,
    std::span<const RingElement> b
) {
    size_t n = std::min(a.size(), b.size());
    RingElement total;  // 零元
    for (size_t i = 0; i < n; ++i)
        total = total + a[i] * b[i];  // [层2] 环乘 + 环加
    return total;
}

// ═══════════════════════════════════════════════════════════════════════
// [层2] [Z/3¹¹Z] 矩阵乘法: C[n][i] = Σ_j A[n][j] × B[j][i]
// ═══════════════════════════════════════════════════════════════════════

inline void matmul(
    const RingElement* x,    // [N, in_dim]
    const RingElement* w,    // [out_dim, in_dim]
    int N, int out_dim, int in_dim,
    RingElement* result      // [N, out_dim]
) noexcept {
    for (int n = 0; n < N; ++n) {
        for (int i = 0; i < out_dim; ++i) {
            RingElement total;
            for (int j = 0; j < in_dim; ++j) {
                total = total + x[n * in_dim + j] * w[i * in_dim + j];
            }
            result[n * out_dim + i] = total;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// [层2] [Z/3¹¹Z] 门控: 范数平方 > 阈值 → 激活
// ═══════════════════════════════════════════════════════════════════════

inline std::vector<uint8_t> gate(
    std::span<const RingElement> x,  // [N]
    int32_t threshold_q16            // [层0] Q16阈值
) {
    int N = (int)x.size();
    std::vector<uint8_t> g(N, 0);
    for (int n = 0; n < N; ++n) {
        // 范数: 非零trit计数 (RingElement内部是trit向量)
        int non_zero = 0;
        for (int k = 0; k < 11; ++k)
            if (x[n][k] != 0) non_zero++;
        g[n] = ((non_zero << ZHONGLV_SHIFT) > threshold_q16) ? 1 : 0;
    }
    return g;
}

} // namespace sov::math::z3r

#endif // SOV_MATH_Z3R_LAYER2_H
