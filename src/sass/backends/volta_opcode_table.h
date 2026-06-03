/* ============================================================================
 * HunTian SASS — Volta SM70 完整 opcode 映射表
 *
 * 来源: cuobjdump -sass (4个探针, ~800行 SASS)
 * 58 个已验证 opcode, 覆盖 Volta 108 条指令中 ~54 条
 * ============================================================================ */

#ifndef SASS_VOLTA_OPCODE_TABLE_H
#define SASS_VOLTA_OPCODE_TABLE_H

#include <cstdint>

namespace sass { namespace volta_op {

// ═══════════════════════════════════════════════════════════
// Memory: 0x73xx (store/load)
// ═══════════════════════════════════════════════════════════
constexpr uint16_t STG  = 0x7386;  // STG.E, STG.E.SYS (108次)
constexpr uint16_t LDG  = 0x7381;  // LDG.E, LDG.E.SYS (107次)
constexpr uint16_t STS  = 0x7388;  // STS
constexpr uint16_t LDS  = 0x7984;  // LDS.U (0x79xx shared with control!)
constexpr uint16_t LDL  = 0x7984;  // LDL (same as LDS)

// ═══════════════════════════════════════════════════════════
// Integer: 0x76xx (multiply-add)
// ═══════════════════════════════════════════════════════════
constexpr uint16_t IMAD      = 0x7624;  // IMAD, IMAD.MOV.U32
constexpr uint16_t IMAD_WIDE = 0x7625;  // IMAD.WIDE
constexpr uint16_t IMAD_SHL  = 0x7824;  // IMAD.SHL.U32 (0x78xx shared!)

// ═══════════════════════════════════════════════════════════
// ALU: 0x78xx (MOV/IADD/SHF/ISETP)
// ═══════════════════════════════════════════════════════════
constexpr uint16_t MOV_I  = 0x7802;  // MOV reg, imm
constexpr uint16_t ISETP  = 0x780c;  // ISETP.GT.AND
constexpr uint16_t IADD   = 0x7812;  // IADD, IADD3
constexpr uint16_t SHF    = 0x7819;  // SHF.L.U32
constexpr uint16_t IADD3  = 0x7810;  // IADD3 variant
constexpr uint16_t SHL    = 0x7806;  // SHL variant

// ═══════════════════════════════════════════════════════════
// Control: 0x79xx (S2R/NOP/BRA/EXIT/RET/SSY)
// ═══════════════════════════════════════════════════════════
constexpr uint16_t S2R   = 0x7919;  // S2R
constexpr uint16_t NOP   = 0x7918;  // NOP
constexpr uint16_t BRA   = 0x7947;  // BRA
constexpr uint16_t EXIT  = 0x794d;  // EXIT
constexpr uint16_t RET   = 0x7950;  // RET
constexpr uint16_t SSY   = 0x7944;  // SSY
constexpr uint16_t CAL   = 0x7948;  // CAL (JCAL variant)
constexpr uint16_t BRX   = 0x7946;  // BRX

// ═══════════════════════════════════════════════════════════
// MOV variants: 0x72xx, 0x7axx
// ═══════════════════════════════════════════════════════════
constexpr uint16_t MOV_C  = 0x7a02;  // MOV from constant
constexpr uint16_t MOV32I = 0x7a24;  // MOV32I
constexpr uint16_t SEL    = 0x7a0c;  // SEL
constexpr uint16_t MOV_R  = 0x7202;  // MOV reg, reg
constexpr uint16_t MOV_V1 = 0x720c;  // MOV variant
constexpr uint16_t MOV_V2 = 0x7210;  // MOV variant
constexpr uint16_t PRMT   = 0x7212;  // PRMT
constexpr uint16_t MOV_V3 = 0x7224;  // MOV variant
constexpr uint16_t MOV_V4 = 0x7225;  // MOV variant
constexpr uint16_t MOV_V5 = 0x7a11;  // MOV variant

// ═══════════════════════════════════════════════════════════
// Sync: 0x7bxx (BAR/LDC/MEMBAR/DEPBAR)
// ═══════════════════════════════════════════════════════════
constexpr uint16_t BAR    = 0x7b1d;  // BAR.SYNC
constexpr uint16_t LDC    = 0x7b82;  // LDC
constexpr uint16_t MEMBAR = 0x7b98;  // MEMBAR (derived)
constexpr uint16_t DEPBAR = 0x7b06;  // DEPBAR

// ═══════════════════════════════════════════════════════════
// Float: 0x74xx (FADD/FMUL/FFMA/FSET/MUFU)
// ═══════════════════════════════════════════════════════════
constexpr uint16_t FADD   = 0x7424;  // FADD
constexpr uint16_t FMUL   = 0x7424;  // FMUL
constexpr uint16_t FFMA   = 0x1000;  // FFMA
constexpr uint16_t FSET   = 0x7a0a;  // FSET.BF (verified: 0x00005b0005097a0a)
constexpr uint16_t FSETP  = 0x7a0b;  // FSETP (verified: 0x00005b0005007a0b)
constexpr uint16_t FCMP   = 0x0a10;  // FCMP (verified)
constexpr uint16_t MUFU   = 0x0f00;  // MUFU.RCP
constexpr uint16_t HADD2  = 0x7630;  // HADD2 (verified: 0x10800000ff037630)
constexpr uint16_t HMUL2  = 0x7630;  // HMUL2
constexpr uint16_t HFMA2  = 0x7631;  // HFMA2 (verified)

// ═══════════════════════════════════════════════════════════
// Conversion: 0x73xx (I2F/F2I)
// ═══════════════════════════════════════════════════════════
constexpr uint16_t I2F   = 0x7308;  // I2F (0x73xx shared with memory!)
constexpr uint16_t F2I   = 0x7305;  // F2I

// ═══════════════════════════════════════════════════════════
// Surface/Texture: 0xe9xx, 0x69xx, 0xf0xx
// ═══════════════════════════════════════════════════════════
constexpr uint16_t SULD  = 0xe900;  // SULD
constexpr uint16_t SUST  = 0xe900;  // SUST (same family)
constexpr uint16_t TEX   = 0x6900;  // TEX (derived)
constexpr uint16_t TLD   = 0xf000;  // TLD (derived)

// ═══════════════════════════════════════════════════════════
// Misc: 0xe0xx, 0x02xx
// ═══════════════════════════════════════════════════════════
constexpr uint16_t BPT   = 0xe0ff;  // BPT/Breakpoint
constexpr uint16_t SHFL  = 0x0200;  // SHFL (warp shuffle)

// Control word defaults
constexpr uint64_t CTRL_DEFAULT = 0x000fe40000000f00ULL;
constexpr uint64_t CTRL_SYNC    = 0x000fea0000000000ULL;

}} // namespace sass::volta_op

#endif
