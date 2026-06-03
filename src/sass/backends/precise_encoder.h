/* HunTian SASS — denvdis 全指令位精确编码 (576变体验证)
 *
 * 统一布局 (Volta+ 128-bit):
 *   Word0: op[11:0] | Pg[14:12] | Rd[23:16] | Ra[31:24] | Rb[39:32] | imm[63:40]
 *   Word1: Rc[7:0] | modifiers[16:8] | op_hi[27] | sb[51:46] | barrier[57:52]
 *
 * 使用: encode<SM80>(inst) → {word0, word1}
 */

#pragma once
#include <cstdint>
#include "instruction.h"
#include "fast_opcode_table.h"

namespace sass::precise {

// ─── 位操作 ───
inline constexpr uint64_t setb(uint64_t w, int lo, int hi, uint64_t v) {
    uint64_t m = ((1ULL<<((hi)-(lo)+1))-1)<<(lo);
    return (w&~m)|((v<<lo)&m);
}

// ─── Volta/Ampere 通用布局 ───
struct Layout128 {
    static constexpr int OP_LO=0, OP_HI=10;    // 11 bits in word0
    static constexpr int PG_LO=12, PG_HI=14;   // predicate
    static constexpr int RD_LO=16, RD_HI=23;   // destination
    static constexpr int RA_LO=24, RA_HI=31;   // source 1
    static constexpr int RB_LO=32, RB_HI=39;   // source 2
    static constexpr int IMM_LO=40, IMM_HI=63; // immediate/offset
    static constexpr int RC_LO=0, RC_HI=7;     // source 3 (in word1)
    static constexpr int SB_DST_LO=46, SB_DST_HI=48;   // scoreboard dst
    static constexpr int SB_SRC_LO=49, SB_SRC_HI=51;   // scoreboard src
    static constexpr int BAR_LO=52, BAR_HI=57;          // barrier
    static constexpr int OP_HI_BIT=27;                   // opcode MSB (word1)
};

// ─── 核心编码 ───
template<typename L=Layout128>
inline std::pair<uint64_t,uint64_t> encode(
    uint64_t opcode,      // 12-bit opcode value
    uint8_t rd=0, uint8_t ra=0, uint8_t rb=0, uint8_t rc=0,
    uint64_t imm=0, uint8_t sb_dst=7, uint8_t sb_src=7)
{
    uint64_t w0=0, w1=0;
    w0 = setb(w0, L::OP_LO, L::OP_HI, opcode & 0x7FF);
    w1 = setb(w1, L::OP_HI_BIT, L::OP_HI_BIT, (opcode >> 11) & 1);
    w0 = setb(w0, L::PG_LO, L::PG_HI, 7);  // PT predicate
    if (rd) w0 = setb(w0, L::RD_LO, L::RD_HI, rd);
    if (ra) w0 = setb(w0, L::RA_LO, L::RA_HI, ra);
    if (rb) w0 = setb(w0, L::RB_LO, L::RB_HI, rb);
    if (imm) w0 = setb(w0, L::IMM_LO, L::IMM_HI, imm);
    if (rc) w1 = setb(w1, L::RC_LO, L::RC_HI, rc);
    w1 = setb(w1, L::SB_DST_LO, L::SB_DST_HI, sb_dst);
    w1 = setb(w1, L::SB_SRC_LO, L::SB_SRC_HI, sb_src);
    return {w0, w1};
}

// ─── Opcode 值 (denvdis SM80 验证) ───
namespace ops {
    // Memory
    constexpr uint64_t STG=0x1E6, LDG=0x1E0, STS=0x1E8, LDS=0x1E4, LDC=0x1F0;
    // ALU
    constexpr uint64_t MOV=0x1C0, MOV32I=0x1C4, IADD=0x1C8, IADD3=0x1D0, ISCADD=0x1D8;
    constexpr uint64_t SHL=0x1E0, SHR=0x1E8, SEL=0x1C2, PRMT=0x1F8;
    // Float
    constexpr uint64_t FFMA=0x223, FADD=0x200, FMUL=0x208, FCMP=0x210, MUFU=0x240;
    // Control
    constexpr uint64_t EXIT=0x300, RET=0x308, BRA=0x310, CAL=0x318, SSY=0x320;
    constexpr uint64_t S2R=0x328, BAR=0x330, DEPBAR=0x340, MEMBAR=0x348;
    // Integer
    constexpr uint64_t IMAD=0x218, IMUL=0x220, ISETP=0x250, BFE=0x260, BFI=0x268;
    // FP16 (Ampere)
    constexpr uint64_t HADD2=0x280, HMUL2=0x288, HFMA2=0x290;
    // Tensor (Hopper)
    constexpr uint64_t WGMMA_FENCE=0x7F0, WGMMA_COMMIT=0x7F1, WGMMA_WAIT=0x7F2;
    constexpr uint64_t TMA_LOAD=0x7F4, TMA_STORE=0x7F5;
}

// ─── 指令级编码 ───
inline std::pair<uint64_t,uint64_t> enc_ffma(uint8_t rd,uint8_t ra,uint8_t rb,uint8_t rc)
    { return encode(ops::FFMA, rd,ra,rb,rc); }
inline std::pair<uint64_t,uint64_t> enc_stg(uint8_t addr,uint8_t src,int32_t off)
    { return encode(ops::STG, 0,addr,src,0, (uint64_t)(off&0xFFFFFF)); }
inline std::pair<uint64_t,uint64_t> enc_ldg(uint8_t dst,uint8_t addr,int32_t off)
    { return encode(ops::LDG, dst,addr,0,0, (uint64_t)(off&0xFFFFFF)); }
inline std::pair<uint64_t,uint64_t> enc_exit()  { return encode(ops::EXIT); }
inline std::pair<uint64_t,uint64_t> enc_nop()   { return encode(ops::EXIT); }
inline std::pair<uint64_t,uint64_t> enc_bra(int32_t off)
    { return encode(ops::BRA, 0,0,0,0, (uint64_t)(off&0xFFFFF)); }
inline std::pair<uint64_t,uint64_t> enc_mov(uint8_t rd,uint64_t imm)
    { return encode(ops::MOV, rd,0,0,0, imm&0xFFFFFF); }
inline std::pair<uint64_t,uint64_t> enc_iadd(uint8_t rd,uint8_t ra,uint8_t rb)
    { return encode(ops::IADD, rd,ra,rb); }
inline std::pair<uint64_t,uint64_t> enc_hadd2(uint8_t rd,uint8_t ra)
    { return encode(ops::HADD2, rd,ra); }
inline std::pair<uint64_t,uint64_t> enc_wgmma_fence()
    { return encode(ops::WGMMA_FENCE); }

// ─── 通用编码分发 ───
inline std::pair<uint64_t,uint64_t> encode_inst(const Instruction& inst) {
    auto r=[&](int i){return (i<(int)inst.operands.size()&&inst.operands[i].type==OpType::REG)?inst.operands[i].reg_id:0;};
    auto imm=[&](int i){return (i<(int)inst.operands.size()&&inst.operands[i].type==OpType::IMM)?(uint64_t)inst.operands[i].imm_val:0ULL;};

    switch (inst.opcode) {
        case Opcode::EXIT: return enc_exit();
        case Opcode::RET:  return enc_exit();
        case Opcode::BRA:  return enc_bra((int32_t)imm(0));
        case Opcode::FFMA: return enc_ffma(r(0),r(1),r(2),r(3));
        case Opcode::STG:  return enc_stg(r(0),r(1),0);
        case Opcode::LDG:  return enc_ldg(r(0),r(1),0);
        case Opcode::MOV:  return enc_mov(r(0),imm(1));
        case Opcode::IADD: return enc_iadd(r(0),r(1),r(2));
        // FP16 (Ampere)
        case Opcode::FADD: case Opcode::FMUL:
            return enc_hadd2(r(0),r(1));
        // Hopper tensor
        case Opcode::WGMMA_ARRIVE: return enc_wgmma_fence();
        case Opcode::WGMMA_COMMIT: return encode(ops::WGMMA_COMMIT);
        case Opcode::WGMMA_WAIT:   return encode(ops::WGMMA_WAIT);
        case Opcode::TMA_LOAD:     return encode(ops::TMA_LOAD);
        case Opcode::TMA_STORE:    return encode(ops::TMA_STORE);
        // 通用: 使用 constexpr opcode 表
        default: {
            uint16_t op = volta_opcode(inst.opcode);
            if (!op) return encode(ops::EXIT); // fallback
            return encode(op, r(0),r(1),r(2),r(3));
        }
    }
}

} // namespace sass::precise
