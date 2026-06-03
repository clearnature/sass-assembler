/* Ada/Hopper/Blackwell 独立编码验证 */
#include "test_common.h"
#include "sass/device_backend.h"
#include "sass/instruction.h"
#include "../src/sass/backends/volta_backend.h"
using namespace sass;
static uint64_t w0(const std::vector<SASSWord>& c,size_t i){return i*2<c.size()?c[i*2]:0;}
static ManifoldResult s(const Instruction& i){ManifoldResult r;r.block_id=0;r.optimized_sequence={i};return r;}
static Instruction r0(Opcode o){Instruction i;i.opcode=o;return i;}
static Instruction r2(Opcode o,uint8_t d,uint8_t s1){Instruction i;i.opcode=o;i.operands.push_back({OpType::REG,{.reg_id=d}});i.operands.push_back({OpType::REG,{.reg_id=s1}});return i;}

// Ada SM89
TEST(ada_exit){AdaBackend b;auto c=b.encode(s(r0(Opcode::EXIT)));ASSERT_EQ(w0(c,0)&0xFFFF,0x794d);return true;}
TEST(ada_stg) {AdaBackend b;auto c=b.encode(s(r2(Opcode::STG,2,5)));ASSERT_TRUE(w0(c,0)!=0||c.size()>=2);return true;}
TEST(ada_ldg) {AdaBackend b;auto c=b.encode(s(r2(Opcode::LDG,0,2)));ASSERT_TRUE(w0(c,0)!=0||c.size()>=2);return true;}
TEST(ada_s2r) {AdaBackend b;auto c=b.encode(s(r2(Opcode::S2R,4,0)));ASSERT_EQ(w0(c,0)&0xFFFF,0x7919);return true;}
TEST(ada_arch){AdaBackend b;ASSERT_EQ(b.arch().compute_capability,89);return true;}

// Hopper SM90
TEST(hop_exit){HopperBackend b;auto c=b.encode(s(r0(Opcode::EXIT)));ASSERT_EQ(w0(c,0)&0xFFFF,0x794d);return true;}
TEST(hop_stg) {HopperBackend b;auto c=b.encode(s(r2(Opcode::STG,2,5)));ASSERT_TRUE(w0(c,0)!=0||c.size()>=2);return true;}
TEST(hop_arch){HopperBackend b;ASSERT_EQ(b.arch().compute_capability,90);return true;}

// Blackwell SM100
TEST(bw_exit) {BlackwellBackend b;auto c=b.encode(s(r0(Opcode::EXIT)));ASSERT_EQ(w0(c,0)&0xFFFF,0x794d);return true;}
TEST(bw_stg)  {BlackwellBackend b;auto c=b.encode(s(r2(Opcode::STG,2,5)));ASSERT_TRUE(w0(c,0)!=0||c.size()>=2);return true;}
TEST(bw_arch) {BlackwellBackend b;ASSERT_EQ(b.arch().compute_capability,100);return true;}

// 工厂 + 跨架构一致性
TEST(factory_all_7){for(auto& sm:{"sm_61","sm_70","sm_75","sm_80","sm_86","sm_89","sm_90","sm_100"}){ASSERT_TRUE(DeviceBackend::create(sm)!=nullptr);}return true;}
TEST(cross_arch_exit_794d){for(auto& sm:{"sm_70","sm_75","sm_80","sm_86","sm_89","sm_90","sm_100"}){auto b=DeviceBackend::create(sm);ASSERT_STR_EQ(b->disassemble(0x000000000000794dULL),"EXIT");}return true;}

int main(){return run_all_tests();}
