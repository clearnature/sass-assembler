/* ============================================================================
 * HunTian SASS 汇编器 — 引擎包装层
 *
 * x86_64/AVX2: 使用 third_party/huntian/vavx3_primitives.h 真实引擎
 * 非 x86:      使用自包含的回退实现，保证可编译
 * ============================================================================ */

#ifndef SASS_WRAPPER_H
#define SASS_WRAPPER_H

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <cmath>
#include <algorithm>

// ─── 真实 VAVX3 引擎 (x86_64) ───
#if defined(__x86_64__) || defined(_M_X64)
#define SASS_AVX2_ENABLED 1

// 核心几何引擎 — 涡旋映射、旋转、拉普拉斯、拓扑编织、Yamabe 流
#include "third_party/huntian/vavx3_primitives.h"
#include "third_party/huntian/vavx3_blas.h"

namespace sass {
    using VAVX3_512i = ::vavx3_512i;

    // ─── 几何映射 ───
    inline VAVX3_512i geo_vortex_map(VAVX3_512i r, VAVX3_512i theta) {
        return ::vavx3_geo_vortex_map_512(r, theta);
    }
    inline void geo_rotate(VAVX3_512i* x, VAVX3_512i* y, VAVX3_512i angle) {
        ::vavx3_geo_rotate_512(x, y, angle);
    }
    inline void toroidal_inversion(VAVX3_512i* x, VAVX3_512i* y, int max_d_sq) {
        ::vavx3_geo_toroidal_inversion_512(x, y, max_d_sq);
    }

    // ─── 微分几何算子 ───
    inline VAVX3_512i laplacian(VAVX3_512i center, VAVX3_512i left, VAVX3_512i right,
                                VAVX3_512i top, VAVX3_512i bottom) {
        return ::vavx3_laplacian_512(center, left, right, top, bottom);
    }
    inline VAVX3_512i yamabe_flow(VAVX3_512i psi, VAVX3_512i laplacian) {
        return ::vavx3_yamabe_flow_512(psi, laplacian);
    }
    inline VAVX3_512i topological_braid(VAVX3_512i a, VAVX3_512i b) {
        return ::vavx3_topological_braid_512(a, b);
    }

    // ─── 向量加载 ───
    inline VAVX3_512i load_512(const void* p) {
        return ::vavx3_load_512(p);
    }

    inline uint64_t void_spin_4320(uint64_t state) {
        constexpr uint64_t MASK = 0x3FFFFFFFFFFFFFFFULL;
        return ((state >> 12) | (state << 52)) & MASK;
    }
} // namespace sass

#else
// ─── 非 x86 回退 ───
namespace sass {
struct VAVX3_512i { uint64_t data[8]; };
inline uint64_t void_spin_4320(uint64_t state) {
    constexpr uint64_t MASK = 0x3FFFFFFFFFFFFFFFULL;
    return ((state >> 12) | (state << 52)) & MASK;
}
} // namespace sass
#endif

namespace sass {

// ─── 4320D 流形调度辅助 ───
inline uint32_t manifold_slot(uint64_t seed, double complexity, double phase) {
    uint64_t s = void_spin_4320(seed + static_cast<uint64_t>(complexity * 100.0));
    return static_cast<uint32_t>((s + static_cast<uint64_t>(phase * 1000.0)) % 4320);
}

#if !(defined(__x86_64__) || defined(_M_X64))
inline double compute_curvature(const std::vector<double>& field, size_t i) {
    if (i == 0 || i >= field.size() - 1) return 0.0;
    return field[i + 1] + field[i - 1] - 2.0 * field[i];
}
#endif

} // namespace sass

#endif // SASS_WRAPPER_H
