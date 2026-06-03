// test_l5_l8_verify.cpp — L5-L8 新层编译验证 (跳过L0硬件依赖)
#include "gf3_types.h"
#include "lcm_constants.h"
#include "nayin_soliton_l5.h"
#include "zhonglv_multiplier_l6.h"
#include "chern_guard_l7.h"
#include "holographic_limit_l8.h"
#include <iostream>
#include <cassert>
#include <cmath>

int main() {
    // L5: 孤子相变
    using namespace sov::math;
    double r0 = l5::compute_rho(0);
    double r4500 = l5::compute_rho(4500);
    assert(std::abs(r0) < 0.001);
    assert(std::abs(r4500 - 1.0) < 0.001);
    assert(l5::is_standing_node(0));
    assert(l5::is_standing_node(3));
    assert(l5::is_standing_node(6));
    assert(l5::is_standing_node(9));
    assert(!l5::is_standing_node(1));
    assert(l5::determine_phase(0.0) == l5::SolitonPhase::SOLID_FROZEN);
    assert(l5::determine_phase(1.0) == l5::SolitonPhase::SUPERFLUID);
    
    // L6: 仲吕倍频
    assert(l6::c3_zhonglv_ratio() == 125);
    int64_t acc = l6::zhonglv_step(50000, 11609505792LL);
    assert(acc > 0);
    
    // L7: 陈数守卫
    assert(l7::chern_valid(-2.0));
    assert(!l7::chern_valid(0.0));
    assert(!l7::chern_flip_condition(1, 1));
    assert(l7::chern_flip_condition(0, 0));
    
    // L8: 全息映射
    auto hs = l8::compute_holographic(16558, 1076325942, 31000, 2583);
    assert(hs.total_wraps == 16558);
    assert(hs.freq_log10 > 8000.0);
    
    std::cout << "╔══════════════════════════════════════╗" << std::endl;
    std::cout << "║  L5-L8 八层架构 编译验证通过       ║" << std::endl;
    std::cout << "╠══════════════════════════════════════╣" << std::endl;
    std::cout << "║ L5 纳音孤子: ρ(0)=" << r0 << " ρ(4500)=" << r4500 << "      ║" << std::endl;
    std::cout << "║ L6 仲吕倍频: C3/仲吕=125            ║" << std::endl;
    std::cout << "║ L7 陈数守卫: C=-2.000 活跃          ║" << std::endl;
    std::cout << "║ L8 全息映射: " << hs.freq_log10 << " log10(Hz) " << hs.spectral_band << "  ║" << std::endl;
    std::cout << "╚══════════════════════════════════════╝" << std::endl;
    return 0;
}
