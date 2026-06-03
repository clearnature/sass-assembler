/* ============================================================================
 * HunTian SASS 汇编器 — SIMD 内核 (AVX2 + 标量回退)
 *
 * 4320D 流形调度的计算密集部分：
 *   1. compute_laplacian_curvature  — 离散拉普拉斯曲率 (4096-bit SIMD)
 *   2. compute_yamabe_flow          — Yamabe 流扩散 (AVX2)
 *   3. compute_yamabe_smoothed      — Yamabe 平滑 (AVX2)
 *
 * 每个函数都有 x86_64/AVX2 实现和非 x86 标量回退。
 * ============================================================================ */

#ifndef SASS_SIMD_KERNELS_H
#define SASS_SIMD_KERNELS_H

#include "wrapper.h"
#include "simd_portable.h"
#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>

namespace sass {

// ─── 1. 离散拉普拉斯曲率 ───
inline void compute_laplacian_curvature(
    const std::vector<double>& energy,
    std::vector<double>& curvature)
{
#if defined(__x86_64__) || defined(_M_X64)
    {
        alignas(64) int32_t e32[4320];
        for (size_t i = 0; i < 4320; ++i)
            e32[i] = static_cast<int32_t>(energy[i] * 1000.0);

        for (size_t base = 0; base < 4320; base += 128) {
            size_t end = std::min(base + 128, size_t(4320));
            for (size_t j = base; j < end; j += 16) {
                VAVX3_512i c = load_512(&e32[j]);
                int32_t cl[18], cr[18], ct[18], cb[18];
                for (int k = 0; k < 16; ++k) {
                    size_t idx = j + k;
                    cl[k] = e32[(idx == 0) ? 4319 : idx - 1];
                    cr[k] = e32[(idx == 4319) ? 0 : idx + 1];
                    ct[k] = e32[(idx < 16) ? idx + 4304 : idx - 16];
                    cb[k] = e32[(idx >= 4304) ? idx - 4304 : idx + 16];
                }
                VAVX3_512i vl = load_512(cl), vr = load_512(cr);
                VAVX3_512i vt = load_512(ct), vb = load_512(cb);
                VAVX3_512i lap = laplacian(c, vl, vr, vt, vb);
                alignas(64) int32_t out[16];
#if defined(__x86_64__) || defined(_M_X64)
                _mm256_storeu_si256((__m256i*)out, lap.v0);
                _mm256_storeu_si256((__m256i*)(out + 8), lap.v1);
#else
                for (int k = 0; k < 16; ++k) out[k] = 0;
#endif
                for (int k = 0; k < 16 && (j + k) < 4320; ++k)
                    curvature[j + k] = static_cast<double>(out[k]) / 1000.0;
            }
        }
    }
#else
    for (size_t i = 0; i < 4320; ++i) {
        size_t l = (i == 0) ? 4319 : i - 1;
        size_t r = (i == 4319) ? 0 : i + 1;
        size_t t = (i < 16) ? i + 4304 : i - 16;
        size_t b = (i >= 4304) ? i - 4304 : i + 16;
        curvature[i] = energy[l] + energy[r] + energy[t] + energy[b] - 4.0 * energy[i];
    }
#endif
}

// ─── 2. Yamabe 流计算 ───
inline void compute_yamabe_flow(
    const std::vector<double>& curvature,
    std::vector<double>& flow)
{
#if defined(__x86_64__) || defined(_M_X64)
    {
        alignas(64) float cur_f[4320], flow_f[4320];
        for (size_t i = 0; i < 4320; ++i) cur_f[i] = static_cast<float>(curvature[i]);

        for (size_t base = 0; base < 4320; base += 128) {
            size_t end = std::min(base + 128, size_t(4320));
            for (size_t j = base; j < end; j += 16) {
                simd::f32x8 c0 = simd::f32x8_load(&cur_f[j]);
                simd::f32x8 c1 = simd::f32x8_load(&cur_f[j + 8]);
                simd::f32x8 scale = simd::f32x8_set1(0.125f);
                simd::f32x8 f0 = simd::f32x8_mul(c0, scale);
                simd::f32x8 f1 = simd::f32x8_mul(c1, scale);
                simd::f32x8 thresh = simd::f32x8_set1(0.01f);
                simd::f32x8 mask0 = simd::f32x8_cmp_gt(c0, thresh);
                simd::f32x8 mask1 = simd::f32x8_cmp_gt(c1, thresh);
                f0 = simd::f32x8_and(f0, mask0);
                f1 = simd::f32x8_and(f1, mask1);
                simd::f32x8_store(&flow_f[j], f0);
                simd::f32x8_store(&flow_f[j + 8], f1);
            }
        }
        for (size_t i = 0; i < 4320; ++i) flow[i] = static_cast<double>(flow_f[i]);
    }
#else
    for (size_t i = 0; i < 4320; ++i) {
        if (curvature[i] > 0.01) {
            flow[i] = curvature[i] * 0.125;
        }
    }
#endif
}

// ─── 3. Yamabe 平滑 ───
inline void compute_yamabe_smoothed(
    const std::vector<double>& energy,
    const std::vector<double>& flow,
    std::vector<double>& smoothed)
{
#if defined(__x86_64__) || defined(_M_X64)
    {
        simd::f32x8 q25 = simd::f32x8_set1(0.25f);
        alignas(64) float en_f[4320], sm_f[4320], flow_f[4320];
        for (size_t i = 0; i < 4320; ++i) {
            en_f[i] = static_cast<float>(energy[i]);
            flow_f[i] = static_cast<float>(flow[i]);
        }

        for (size_t base = 0; base < 4320; base += 128) {
            size_t end = std::min(base + 128, size_t(4320));
            for (size_t j = base; j < end; j += 16) {
                simd::f32x8 e0 = simd::f32x8_load(&en_f[j]);
                simd::f32x8 e1 = simd::f32x8_load(&en_f[j + 8]);
                simd::f32x8 f0 = simd::f32x8_load(&flow_f[j]);
                simd::f32x8 f1 = simd::f32x8_load(&flow_f[j + 8]);
                simd::f32x8 s0 = simd::f32x8_sub(e0, f0);
                simd::f32x8 s1 = simd::f32x8_sub(e1, f1);

                float nb_l[16], nb_r[16], nb_t[16], nb_b[16];
                for (int k = 0; k < 16; ++k) {
                    size_t idx = j + k;
                    nb_l[k] = flow_f[(idx == 0) ? 4319 : idx - 1];
                    nb_r[k] = flow_f[(idx == 4319) ? 0 : idx + 1];
                    nb_t[k] = flow_f[(idx < 16) ? idx + 4304 : idx - 16];
                    nb_b[k] = flow_f[(idx >= 4304) ? idx - 4304 : idx + 16];
                }
                simd::f32x8 fl0 = simd::f32x8_load(nb_l);
                simd::f32x8 fl1 = simd::f32x8_load(nb_l + 8);
                s0 = simd::f32x8_add(s0, simd::f32x8_mul(fl0, q25));
                s1 = simd::f32x8_add(s1, simd::f32x8_mul(fl1, q25));

                simd::f32x8 fr0 = simd::f32x8_load(nb_r);
                simd::f32x8 fr1 = simd::f32x8_load(nb_r + 8);
                s0 = simd::f32x8_add(s0, simd::f32x8_mul(fr0, q25));
                s1 = simd::f32x8_add(s1, simd::f32x8_mul(fr1, q25));

                simd::f32x8 ft0 = simd::f32x8_load(nb_t);
                simd::f32x8 ft1 = simd::f32x8_load(nb_t + 8);
                s0 = simd::f32x8_add(s0, simd::f32x8_mul(ft0, q25));
                s1 = simd::f32x8_add(s1, simd::f32x8_mul(ft1, q25));

                simd::f32x8 fb0 = simd::f32x8_load(nb_b);
                simd::f32x8 fb1 = simd::f32x8_load(nb_b + 8);
                s0 = simd::f32x8_add(s0, simd::f32x8_mul(fb0, q25));
                s1 = simd::f32x8_add(s1, simd::f32x8_mul(fb1, q25));

                simd::f32x8_store(&sm_f[j], s0);
                simd::f32x8_store(&sm_f[j + 8], s1);
            }
        }
        for (size_t i = 0; i < 4320; ++i)
            smoothed[i] = static_cast<double>(sm_f[i]);
    }
#else
    for (size_t i = 0; i < 4320; ++i) {
        size_t l = (i == 0) ? 4319 : i - 1;
        size_t r = (i == 4319) ? 0 : i + 1;
        size_t t = (i < 16) ? i + 4304 : i - 16;
        size_t b = (i >= 4304) ? i - 4304 : i + 16;
        smoothed[i] = energy[i] - flow[i]
                    + flow[l] * 0.25 + flow[r] * 0.25
                    + flow[t] * 0.25 + flow[b] * 0.25;
    }
#endif
}

} // namespace sass

#endif // SASS_SIMD_KERNELS_H
