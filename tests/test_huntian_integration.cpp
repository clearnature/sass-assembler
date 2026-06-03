/* ============================================================================
 * HunTian SASS Assembler — huntian 集成层单元测试
 * ============================================================================ */

#include "test_common.h"
#include "sass/huntian_integration.h"

using namespace sass;

TEST(huntian_yamabe_zero_input) {
    // 零输入 → 零输出 (yamabe_flow: psi + laplacian/4 = 0 + 0/4 = 0)
    float psi[16] = {}, lap[16] = {}, out[16] = {};
    huntian_yamabe_flow_f32(psi, lap, out, 16);
    for (int i = 0; i < 16; ++i)
        ASSERT_TRUE(std::abs(out[i]) < 0.01f);
    return true;
}

TEST(huntian_yamabe_nonzero) {
    // psi=1.0, lap=0 → out ≈ 1.0 (psi unchanged)
    float psi[16], lap[16] = {}, out[16] = {};
    for (int i = 0; i < 16; ++i) psi[i] = 1.0f;
    huntian_yamabe_flow_f32(psi, lap, out, 16);
    for (int i = 0; i < 16; ++i)
        ASSERT_TRUE(std::abs(out[i] - 1.0f) < 0.01f);
    return true;
}

TEST(huntian_yamabe_deterministic) {
    float psi[16], lap[16], out1[16], out2[16];
    for (int i = 0; i < 16; ++i) { psi[i] = float(i); lap[i] = float(i % 3); }
    huntian_yamabe_flow_f32(psi, lap, out1, 16);
    huntian_yamabe_flow_f32(psi, lap, out2, 16);
    for (int i = 0; i < 16; ++i)
        ASSERT_EQ(out1[i], out2[i]);
    return true;
}

int main() {
    return run_all_tests();
}
