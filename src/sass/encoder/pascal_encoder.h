/* Pascal GP106 编码器 — 22 opcode 家族, 108 条指令
 *
 * 从 PascalBackend 拆分: 纯编码函数, 无状态
 * 来源: /data/rtl-sdr/ptx_gp106/docs/SASS_ENCODING_RULES.md + cuobjdump 验证
 */

#pragma once
#include <cstdint>
#include "instruction.h"

namespace sass::pascal {

// Pascal SM61 opcode bytes (cuobjdump verified)
namespace op {
    constexpr uint8_t EXIT     = 0xe3, BRA      = 0xe2, FFMA     = 0x59;
    constexpr uint8_t XMAD_A   = 0x4e, XMAD_B   = 0x51, XMAD_MRG = 0x4f;
    constexpr uint8_t XMAD_PSL = 0x5b, MOV32I   = 0x01;
    constexpr uint8_t ALU      = 0x4c, ALU2     = 0x5c, FLOAT    = 0xf0;
    constexpr uint8_t MEM      = 0xee, MEM2     = 0xef;
    constexpr uint8_t PRED     = 0x4b, PRED_IMM = 0x36, FSET_BF  = 0x48;
    constexpr uint8_t SHIFT    = 0x38, LOGIC    = 0x04, LOGIC3   = 0x3c;
    constexpr uint8_t MUFU     = 0x50, DFMA     = 0x53;
    constexpr uint8_t ATOM     = 0xed, SURFACE  = 0xeb;
    constexpr uint8_t TEXTURE  = 0xd8, TEXLD    = 0xda;
}

// ─── 辅助 ───
inline uint8_t reg(const Instruction& i, size_t n) {
    return (n<i.operands.size()&&i.operands[n].type==OpType::REG)?i.operands[n].reg_id:0;
}
inline bool has_reg(const Instruction& i, size_t n) {
    return n<i.operands.size()&&i.operands[n].type==OpType::REG;
}
inline uint64_t imm(const Instruction& i, size_t n) {
    return (n<i.operands.size()&&i.operands[n].type==OpType::IMM)?(uint64_t)i.operands[n].imm_val:0;
}

// ═══ 编码函数 (22 families) ═══

inline uint64_t enc_bra(const Instruction& inst) {
    int16_t off=(inst.operands.size()>0&&inst.operands[0].type==OpType::IMM)
        ?(int16_t)inst.operands[0].imm_val:0;
    uint64_t w=(uint64_t)op::BRA<<56; w|=0x400fffff87000000ULL;
    w|=((uint64_t)off&0xFFFF)<<8; return w;
}

inline uint64_t enc_ffma(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1);
    uint8_t rb=has_reg(inst,2)?reg(inst,2):0xFF;
    uint8_t rc=has_reg(inst,3)?reg(inst,3):0xFF;
    uint64_t w=(uint64_t)op::FFMA<<56; w|=0x8000ULL<<40;
    w|=(uint64_t)rc<<32; w|=(uint64_t)rb<<24;
    w|=(uint64_t)ra<<16; w|=rd; return w;
}

inline uint64_t enc_xmad(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1);
    uint8_t rb=has_reg(inst,2)?reg(inst,2):0, rc=has_reg(inst,3)?reg(inst,3):0xFF;
    uint64_t w=(uint64_t)op::XMAD_A<<56; w|=(uint64_t)rc<<32;
    w|=(uint64_t)rb<<24; w|=(uint64_t)ra<<16; w|=rd; return w;
}

inline uint64_t enc_xmad_mrg(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1), rb=has_reg(inst,2)?reg(inst,2):0;
    uint64_t w=(uint64_t)op::XMAD_MRG<<56; w|=0x107f800000ULL;
    w|=(uint64_t)rb<<16; w|=(uint64_t)ra<<8; w|=rd; return w;
}

inline uint64_t enc_xmad_psl(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1), rb=has_reg(inst,2)?reg(inst,2):0, rc=has_reg(inst,3)?reg(inst,3):0;
    uint64_t w=(uint64_t)op::XMAD_PSL<<56; w|=0x3000180000ULL;
    w|=(uint64_t)rc<<32; w|=(uint64_t)rb<<16; w|=(uint64_t)ra<<8; w|=rd; return w;
}

