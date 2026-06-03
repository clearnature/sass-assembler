/* HunTian SASS — denvdis 位级精确编码 DSL
 *
 * 来自 denvdis SM80 数据 (576 变体, 117 类)
 *
 * 编码原语:
 *   set(w, lo, hi, val)  — 设置位域
 *   encode(opcode, fields) — 组装 128-bit 指令对
 *
 * 位域布局 (denvdis 验证):
 *   Word 0 (bits[63:0]):  opcode[11:0], Pg[15:12], Rd[23:16], Ra[31:24], Rb[39:32], offset[63:40]
 *   Word 1 (bits[127:64]): Rc[71:64], modifiers[80:72], opcodeMSB[91], scoreboard[115:110], barrier[121:116]
 */

#pragma once
#include <cstdint>
#include "instruction.h"

namespace sass::dsl {

// ─── 位域操作 ───
struct BitField { int lo, hi; };
constexpr uint64_t mask(int lo, int hi) { return ((1ULL<<((hi)-(lo)+1))-1)<<(lo); }
inline uint64_t set(uint64_t w, int lo, int hi, uint64_t v) { return (w&~mask(lo,hi))|((v<<lo)&mask(lo,hi)); }
inline uint64_t get(uint64_t w, int lo, int hi) { return (w>>lo)&((1ULL<<((hi)-(lo)+1))-1); }

// ─── 指令字段布局 (denvdis 提取) ───
struct FieldLayout {
    BitField opcode;     // 例如: {0,10} + {91,91} for 13-bit
    BitField Pg;         // predicate guard: {12,14}
    BitField Pg_not;     // predicate not: {15,15}
    BitField Rd;         // {16,23}
    BitField Ra;         // {24,31}
    BitField Rb;         // {32,39}
    BitField Rc;         // {64,71}  (in word1)
    BitField offset;     // {40,63}
    BitField dst_sb;     // {110,112}
    BitField src_sb;     // {113,115}
    BitField barrier;    // {116,121}
    BitField opex;       // {122,124}
};

// FFMA from denvdis: opcode=13bits[91]+[10:0], Rd[23:16], Ra[31:24], Rb[39:32], Rc[71:64]
constexpr FieldLayout FFMA_LAYOUT = {
    {0,10}, {12,14}, {15,15}, {16,23}, {24,31}, {32,39}, {64,71}, {40,63}, {110,112}, {113,115}, {116,121}, {122,124}
};

// STG from denvdis: Ra[31:24], Rb[39:32], Ra_offset[63:40]
constexpr FieldLayout STG_LAYOUT = {
    {0,10}, {12,14}, {15,15}, {16,23}, {24,31}, {32,39}, {64,71}, {40,63}, {110,112}, {113,115}, {116,121}, {122,124}
};

// EXIT: fixed
constexpr FieldLayout EXIT_LAYOUT = { {0,15}, {12,14}, {15,15}, {16,23}, {24,31}, {32,39}, {64,71}, {40,63}, {110,112}, {113,115}, {116,121}, {122,124} };

// ─── 组装 128-bit 指令对 ───
inline std::pair<uint64_t,uint64_t> assemble(const FieldLayout& L,
    uint64_t opcode_val, uint8_t rd, uint8_t ra, uint8_t rb, uint8_t rc,
    uint64_t imm=0, bool predicate=true, uint8_t sb_dst=7, uint8_t sb_src=7)
{
    uint64_t w0=0, w1=0;
    // Opcode (up to 16 bits, split across words)
    w0 = set(w0, L.opcode.lo, L.opcode.lo+11, opcode_val & 0xFFF);
    if (L.opcode.hi > 63) w1 = set(w1, L.opcode.hi-64, L.opcode.hi-64, (opcode_val>>12)&1);
    // Predicate
    if (predicate) { w0=set(w0,L.Pg.lo,L.Pg.hi,7); w0=set(w0,L.Pg_not.lo,L.Pg_not.hi,0); }
    // Registers
    if (L.Rd.hi<64) { w0=set(w0,L.Rd.lo,L.Rd.hi,rd); }
    if (L.Ra.hi<64) { w0=set(w0,L.Ra.lo,L.Ra.hi,ra); }
    if (L.Rb.hi<64) { w0=set(w0,L.Rb.lo,L.Rb.hi,rb); }
    if (L.Rc.lo>=64) { w1=set(w1,L.Rc.lo-64,L.Rc.hi-64,rc); }
    // Immediate
    if (L.offset.hi<64) w0=set(w0,L.offset.lo,L.offset.hi,imm);
    // Scoreboards
    w1=set(w1,L.dst_sb.lo-64,L.dst_sb.hi-64,sb_dst);
    w1=set(w1,L.src_sb.lo-64,L.src_sb.hi-64,sb_src);
    return {w0,w1};
}

// ─── 指令级编码 ───
inline std::pair<uint64_t,uint64_t> encode_ffma(uint8_t rd,uint8_t ra,uint8_t rb,uint8_t rc){
    return assemble(FFMA_LAYOUT, 0x223, rd,ra,rb,rc);  // FFMA opcode=0b1000100011
}
inline std::pair<uint64_t,uint64_t> encode_stg(uint8_t addr_reg,uint8_t src_reg,int32_t off){
    return assemble(STG_LAYOUT, 0x1E6, 0,addr_reg,src_reg,0, off&0xFFFFFF);
}
inline std::pair<uint64_t,uint64_t> encode_exit(){
    return {0x794dULL, 0x0fe40000000f00ULL};
}
inline std::pair<uint64_t,uint64_t> encode_nop(){
    return {0x7918ULL, 0x0fe40000000f00ULL};
}

} // namespace sass::dsl
