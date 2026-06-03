// gf3_layer2.h — 层2: Z/3¹¹Z 环, 3≠0, 位权重3^k, 逢三进一
//
// 宪法声明:
//   数学空间: Z/3¹¹Z = Z/177147Z, 11位3-adic截断环
//   层1提供: GF(3) trit值 {0,1,2}
//   层2提供: 位权重 3^k (k=0..10), 3≠0
//   Tryte = 6位基3数 = Σ (层1 trit × 层2位权), 值域 [0,728]
//   加法: 逢三进一 (进位在基3位置间传播)
//   乘法: GF(3)逐trit乘 + 位权展开 + 累加
//   范畴边界: Z/3¹¹Z ≠ GF(3) — 3不是零元!
#ifndef SOV_MATH_GF3_LAYER2_H
#define SOV_MATH_GF3_LAYER2_H

#include "gf3_types.h"
#include <span>
#include <vector>
#include <cstring>

namespace sov::math {

// ============================================================================
// 层2 GF(3)→Z/3¹¹Z: trit×位权乘法 (层1值 × 层2位权)
// ============================================================================

// [层1↔层2] GF(3)乘法表查询: 层1的GF(3)域乘法 (模3)
// 输入a,b∈{0,1,2}来自层1, 输出∈{0,1,2}在层1
inline constexpr uint8_t z3r_mul(uint8_t a, uint8_t b) noexcept {
    return TRIT_MUL_LUT[a][b];  // [层1] [GF(3)模3] GF(3)乘法
}

// ============================================================================
// 层2 逢三进一加法 (基3进位传播)
// ============================================================================

// [层2] [Z/3¹¹Z] 逢三进一: sum=(a+b+carry_in)%3, carry=(a+b+carry_in)/3
struct z3r_add_result { uint8_t sum; uint8_t carry; };

inline constexpr z3r_add_result z3r_add_carry(uint8_t a, uint8_t b, uint8_t carry_in = 0) {
    int total = (int)a + (int)b + (int)carry_in;  // [层0] 模2整数加法
    return {
        (uint8_t)(total % 3),   // [层1] 模3归约→本位
        (uint8_t)(total / 3)    // [层2] 逢三进一→进位到高位
    };
}

// ============================================================================
// 层2 Tryte运算: 6位基3数的位权展开
// ============================================================================

// [层2] [Z/3¹¹Z] 范数: |T₀|=0, |T₁|=1, |T₂|=1
inline constexpr int z3r_norm(uint8_t x) noexcept {
    return TRIT_NORM_LUT[x];  // [层1] [GF(3)] 范数表
}

// ============================================================================
// 层2 批量向量运算 (Z/3¹¹Z进位传播)
// ============================================================================

// [层2] [Z/3¹¹Z] 向量加法: 逢三进一, 进位在trit间传播
// 注意: 这不是层1的逐trit模3加法! 进位从低位向高位传播
inline std::vector<uint8_t> z3r_vector_add(
    std::span<const uint8_t> a,    // [层1] GF(3) trit值
    std::span<const uint8_t> b     // [层1] GF(3) trit值
) {
    size_t n = std::min(a.size(), b.size());
    std::vector<uint8_t> result(n);
    uint8_t carry = 0;                      // [层2] 进位状态
    for (size_t i = 0; i < n; ++i) {
        auto r = z3r_add_carry(a[i], b[i], carry);  // [层2] 逢三进一
        result[i] = r.sum;                           // [层1] GF(3)本位
        carry = r.carry;                             // [层2] 进位传播到高位
    }
    return result;
}

// [层2] [Z/3¹¹Z] 点积: Σ GF3_MUL(a[i],b[i]) × 3^i (位权展开)
// 先用层1 GF(3)乘法得到每对trit的乘积{0,1,2}
// 再用模2整数累加 (进位由CPU自动处理)
inline int64_t z3r_dot_product(
    std::span<const uint8_t> a,    // [层1] GF(3) trit值
    std::span<const uint8_t> b     // [层1] GF(3) trit值
) {
    size_t n = std::min(a.size(), b.size());
    int64_t total = 0;
    for (size_t i = 0; i < n; ++i) {
        total += (int64_t)TRIT_MUL_LUT[a[i]][b[i]];  // [层1] GF(3)乘积 + [层0] 模2累加
    }
    return total;  // 调用方自行 %3 归约到 GF(3) trit
}

// [层2] [Z/3¹¹Z] 范数平方向量
inline int64_t z3r_vector_norm_sq(std::span<const uint8_t> x) {
    int64_t count = 0;
    for (size_t i = 0; i < x.size(); ++i)
        if (x[i] != 0) count++;  // [层1] [GF(3)] |T₀|=0, |T₁|=|T₂|=1
    return count;
}

// ============================================================================
// 层2 矩阵乘法: C[n][i] = (Σ_j GF3_MUL(x[n][j], w[i][j])) % 3
// 层1 GF(3)乘法 × 层0模2累加 → 层1模3归约
// ============================================================================

// [层2] [Z/3¹¹Z] GF(3)矩阵乘法
void z3r_matmul(
    const uint8_t* x,        // [层1] [N, in_dim] GF(3) trit值
    const uint8_t* w,        // [层1] [out_dim, in_dim] GF(3) trit权重
    int N, int out_dim, int in_dim,
    uint8_t* result          // [层1] [N, out_dim] GF(3) trit输出
) {
    for (int n = 0; n < N; ++n) {
        for (int i = 0; i < out_dim; ++i) {
            int64_t total = 0;
            for (int j = 0; j < in_dim; ++j) {
                total += TRIT_MUL_LUT[x[n * in_dim + j]][w[i * in_dim + j]];
                // [层1] GF(3)乘法 + [层0] 模2累加
            }
            result[n * out_dim + i] = (uint8_t)(total % 3);
            // [层1] 模3归约→GF(3) trit
        }
    }
}

// ============================================================================
// 层2 √3门控: gate = (|H|² > Δ²) ? 1 : 0
// ============================================================================

// [层2] [Z/3¹¹Z] 门控: 范数 > Q16能隙阈值 → 激活
inline std::vector<uint8_t> z3r_gate(
    std::span<const uint8_t> x,  // [层1] GF(3) trit值
    int dim,
    int32_t threshold_q16        // [层0] [模2Q16] 能隙阈值
) {
    int N = (int)x.size() / dim;
    std::vector<uint8_t> gate(N, 0);

    for (int n = 0; n < N; ++n) {
        int non_zero = 0;
        for (int j = 0; j < dim; ++j)
            if (x[n * dim + j] != 0) non_zero++;  // [层1] [GF(3)] 范数计数
        // [层0] [模2Q16] 整数比较: non_zero×65536 > threshold_q16
        gate[n] = ((non_zero << ZHONGLV_SHIFT) > threshold_q16) ? GF3_T1 : GF3_T0;
    }
    return gate;
}

// [层2] [Z/3¹¹Z] 门控乘: result = gate × x (GF(3)乘法)
inline std::vector<uint8_t> z3r_gated_mul(
    std::span<const uint8_t> gate,  // [层1] gate值 {0,1,2}
    std::span<const uint8_t> x,     // [层1] GF(3) trit值
    int dim
) {
    int N = (int)gate.size();
    std::vector<uint8_t> result(N * dim);
    for (int n = 0; n < N; ++n) {
        uint8_t g = gate[n] % 3;
        for (int j = 0; j < dim; ++j) {
            result[n * dim + j] = TRIT_MUL_LUT[g][x[n * dim + j]];
            // [层1] [GF(3)模3] GF(3)乘法
        }
    }
    return result;
}

// ============================================================================
// 层2 A4四面体翻转 (层1 GF(3)值上的群操作)
// ============================================================================

// [层2] [A4群] 操作类型
enum class A4Op : uint8_t {
    C3_CLOCKWISE      = 0,  // [层1] [GF(3)] (t+1)%3
    C3_COUNTERCLOCK   = 1,  // [层1] [GF(3)] (t+2)%3
    AUTOMORPHISM      = 2,  // [层1] [GF(3)] (6-t)%3
    CHIRAL_EXCHANGE   = 3,  // [层3] [手性] 成对交换
};

// [层2] [A4群] 对3个trit执行A4翻转 (层1 GF(3)值上操作)
inline void a4_flip_triple(uint8_t* a4, A4Op op) {
    switch (op) {
        case A4Op::C3_CLOCKWISE:
            a4[0] = (a4[0] + 1) % 3;  // [层1] GF(3): T0→T1→T2→T0
            a4[1] = (a4[1] + 1) % 3;
            a4[2] = (a4[2] + 1) % 3;
            break;
        case A4Op::C3_COUNTERCLOCK:
            a4[0] = (a4[0] + 2) % 3;  // [层1] GF(3): T0→T2→T1→T0
            a4[1] = (a4[1] + 2) % 3;
            a4[2] = (a4[2] + 2) % 3;
            break;
        case A4Op::AUTOMORPHISM:
            a4[0] = (6 - a4[0]) % 3;  // [层1] GF(3): 保持GF(3)结构
            a4[1] = (6 - a4[1]) % 3;
            a4[2] = (6 - a4[2]) % 3;
            break;
        case A4Op::CHIRAL_EXCHANGE:
            break;  // [层3] 手性交换在成对索引上执行
    }
}

// ============================================================================
// 层2 RMSNorm (GF(3)归一化 + Q16定点缩放)
// ============================================================================

// [层0→层2] Q16 rsqrt LUT预计算 (模2编译期)
inline std::vector<int32_t> build_rsqrt_lut_q16(int dim, float eps = 1e-5f) {
    std::vector<int32_t> lut(dim + 1);
    for (int m = 0; m <= dim; ++m) {
        double mean = (double)m / (double)dim;
        double val = 1.0 / std::sqrt(mean + (double)eps);       // [层0] 编译期double
        lut[m] = (int32_t)(val * (double)ZHONGLV_BOUNDARY + 0.5); // [层0] Q16定点截断
    }
    return lut;
}

// [层2] [Z/3¹¹Z] RMSNorm前向: GF(3)乘法 + Q16定点缩放 → 模3归约
inline std::vector<uint8_t> z3r_rms_norm(
    std::span<const uint8_t> x_flat,         // [层1] GF(3) trit值 [N×dim]
    std::span<const uint8_t> gamma,          // [层1] GF(3) gamma trit [dim]
    std::span<const int32_t> rsqrt_lut_q16,  // [层0] [模2Q16] rsqrt LUT
    int dim
) {
    int N = (int)x_flat.size() / dim;
    std::vector<uint8_t> result(N * dim);

    for (int n = 0; n < N; ++n) {
        int non_zero = 0;
        for (int j = 0; j < dim; ++j)
            if (x_flat[n * dim + j] != 0) non_zero++;  // [层1] [GF(3)] 范数

        int32_t rsqrt = rsqrt_lut_q16[non_zero];        // [层0] [模2Q16] 定点缩放因子

        for (int j = 0; j < dim; ++j) {
            int prod = TRIT_MUL_LUT[x_flat[n * dim + j]][gamma[j % gamma.size()]];
            // [层1] [GF(3)模3] GF(3)乘积 {0,1,2}
            int32_t scaled = (prod * rsqrt + 32768) >> ZHONGLV_SHIFT;
            // [层0] [模2Q16] 定点乘+移位: (prod×rsqrtQ16 + 0.5) 截断
            result[n * dim + j] = (uint8_t)(scaled % 3);
            // [层1] [GF(3)模3] 归约到{0,1,2}
        }
    }
    return result;
}

} // namespace sov::math

#endif // SOV_MATH_GF3_LAYER2_H
