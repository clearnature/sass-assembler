/* Volta SM70 反汇编验证 — cuobjdump hex → 预期文本 */
#include "test_common.h"
#include "sass/device_backend.h"
#include "../src/sass/backends/volta_backend.h"

using namespace sass;
static VoltaBackend vb;

TEST(v_mov)  { ASSERT_STR_EQ(vb.disassemble(0x00000a0000017a02ULL),"MOV"); return true; }
TEST(v_s2r)  { ASSERT_STR_EQ(vb.disassemble(0x0000000000047919ULL),"S2R"); return true; }
TEST(v_nop)  { ASSERT_STR_EQ(vb.disassemble(0x0000000000007918ULL),"NOP"); return true; }
TEST(v_exit) { ASSERT_STR_EQ(vb.disassemble(0x000000000000794dULL),"EXIT"); return true; }
TEST(v_bra)  { ASSERT_STR_EQ(vb.disassemble(0xfffffff000007947ULL),"BRA"); return true; }
TEST(v_bar)  { ASSERT_STR_EQ(vb.disassemble(0x0000000000007b1dULL),"BAR.SYNC"); return true; }
TEST(v_ldg)  { ASSERT_STR_EQ(vb.disassemble(0x0000000002027381ULL),"LDG"); return true; }
TEST(v_stg)  { ASSERT_STR_EQ(vb.disassemble(0x0000000904007386ULL),"STG"); return true; }
TEST(v_sts)  { ASSERT_STR_EQ(vb.disassemble(0x0000000007007388ULL),"STS"); return true; }
TEST(v_imad) { ASSERT_STR_EQ(vb.disassemble(0x00005a0004027625ULL),"IMAD.WIDE"); return true; }
TEST(v_iadd) { ASSERT_STR_EQ(vb.disassemble(0x0000000000007812ULL),"IADD"); return true; }
TEST(v_isetp){ ASSERT_STR_EQ(vb.disassemble(0x0000003f0000780cULL),"ISETP"); return true; }
TEST(v_shf)  { ASSERT_STR_EQ(vb.disassemble(0x0000000204077819ULL),"SHF"); return true; }
TEST(v_i2f)  { ASSERT_STR_EQ(vb.disassemble(0x0000000200007308ULL),"I2F"); return true; }
TEST(v_sel)  { ASSERT_STR_EQ(vb.disassemble(0x0000000000007a0cULL),"SEL"); return true; }
TEST(v_ret)  { ASSERT_STR_EQ(vb.disassemble(0x0000000000007950ULL),"RET"); return true; }
TEST(v_ssy)  { ASSERT_STR_EQ(vb.disassemble(0x0000000000007944ULL),"SSY"); return true; }
TEST(v_cal)  { ASSERT_STR_EQ(vb.disassemble(0x0000000000007948ULL),"CAL"); return true; }
TEST(v_arch) { ASSERT_TRUE(vb.arch().name.find("Volta")!=std::string::npos); ASSERT_EQ(vb.arch().compute_capability,70); return true; }

TEST(v_unknown) {
    ASSERT_TRUE(vb.disassemble(0xDEADBEEF00000000ULL).find("UNKNOWN") != std::string::npos);
    return true;
}
int main() { return run_all_tests(); }
