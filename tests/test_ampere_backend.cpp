/* Ampere SM80 反汇编验证 */
#include "test_common.h"
#include "sass/device_backend.h"
#include "../src/sass/backends/volta_backend.h"

using namespace sass;
static AmpereBackend ab;

// Shared Volta opcodes (should still work on Ampere)
TEST(a_exit) { ASSERT_STR_EQ(ab.disassemble(0x000000000000794dULL),"EXIT"); return true; }
TEST(a_bra)  { ASSERT_STR_EQ(ab.disassemble(0xfffffff000007947ULL),"BRA"); return true; }
TEST(a_nop)  { ASSERT_STR_EQ(ab.disassemble(0x0000000000007918ULL),"NOP"); return true; }
TEST(a_stg)  { ASSERT_STR_EQ(ab.disassemble(0x0000000502007386ULL),"STG"); return true; }
TEST(a_ldg)  { ASSERT_STR_EQ(ab.disassemble(0x0000000002027381ULL),"LDG"); return true; }
TEST(a_bar)  { ASSERT_STR_EQ(ab.disassemble(0x0000000000007b1dULL),"BAR.SYNC"); return true; }

// Ampere-specific opcodes
TEST(a_mma)    { ASSERT_STR_EQ(ab.disassemble(0x0000000000001800ULL),"MMA.SYNC"); return true; }
TEST(a_hadd2)  { ASSERT_STR_EQ(ab.disassemble(0x0000000000000804ULL),"HADD2"); return true; }
TEST(a_iadd3)  { ASSERT_STR_EQ(ab.disassemble(0x0000000000008212ULL),"IADD3"); return true; }
TEST(a_bar2)   { ASSERT_STR_EQ(ab.disassemble(0x000000000000c0ffULL),"BAR"); return true; }
TEST(a_membar) { ASSERT_STR_EQ(ab.disassemble(0x000000000000d000ULL),"MEMBAR"); return true; }

TEST(a_arch) {
    ASSERT_TRUE(ab.arch().name.find("Ampere") != std::string::npos);
    ASSERT_EQ(ab.arch().compute_capability, 80);
    ASSERT_EQ(ab.total_instructions(), 150);
    return true;
}

// Factory test
TEST(factory_ampere) {
    auto be = DeviceBackend::create("sm_80");
    ASSERT_TRUE(be != nullptr);
    ASSERT_EQ(be->arch().compute_capability, 80);
    return true;
}
TEST(factory_volta) {
    auto be = DeviceBackend::create("sm_70");
    ASSERT_TRUE(be != nullptr);
    ASSERT_EQ(be->arch().compute_capability, 70);
    return true;
}
TEST(factory_pascal) {
    auto be = DeviceBackend::create("sm_61");
    ASSERT_TRUE(be != nullptr);
    ASSERT_EQ(be->arch().compute_capability, 61);
    return true;
}
TEST(factory_invalid) {
    auto be = DeviceBackend::create("sm_999");
    ASSERT_TRUE(be == nullptr);
    return true;
}

int main() { return run_all_tests(); }
