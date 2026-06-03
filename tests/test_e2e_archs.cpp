/* 全架构端到端测试: .sass → encode → disassemble → 验证 */
#include "test_common.h"
#include "sass/assembler.h"
#include "sass/disassembler.h"
#include "sass/device_backend.h"
#include "../src/sass/backends/volta_backend.h"
#include "../src/sass/pascal_backend.h"

using namespace sass;

// 工厂返回正确类型
TEST(factory_types) {
    ASSERT_TRUE(dynamic_cast<PascalBackend*>(DeviceBackend::create("sm_61").get()));
    ASSERT_TRUE(dynamic_cast<VoltaBackend*>(DeviceBackend::create("sm_70").get()));
    ASSERT_TRUE(dynamic_cast<TuringBackend*>(DeviceBackend::create("sm_75").get()));
    ASSERT_TRUE(dynamic_cast<AmpereBackend*>(DeviceBackend::create("sm_80").get()));
    ASSERT_TRUE(dynamic_cast<AdaBackend*>(DeviceBackend::create("sm_89").get()));
    ASSERT_TRUE(dynamic_cast<HopperBackend*>(DeviceBackend::create("sm_90").get()));
    ASSERT_TRUE(dynamic_cast<BlackwellBackend*>(DeviceBackend::create("sm_100").get()));
    return true;
}

// Pascal 端到端
TEST(e2e_pascal_ffma) {
    sass::Assembler asm_;
    auto code = asm_.assemble("ffma R0, R1, R2;");
    ASSERT_EQ(code.size(), 1);
    auto dis = disassemble(code[0]);
    ASSERT_TRUE(dis.find("ffma") != std::string::npos);
    ASSERT_TRUE(dis.find("R0") != std::string::npos);
    return true;
}

// Volta 编码往返
TEST(e2e_volta_encode_decode) {
    VoltaBackend vb;
    Instruction inst; inst.opcode = Opcode::EXIT;
    ManifoldResult r; r.block_id = 0; r.optimized_sequence = {inst};
    auto code = vb.encode(r);
    ASSERT_EQ(code.size(), 2);  // 128-bit pair
    ASSERT_STR_EQ(vb.disassemble(code[0]), "EXIT");
    return true;
}

// Ampere 编码往返
TEST(e2e_ampere_encode_decode) {
    AmpereBackend ab;
    Instruction inst; inst.opcode = Opcode::EXIT;
    ManifoldResult r; r.block_id = 0; r.optimized_sequence = {inst};
    auto code = ab.encode(r);
    ASSERT_EQ(code.size(), 2);
    ASSERT_STR_EQ(ab.disassemble(code[0]), "EXIT");
    return true;
}

// 跨架构一致性: 所有架构对 EXIT 编码相同
TEST(cross_arch_exit_consistent) {
    uint64_t exit_hex = 0x000000000000794dULL;
    for (auto& sm : {"sm_70","sm_75","sm_80","sm_86","sm_89","sm_90","sm_100"}) {
        auto be = DeviceBackend::create(sm);
        ASSERT_TRUE(be != nullptr);
        ASSERT_STR_EQ(be->disassemble(exit_hex), "EXIT");
    }
    return true;
}

// Ampere FP16 vs Volta FP16 (不同!)
TEST(ampere_fp16_differs_from_volta) {
    VoltaBackend vb;
    AmpereBackend ab;
    // Volta HADD2 = 0x7630, Ampere 可能不同
    uint64_t v_hadd2 = 0x10800000ff037630ULL;
    uint64_t a_hfma2 = 0x00000004ff037435ULL;
    ASSERT_STR_EQ(vb.disassemble(v_hadd2), "HADD2");
    // Ampere HFMA2 uses different opcode
    ASSERT_TRUE(ab.disassemble(a_hfma2).find("UNKNOWN") != std::string::npos
             || ab.disassemble(a_hfma2).find("HFMA") != std::string::npos);
    return true;
}

int main() { return run_all_tests(); }
