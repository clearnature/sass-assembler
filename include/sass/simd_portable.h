/* ============================================================================
 * HunTian SASS 汇编器 — SIMD 可移植抽象层
 *
 * 封装 x86_64 AVX2 内联函数，提供标量回退。
 * 未来可扩展 ARM NEON / SVE 实现。
 *
 * 类型：
 *   simd_f32x8 — 8 个 float 的 SIMD 向量 (AVX2: __m256)
 *
 * 操作：
 *   simd_f32x8_load(p)    — 从对齐内存加载
 *   simd_f32x8_store(p,v) — 存储到对齐内存
 *   simd_f32x8_set1(x)    — 广播标量
 *   simd_f32x8_mul(a,b)   — 逐元素乘
 *   simd_f32x8_add(a,b)   — 逐元素加
 *   simd_f32x8_sub(a,b)   — 逐元素减
 *   simd_f32x8_cmp_gt(a,b)— a > b 掩码
 *   simd_f32x8_and(a,b)   — 按位与
 * ============================================================================ */

#ifndef SASS_SIMD_PORTABLE_H
#define SASS_SIMD_PORTABLE_H

#include <cstdint>
#include <cstring>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>

namespace sass { namespace simd {

using f32x8 = __m256;

inline f32x8 f32x8_load(const float* p)      { return _mm256_loadu_ps(p); }
inline void  f32x8_store(float* p, f32x8 v)   { _mm256_storeu_ps(p, v); }
inline f32x8 f32x8_set1(float x)              { return _mm256_set1_ps(x); }
inline f32x8 f32x8_mul(f32x8 a, f32x8 b)     { return _mm256_mul_ps(a, b); }
inline f32x8 f32x8_add(f32x8 a, f32x8 b)     { return _mm256_add_ps(a, b); }
inline f32x8 f32x8_sub(f32x8 a, f32x8 b)     { return _mm256_sub_ps(a, b); }
inline f32x8 f32x8_cmp_gt(f32x8 a, f32x8 b)  { return _mm256_cmp_ps(a, b, _CMP_GT_OS); }
inline f32x8 f32x8_and(f32x8 a, f32x8 b)     { return _mm256_and_ps(a, b); }

}} // namespace sass::simd

#else
// ─── 标量回退 ───
namespace sass { namespace simd {

struct f32x8 { float d[8]; };

inline f32x8 f32x8_load(const float* p) {
    f32x8 v; std::memcpy(v.d, p, 32); return v;
}
inline void f32x8_store(float* p, f32x8 v) {
    std::memcpy(p, v.d, 32);
}
inline f32x8 f32x8_set1(float x) {
    f32x8 v;
    for (int i = 0; i < 8; ++i) v.d[i] = x;
    return v;
}
inline f32x8 f32x8_mul(f32x8 a, f32x8 b) {
    f32x8 r;
    for (int i = 0; i < 8; ++i) r.d[i] = a.d[i] * b.d[i];
    return r;
}
inline f32x8 f32x8_add(f32x8 a, f32x8 b) {
    f32x8 r;
    for (int i = 0; i < 8; ++i) r.d[i] = a.d[i] + b.d[i];
    return r;
}
inline f32x8 f32x8_sub(f32x8 a, f32x8 b) {
    f32x8 r;
    for (int i = 0; i < 8; ++i) r.d[i] = a.d[i] - b.d[i];
    return r;
}
inline f32x8 f32x8_cmp_gt(f32x8 a, f32x8 b) {
    f32x8 r;
    for (int i = 0; i < 8; ++i) r.d[i] = (a.d[i] > b.d[i]) ? -1.0f : 0.0f;
    return r;
}
inline f32x8 f32x8_and(f32x8 a, f32x8 b) {
    f32x8 r;
    for (int i = 0; i < 8; ++i) {
        uint32_t ai, bi;
        std::memcpy(&ai, &a.d[i], 4);
        std::memcpy(&bi, &b.d[i], 4);
        uint32_t ri = ai & bi;
        std::memcpy(&r.d[i], &ri, 4);
    }
    return r;
}

}} // namespace sass::simd
#endif

#endif // SASS_SIMD_PORTABLE_H