inline uint64_t enc_alu(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1), rb=has_reg(inst,2)?reg(inst,2):0;
    uint64_t w=(uint64_t)op::ALU<<56; w|=(uint64_t)ra<<16; w|=(uint64_t)rb<<8; w|=rd; return w;
}

inline uint64_t enc_mov32i(const Instruction& inst) {
    uint8_t rd=reg(inst,0);
    return ((uint64_t)op::MOV32I<<56)|rd;
}

inline uint64_t enc_float(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1), rb=has_reg(inst,2)?reg(inst,2):0;
    uint64_t w=(uint64_t)op::FLOAT<<56; w|=(uint64_t)ra<<16; w|=(uint64_t)rb<<8; w|=rd; return w;
}

inline uint64_t enc_mem(const Instruction& inst) {
    uint8_t r1=reg(inst,0), r2=has_reg(inst,1)?reg(inst,1):0;
    return ((uint64_t)op::MEM<<56)|((uint64_t)r2<<8)|r1;
}

inline uint64_t enc_mem2(const Instruction& inst) {
    uint8_t r1=reg(inst,0), r2=has_reg(inst,1)?reg(inst,1):0;
    return ((uint64_t)op::MEM2<<56)|((uint64_t)r2<<8)|r1;
}

inline uint64_t enc_pred(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1);
    return ((uint64_t)op::PRED<<56)|((uint64_t)rd<<16)|ra;
}

inline uint64_t enc_mufu(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1);
    return ((uint64_t)op::MUFU<<56)|((uint64_t)rd<<8)|ra;
}

inline uint64_t enc_dfma(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1), rb=has_reg(inst,2)?reg(inst,2):0;
    return ((uint64_t)op::DFMA<<56)|((uint64_t)rd<<16)|((uint64_t)ra<<8)|rb;
}

inline uint64_t enc_cvt(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1);
    return ((uint64_t)op::ALU2<<56)|((uint64_t)rd<<16)|ra;
}

inline uint64_t enc_shr(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1);
    return ((uint64_t)op::SHIFT<<56)|((uint64_t)rd<<16)|ra;
}

inline uint64_t enc_lop32i(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1);
    return ((uint64_t)op::LOGIC<<56)|((uint64_t)rd<<8)|ra;
}

inline uint64_t enc_lop3(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1), rb=has_reg(inst,2)?reg(inst,2):0;
    return ((uint64_t)op::LOGIC3<<56)|((uint64_t)rd<<16)|((uint64_t)ra<<8)|rb;
}

inline uint64_t enc_bfi(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1);
    return ((uint64_t)op::PRED_IMM<<56)|((uint64_t)rd<<16)|ra;
}

inline uint64_t enc_fset(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=reg(inst,1);
    return ((uint64_t)op::FSET_BF<<56)|((uint64_t)rd<<16)|ra;
}

inline uint64_t enc_tex(const Instruction& inst) {
    return ((uint64_t)op::TEXTURE<<56)|reg(inst,0);
}

inline uint64_t enc_tld(const Instruction& inst) {
    return ((uint64_t)op::TEXLD<<56)|reg(inst,0);
}

inline uint64_t enc_surface(const Instruction& inst) {
    uint8_t rd=reg(inst,0), ra=has_reg(inst,1)?reg(inst,1):0;
    return ((uint64_t)op::SURFACE<<56)|((uint64_t)rd<<16)|ra;
}

inline uint64_t enc_atom(const Instruction& inst) {
    uint8_t r1=reg(inst,0), r2=has_reg(inst,1)?reg(inst,1):0;
    return ((uint64_t)op::ATOM<<56)|((uint64_t)r2<<8)|r1;
}

inline uint64_t enc_huntian_fallback(const Instruction& inst) {
    uint64_t w=(uint64_t)(uint16_t)inst.opcode<<56;
    for(size_t i=0;i<inst.operands.size()&&i<3;i++)
        if(inst.operands[i].type==OpType::REG)
            w|=(uint64_t)inst.operands[i].reg_id<<(16*i);
    return w;
}

} // namespace sass::pascal
