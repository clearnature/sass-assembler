/* ============================================================================
 * HunTian SASS Assembler — SIMD 内核单元测试
 *
 * 验证 compute_laplacian_curvature / compute_yamabe_flow / compute_yamabe_smoothed
 * 的输出性质和边界行为。
 * ============================================================================ */

#include "test_common.h"
#include "../src/sass/simd_kernels.h"
#include <vector>
#include <cmath>

using namespace sass;

// ═══════════════════════════════════════════════════════════════
// 1. 拉普拉斯曲率
// ═══════════════════════════════════════════════════════════════

TEST(simd_laplacian_output_size) {
    std::vector<double> energy(4320, 1.0);
    std::vector<double> curvature(4320, -999.0);

    compute_laplacian_curvature(energy, curvature);

    // 所有 4320 个点都应该被写入
    for (size_t i = 0; i < 4320; ++i)
        ASSERT_TRUE(curvature[i] != -999.0);
    return true;
}

TEST(simd_laplacian_flat_field) {
    // 均匀能量场 → 曲率为 0（或接近 0）
    std::vector<double> energy(4320, 5.0);
    std::vector<double> curvature(4320, 0.0);

    compute_laplacian_curvature(energy, curvature);

    double max_abs = 0.0;
    for (double c : curvature) max_abs = std::max(max_abs, std::abs(c));
    ASSERT_TRUE(max_abs < 1.0);  // 应接近 0
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 2. Yamabe 流
// ═══════════════════════════════════════════════════════════════

TEST(simd_yamabe_flow_output_size) {
    std::vector<double> curvature(4320, 0.0);
    std::vector<double> flow(4320, -999.0);

    compute_yamabe_flow(curvature, flow);

    for (size_t i = 0; i < 4320; ++i)
        ASSERT_TRUE(flow[i] != -999.0);
    return true;
}

TEST(simd_yamabe_flow_zero_curvature) {
    // 零曲率 → 零流
    std::vector<double> curvature(4320, 0.0);
    std::vector<double> flow(4320, -1.0);

    compute_yamabe_flow(curvature, flow);

    for (size_t i = 0; i < 4320; ++i)
        ASSERT_EQ(flow[i], 0.0);
    return true;
}

TEST(simd_yamabe_flow_threshold) {
    // 曲率 < 0.01 → 流 = 0 (被阈值过滤)
    std::vector<double> curvature(4320, 0.005);
    std::vector<double> flow(4320, -1.0);

    compute_yamabe_flow(curvature, flow);

    for (size_t i = 0; i < 4320; ++i)
        ASSERT_EQ(flow[i], 0.0);
    return true;
}

TEST(simd_yamabe_flow_positive) {
    // 曲率 = 1.0 → 流 = 0.125
    std::vector<double> curvature(4320, 1.0);
    std::vector<double> flow(4320, 0.0);

    compute_yamabe_flow(curvature, flow);

    for (size_t i = 0; i < 4320; ++i)
        ASSERT_TRUE(std::abs(flow[i] - 0.125) < 0.001);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 3. Yamabe 平滑
// ═══════════════════════════════════════════════════════════════

TEST(simd_yamabe_smoothed_output_size) {
    std::vector<double> energy(4320, 1.0);
    std::vector<double> flow(4320, 0.0);
    std::vector<double> smoothed(4320, -999.0);

    compute_yamabe_smoothed(energy, flow, smoothed);

    for (size_t i = 0; i < 4320; ++i)
        ASSERT_TRUE(smoothed[i] != -999.0);
    return true;
}

TEST(simd_yamabe_smoothed_no_flow) {
    // flow = 0 → smoothed = energy
    std::vector<double> energy(4320, 3.0);
    std::vector<double> flow(4320, 0.0);
    std::vector<double> smoothed(4320, 0.0);

    compute_yamabe_smoothed(energy, flow, smoothed);

    for (size_t i = 0; i < 4320; ++i)
        ASSERT_TRUE(std::abs(smoothed[i] - 3.0) < 0.001);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 4. 确定性
// ═══════════════════════════════════════════════════════════════

TEST(simd_deterministic_laplacian) {
    std::vector<double> energy(4320, 0.0);
    for (size_t i = 0; i < 4320; ++i) energy[i] = static_cast<double>(i % 100);

    std::vector<double> c1(4320, 0.0), c2(4320, 0.0);
    compute_laplacian_curvature(energy, c1);
    compute_laplacian_curvature(energy, c2);

    for (size_t i = 0; i < 4320; ++i)
        ASSERT_EQ(c1[i], c2[i]);
    return true;
}

int main() {
    return run_all_tests();
}
