/* ============================================================================
 * HunTian SASS 汇编器 — 操作数编解码器
 *
 * 4 种操作数类型的 16 位编码/解码工具：
 *   [15:14] 类型标签 → 00=REG / 01=IMM / 10=MEM(b) / 11=MEM(s)
 *   [13:0]  负载数据 (reg_id / 有符号立即数 / base+offset / 纯偏移)
 * ============================================================================ */

#ifndef SASS_OPERAND_CODEC_H
#define SASS_OPERAND_CODEC_H

#include <cstdint>

namespace sass {

// ─── 操作数类型标签 ───
enum OperandTag : uint16_t {
    TAG_REG  = 0x0000,  // 00
    TAG_IMM  = 0x4000,  // 01
    TAG_MEMB = 0x8000,  // 10  base+offset
    TAG_MEMS = 0xC000,  // 11  纯偏移
    TAG_MASK = 0xC000
};

// ─── 编码 ───
inline uint16_t encode_op_reg(uint8_t reg_id) {
    return static_cast<uint16_t>(reg_id) & 0xFF;  // 00 | 000000 | reg_id
}

inline uint16_t encode_op_imm(int16_t val) {
    // 14-bit signed, clamp
    if (val > 8191) val = 8191;
    if (val < -8192) val = -8192;
    return TAG_IMM | (static_cast<uint16_t>(val & 0x3FFF));
}

inline uint16_t encode_op_mem(uint8_t base, int8_t offset) {
    return TAG_MEMB | (static_cast<uint16_t>(base & 0x3F) << 8)
                     | (static_cast<uint16_t>(offset & 0xFF));
}

inline uint16_t encode_op_mems(int16_t offset) {
    if (offset > 8191) offset = 8191;
    if (offset < -8192) offset = -8192;
    return TAG_MEMS | (static_cast<uint16_t>(offset & 0x3FFF));
}

// ─── 解码：类型判断 ───
inline bool decode_op_is_reg(uint16_t op) {
    return (op & TAG_MASK) == TAG_REG;
}
inline bool decode_op_is_imm(uint16_t op) {
    return (op & TAG_MASK) == TAG_IMM;
}
inline bool decode_op_is_mem(uint16_t op) {
    return (op & TAG_MASK) == TAG_MEMB;
}
inline bool decode_op_is_mems(uint16_t op) {
    return (op & TAG_MASK) == TAG_MEMS;
}

// ─── 解码：值提取 ───
inline uint8_t decode_op_reg(uint16_t op) {
    return op & 0xFF;
}
inline int16_t decode_op_imm(uint16_t op) {
    uint16_t v = op & 0x3FFF;
    if (v & 0x2000) v |= 0xC000;  // sign extend 14→16 bit
    return static_cast<int16_t>(v);
}
inline uint8_t decode_op_base(uint16_t op) {
    return (op >> 8) & 0x3F;
}
inline int8_t decode_op_offset(uint16_t op) {
    return static_cast<int8_t>(op & 0xFF);
}
inline int16_t decode_op_mems_offset(uint16_t op) {
    uint16_t v = op & 0x3FFF;
    if (v & 0x2000) v |= 0xC000;
    return static_cast<int16_t>(v);
}

} // namespace sass

#endif // SASS_OPERAND_CODEC_H
