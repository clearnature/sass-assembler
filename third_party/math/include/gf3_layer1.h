// gf3_layer1.h — 层1: GF(3) 有限域, 每trit独立, 3≡0, 模3运算
//
// 宪法声明:
//   数学空间: GF(3) = Z/3Z, 特征3, 3≡0
//   运算: (a+b)%3 (域加法), (a×b)%3 (域乘法)
//   范数: |0|=0, |1|=1, |2|=1
//   存储: 2-bit/trit, 5 trit/byte打包
//   范畴边界: 层1 GF(3) ≠ 层2 Z/3¹¹Z
#ifndef SOV_MATH_GF3_LAYER1_H
#define SOV_MATH_GF3_LAYER1_H

#include "gf3_types.h"
#include <cstring>
#include <span>
#include <vector>
#include <cstddef>

namespace sov::math {

// [层1] [GF(3)] trit硬件编码常量
inline constexpr uint8_t TRIT_LAYER1_0    = 0b00;  // T0编码
inline constexpr uint8_t TRIT_LAYER1_1    = 0b01;  // T1编码
inline constexpr uint8_t TRIT_LAYER1_2    = 0b10;  // T2编码
inline constexpr uint8_t TRIT_LAYER1_MASK = 0b11;  // 2-bit掩码 (0b11非法)

// ============================================================================
// 层1→层0 编码: 5 trit → 1 字节 (模2运算下的打包)
// ============================================================================

// [层0] [模2编码] 5个GF(3) trit → 1字节 base-3值 [0,242]
// byte = t₀×3⁰ + t₁×3¹ + t₂×3² + t₃×3³ + t₄×3⁴
// 注意: 3¹3²3³3⁴是模2十进制数, 仅用于编码, 不是Z/3¹¹Z位权
inline constexpr uint8_t pack_5_trits(
    uint8_t t0, uint8_t t1, uint8_t t2, uint8_t t3, uint8_t t4
) {
    return t0 + t1 * 3 + t2 * 9 + t3 * 27 + t4 * 81;  // [层0] 模2乘法+加法
}

// [层0] [模2解码] 1字节 → 5个GF(3) trit
inline constexpr void unpack_5_trits(uint8_t packed, uint8_t* out) {
    out[0] = packed / 81; packed %= 81;  // [层0] 模2除法 (高位优先)
    out[1] = packed / 27; packed %= 27;
    out[2] = packed / 9;  packed %= 9;
    out[3] = packed / 3;  packed %= 3;
    out[4] = packed;                     // 0,1,2
}

// ============================================================================
// 层1 GF(3) 批量打包/解包 (模2硬件运算)
// ============================================================================

// [层0] [模2编码] 批量打包: n trit → ceil(n/5) 字节
inline std::vector<uint8_t> pack_trits_layer1(std::span<const uint8_t> trits) {
    size_t n = trits.size();
    size_t packed_len = (n + 4) / 5;
    std::vector<uint8_t> packed(packed_len, 0);

    for (size_t i = 0; i < n; i += 5) {
        uint8_t t0 = (i + 0 < n) ? trits[i + 0] : 0;  // [层1] GF(3)值
        uint8_t t1 = (i + 1 < n) ? trits[i + 1] : 0;
        uint8_t t2 = (i + 2 < n) ? trits[i + 2] : 0;
        uint8_t t3 = (i + 3 < n) ? trits[i + 3] : 0;
        uint8_t t4 = (i + 4 < n) ? trits[i + 4] : 0;
        packed[i / 5] = pack_5_trits(t0, t1, t2, t3, t4);  // [层0] 模2打包
    }
    return packed;
}

// [层0] [模2解码] 批量解包: ceil(n/5) 字节 → n trit
inline std::vector<uint8_t> unpack_trits_layer1(
    std::span<const uint8_t> packed, size_t n_trits
) {
    std::vector<uint8_t> trits(n_trits, 0);
    size_t packed_idx = 0;

    for (size_t i = 0; i < n_trits; i += 5) {
        if (packed_idx >= packed.size()) break;
        uint8_t out[5];
        unpack_5_trits(packed[packed_idx++], out);  // [层0] 模2解包
        for (int j = 0; j < 5 && (i + j) < n_trits; ++j)
            trits[i + j] = out[j];                  // [层1] GF(3)值
    }
    return trits;
}

// ============================================================================
// 层1 GF(3) 逐trit运算 (模3, 每trit独立, 无进位)
// ============================================================================

// [层1] [GF(3)模3] 逐trit加法: (a+b)%3
// 注意: 这是GF(3)域加法, 不是Z/3¹¹Z进位加法!
//       每个trit独立运算, 没有位间进位传播
inline std::vector<uint8_t> layer1_add(
    std::span<const uint8_t> a,
    std::span<const uint8_t> b
) {
    size_t n = std::min(a.size(), b.size());
    std::vector<uint8_t> result(n);
    for (size_t i = 0; i < n; ++i)
        result[i] = (a[i] + b[i]) % 3;  // [层1] 模3归约, 无进位
    return result;
}

// [层1→层0] [模2累加] GF(3)点积: Σ a[i]×b[i] (模2硬件累加)
// 结果在模2下累加为uint64, 不归约到GF(3)
inline uint64_t layer1_dot(std::span<const uint8_t> a, std::span<const uint8_t> b) {
    size_t n = std::min(a.size(), b.size());
    uint64_t total = 0;
    for (size_t i = 0; i < n; ++i)
        total += (uint64_t)a[i] * (uint64_t)b[i];  // [层0] 模2乘法+累加
    return total;
}

// [层1] [GF(3)模3] 范数平方: Σ(a[i]≠0), 即非零trit计数
inline uint64_t layer1_norm_sq(std::span<const uint8_t> x) {
    uint64_t count = 0;
    for (size_t i = 0; i < x.size(); ++i)
        if (x[i] != 0) count++;  // [层1] GF(3): |0|=0, |1|=1, |2|=1
    return count;
}

// ============================================================================
// 层1→层0 SovBlock128 读写 (模2硬件上的字节操作)
// ============================================================================

// [层1→层0] 模2解码: SovBlock128数组 → GF(3) trit列表
inline std::vector<uint8_t> extract_trits_layer1(
    const SovBlock128* blocks, size_t num_blocks
) {
    std::vector<uint8_t> trits;
    trits.reserve(num_blocks * 6);

    for (size_t i = 0; i < num_blocks; ++i) {
        uint8_t out[5];
        unpack_5_trits(blocks[i].qs[0], out);  // [层0] 模2解包
        trits.insert(trits.end(), out, out + 5);
        trits.push_back(blocks[i].qs[1] % 3);   // [层1] GF(3): 第6个trit
    }
    return trits;
}

// [层0→层1] 模2编码: GF(3) trit列表 → SovBlock128数组
inline void inject_trits_layer1(
    SovBlock128* blocks, size_t num_blocks,
    std::span<const uint8_t> trits
) {
    size_t trit_idx = 0;
    for (size_t i = 0; i < num_blocks && trit_idx + 4 < trits.size(); ++i) {
        blocks[i].qs[0] = pack_5_trits(
            trits[trit_idx], trits[trit_idx+1], trits[trit_idx+2],
            trits[trit_idx+3], trits[trit_idx+4]
        );  // [层0] 模2打包
        blocks[i].qs[1] = (trit_idx + 5 < trits.size()) ? trits[trit_idx+5] % 3 : 0;
        blocks[i].qs[2] = blocks[i].qs[3] = blocks[i].qs[4] = blocks[i].qs[5] = 0;
        trit_idx += 6;
    }
}

} // namespace sov::math

#endif // SOV_MATH_GF3_LAYER1_H
