/* ============================================================================
 * HunTian SASS 汇编器 — huntian 引擎集成适配层
 *
 * 桥接 huntian 的 int32 定点几何算子与调度器的 double/float 数据。
 *
 * 当前状态：
 *   ✅ vavx3_laplacian_512  — 已通过 simd_kernels.h 集成
 *   ✅ vavx3_yamabe_flow_512 — 本文件提供 float 适配
 *   ⬜ vavx3_topological_braid_512 — 待集成（需要量子态表示）
 *   ⬜ vavx3_geo_vortex_map_512 — 待集成（需要涡旋坐标）
 *
 * 用法：
 *   #include "huntian_integration.h"
 *   huntian_yamabe_flow_f32(psi_f32, lap_f32, output_f32, count);
 * ============================================================================ */

#ifndef SASS_HUNTIAN_INTEGRATION_H
#define SASS_HUNTIAN_INTEGRATION_H

#include "wrapper.h"
#include <cstring>

namespace sass {

// ─── 将 huntian yamabe_flow_512 适配为 float 数组接口 ───
// psi[i] 和 laplacian[i] 各为 float 数组，结果写入 output。
// 处理 count 个元素（应为 16 的倍数）。
#if defined(__x86_64__) || defined(_M_X64)
inline void huntian_yamabe_flow_f32(
    const float* psi, const float* laplacian, float* output, size_t count)
{
    for (size_t j = 0; j < count; j += 16) {
        // 转换为 huntian 期望的 int32 格式 (×1000 定点)
        alignas(64) int32_t psi_i32[16], lap_i32[16];
        for (int k = 0; k < 16; ++k) {
            psi_i32[k] = static_cast<int32_t>(psi[j + k] * 1000.0f);
            lap_i32[k] = static_cast<int32_t>(laplacian[j + k] * 1000.0f);
        }

        VAVX3_512i p = load_512(psi_i32);
        VAVX3_512i l = load_512(lap_i32);
        VAVX3_512i result = yamabe_flow(p, l);

        alignas(64) int32_t out_i32[16];
        _mm256_storeu_si256((__m256i*)out_i32, result.v0);
        _mm256_storeu_si256((__m256i*)(out_i32 + 8), result.v1);

        for (int k = 0; k < 16; ++k)
            output[j + k] = static_cast<float>(out_i32[k]) / 1000.0f;
    }
}
#else
inline void huntian_yamabe_flow_f32(
    const float*, const float*, float*, size_t) { /* no-op on non-x86 */ }
#endif

} // namespace sass

#endif // SASS_HUNTIAN_INTEGRATION_H
