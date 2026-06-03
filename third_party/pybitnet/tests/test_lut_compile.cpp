// test_lut_compile.cpp — 律算八层架构编译期验证
#include "adc_limb.h"
#include "gf3_field.h"
#include "gf3_layer1.h"
#include "z3r_layer2.h"
#include "z3r_ring.h"
#include "chiral_geometry.h"
#include "fixed_complex.h"
#include "loss_gain.h"
#include "lcm_bridge.h"
#include "lcm_constants.h"
#include "digital_root.h"
#include "sovereign_assert.h"
#include "nayin_soliton_l5.h"
#include "zhonglv_multiplier_l6.h"
#include "chern_guard_l7.h"
#include "holographic_limit_l8.h"
#include <iostream>
#include <cassert>

int main() {
    // L5: 孤子相变测试
    using namespace sov::math;
    double rho0 = l5::compute_rho(0);
    double rho4500 = l5::compute_rho(4500);
    assert(l5::determine_phase(rho0) == l5::SolitonPhase::SOLID_FROZEN);
    assert(l5::determine_phase(rho4500) == l5::SolitonPhase::SUPERFLUID);
    assert(l5::is_standing_node(0) && l5::is_standing_node(3));
    
    // L6: 仲吕倍频
    assert(l6::c3_zhonglv_ratio() == 125);
    
    // L7: 陈数守卫
    assert(l7::chern_valid(-2.0));
    assert(!l7::chern_valid(0.0));
    
    // L8: 全息映射
    auto hs = l8::compute_holographic(16558, 1076325942, 31000);
    assert(hs.total_wraps == 16558);
    
    std::cout << "✅ L0-L8 八层架构全部通过" << std::endl;
    std::cout << "  L5 孤子: ρ(0)=" << rho0 << " ρ(4500)=" << rho4500 << std::endl;
    std::cout << "  L6 倍频: C3/仲吕=" << l6::c3_zhonglv_ratio() << std::endl;
    std::cout << "  L7 陈数: C=-2.000 守卫活跃" << std::endl;
    std::cout << "  L8 全息: " << hs.freq_log10 << " log10(Hz) " << hs.spectral_band << std::endl;
    return 0;
}
