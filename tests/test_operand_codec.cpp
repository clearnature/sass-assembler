/* ============================================================================
 * HunTian SASS Assembler — OperandCodec 单元测试 (红灯)
 *
 * 测试 encode→decode 往返：每种操作数类型验证编码后的解码正确性。
 * ============================================================================ */

#include "test_common.h"
#include "sass/operand_codec.h"

using namespace sass;

// ═══════════════════════════════════════════════════════════════
// 1. REG 类型往返测试
// ═══════════════════════════════════════════════════════════════

TEST(codec_reg_roundtrip_r0) {
    uint16_t enc = encode_op_reg(0);
    ASSERT_TRUE(decode_op_is_reg(enc));
    ASSERT_EQ(decode_op_reg(enc), 0);
    ASSERT_FALSE(decode_op_is_imm(enc));
    ASSERT_FALSE(decode_op_is_mem(enc));
    return true;
}

TEST(codec_reg_roundtrip_r127) {
    uint16_t enc = encode_op_reg(127);
    ASSERT_TRUE(decode_op_is_reg(enc));
    ASSERT_EQ(decode_op_reg(enc), 127);
    return true;
}

TEST(codec_reg_roundtrip_r255) {
    uint16_t enc = encode_op_reg(255);
    ASSERT_TRUE(decode_op_is_reg(enc));
    ASSERT_EQ(decode_op_reg(enc), 255);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 2. IMM 类型往返测试 (14-bit 有符号)
// ═══════════════════════════════════════════════════════════════

TEST(codec_imm_roundtrip_zero) {
    uint16_t enc = encode_op_imm(0);
    ASSERT_TRUE(decode_op_is_imm(enc));
    ASSERT_EQ(decode_op_imm(enc), 0);
    ASSERT_FALSE(decode_op_is_reg(enc));
    ASSERT_FALSE(decode_op_is_mem(enc));
    return true;
}

TEST(codec_imm_roundtrip_positive) {
    uint16_t enc = encode_op_imm(42);
    ASSERT_TRUE(decode_op_is_imm(enc));
    ASSERT_EQ(decode_op_imm(enc), 42);
    return true;
}

TEST(codec_imm_roundtrip_negative) {
    uint16_t enc = encode_op_imm(-100);
    ASSERT_TRUE(decode_op_is_imm(enc));
    ASSERT_EQ(decode_op_imm(enc), -100);
    return true;
}

TEST(codec_imm_roundtrip_max) {
    uint16_t enc = encode_op_imm(8191);   // 14-bit max
    ASSERT_TRUE(decode_op_is_imm(enc));
    ASSERT_EQ(decode_op_imm(enc), 8191);
    return true;
}

TEST(codec_imm_roundtrip_min) {
    uint16_t enc = encode_op_imm(-8192);  // 14-bit min
    ASSERT_TRUE(decode_op_is_imm(enc));
    ASSERT_EQ(decode_op_imm(enc), -8192);
    return true;
}

TEST(codec_imm_clamp_overflow) {
    uint16_t enc = encode_op_imm(10000);  // > 8191, should clamp
    ASSERT_TRUE(decode_op_is_imm(enc));
    ASSERT_EQ(decode_op_imm(enc), 8191);
    return true;
}

TEST(codec_imm_clamp_underflow) {
    uint16_t enc = encode_op_imm(-10000); // < -8192, should clamp
    ASSERT_TRUE(decode_op_is_imm(enc));
    ASSERT_EQ(decode_op_imm(enc), -8192);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 3. MEM (base+offset) 类型往返测试
// ═══════════════════════════════════════════════════════════════

TEST(codec_mem_roundtrip_base0_off0) {
    uint16_t enc = encode_op_mem(0, 0);
    ASSERT_TRUE(decode_op_is_mem(enc));
    ASSERT_EQ(decode_op_base(enc), 0);
    ASSERT_EQ(decode_op_offset(enc), 0);
    ASSERT_FALSE(decode_op_is_reg(enc));
    ASSERT_FALSE(decode_op_is_imm(enc));
    return true;
}

TEST(codec_mem_roundtrip_base10_off5) {
    uint16_t enc = encode_op_mem(10, 5);
    ASSERT_TRUE(decode_op_is_mem(enc));
    ASSERT_EQ(decode_op_base(enc), 10);
    ASSERT_EQ(decode_op_offset(enc), 5);
    return true;
}

TEST(codec_mem_roundtrip_base_max_off_neg) {
    uint16_t enc = encode_op_mem(63, -128);  // base max 6-bit, offset min
    ASSERT_TRUE(decode_op_is_mem(enc));
    ASSERT_EQ(decode_op_base(enc), 63);
    ASSERT_EQ(decode_op_offset(enc), -128);
    return true;
}

TEST(codec_mem_roundtrip_off_max) {
    uint16_t enc = encode_op_mem(0, 127);
    ASSERT_TRUE(decode_op_is_mem(enc));
    ASSERT_EQ(decode_op_base(enc), 0);
    ASSERT_EQ(decode_op_offset(enc), 127);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 4. MEMS (纯偏移) 类型往返测试
// ═══════════════════════════════════════════════════════════════

TEST(codec_mems_roundtrip_zero) {
    uint16_t enc = encode_op_mems(0);
    ASSERT_TRUE(decode_op_is_mems(enc));
    ASSERT_EQ(decode_op_mems_offset(enc), 0);
    ASSERT_FALSE(decode_op_is_reg(enc));
    ASSERT_FALSE(decode_op_is_mem(enc));
    return true;
}

TEST(codec_mems_roundtrip_positive) {
    uint16_t enc = encode_op_mems(256);
    ASSERT_TRUE(decode_op_is_mems(enc));
    ASSERT_EQ(decode_op_mems_offset(enc), 256);
    return true;
}

TEST(codec_mems_roundtrip_negative) {
    uint16_t enc = encode_op_mems(-512);
    ASSERT_TRUE(decode_op_is_mems(enc));
    ASSERT_EQ(decode_op_mems_offset(enc), -512);
    return true;
}

TEST(codec_mems_roundtrip_max) {
    uint16_t enc = encode_op_mems(8191);
    ASSERT_TRUE(decode_op_is_mems(enc));
    ASSERT_EQ(decode_op_mems_offset(enc), 8191);
    return true;
}

TEST(codec_mems_roundtrip_min) {
    uint16_t enc = encode_op_mems(-8192);
    ASSERT_TRUE(decode_op_is_mems(enc));
    ASSERT_EQ(decode_op_mems_offset(enc), -8192);
    return true;
}

TEST(codec_mems_clamp) {
    uint16_t enc = encode_op_mems(10000);
    ASSERT_TRUE(decode_op_is_mems(enc));
    ASSERT_EQ(decode_op_mems_offset(enc), 8191);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 5. 边界与互斥验证
// ═══════════════════════════════════════════════════════════════

TEST(codec_tag_exclusivity) {
    // 每种类型的 tag 不应该被其他 is_* 误判
    uint16_t reg = encode_op_reg(5);
    ASSERT_TRUE(decode_op_is_reg(reg));
    ASSERT_FALSE(decode_op_is_imm(reg));
    ASSERT_FALSE(decode_op_is_mem(reg));
    ASSERT_FALSE(decode_op_is_mems(reg));

    uint16_t imm = encode_op_imm(5);
    ASSERT_FALSE(decode_op_is_reg(imm));
    ASSERT_TRUE(decode_op_is_imm(imm));
    ASSERT_FALSE(decode_op_is_mem(imm));
    ASSERT_FALSE(decode_op_is_mems(imm));

    uint16_t mem = encode_op_mem(5, 0);
    ASSERT_FALSE(decode_op_is_reg(mem));
    ASSERT_FALSE(decode_op_is_imm(mem));
    ASSERT_TRUE(decode_op_is_mem(mem));
    ASSERT_FALSE(decode_op_is_mems(mem));

    uint16_t mems = encode_op_mems(5);
    ASSERT_FALSE(decode_op_is_reg(mems));
    ASSERT_FALSE(decode_op_is_imm(mems));
    ASSERT_FALSE(decode_op_is_mem(mems));
    ASSERT_TRUE(decode_op_is_mems(mems));
    return true;
}

TEST(codec_reg_boundary_values) {
    // 寄存器 ID 0-255 全覆盖边界
    ASSERT_EQ(decode_op_reg(encode_op_reg(0)), 0);
    ASSERT_EQ(decode_op_reg(encode_op_reg(1)), 1);
    ASSERT_EQ(decode_op_reg(encode_op_reg(254)), 254);
    ASSERT_EQ(decode_op_reg(encode_op_reg(255)), 255);
    return true;
}

int main() {
    return run_all_tests();
}
