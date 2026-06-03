/* ============================================================================
 * HunTian SASS 汇编器 — Opcode 名称与操作数表
 *
 * 提供：
 *   opcode_name()     — Opcode 枚举 → 字符串名称
 *   operand_count()   — 每条指令显示的操作数个数 (0/1/2/3)
 *   operand_str()     — 编码后的 16 位操作数 → 反汇编文本
 * ============================================================================ */

#ifndef SASS_OPCODE_TABLE_H
#define SASS_OPCODE_TABLE_H

#include "instruction.h"
#include "operand_codec.h"
#include <string>
#include <cstdint>

namespace sass {

// ─── Opcode → 名称 ───
inline const char* opcode_name(Opcode op) {
    switch (op) {
        // 整数
        case Opcode::IADD:   return "iadd";
        case Opcode::IADD3:  return "iadd3";
        case Opcode::IADD_X:  return "iadd.x";
        case Opcode::ISCADD: return "iscadd";
        case Opcode::ISUB:   return "isub";
        case Opcode::IMAD:   return "imad";
        case Opcode::IMUL:   return "imul";
        case Opcode::IMNMX:  return "imnmx";
        case Opcode::SHL:    return "shl";
        case Opcode::SHR:    return "shr";
        case Opcode::BFE:    return "bfe";
        case Opcode::BFI:    return "bfi";
        case Opcode::LOP3:   return "lop3";
        case Opcode::LOP32I: return "lop32i";
        case Opcode::BREV:   return "brev";

        // 浮点
        case Opcode::FADD:   return "fadd";
        case Opcode::FMUL:   return "fmul";
        case Opcode::FSET:   return "fset";
        case Opcode::FMNMX:  return "fmnmx";
        case Opcode::FSEL:   return "fsel";
        case Opcode::FRCP:   return "frcp";
        case Opcode::FSQRT:  return "fsqrt";

        // XMAD + FFMA
        case Opcode::FFMA:     return "ffma";
        case Opcode::XMAD:     return "xmad";
        case Opcode::XMAD_MRG: return "xmad.mrg";
        case Opcode::XMAD_PSL: return "xmad.psl";

        // 内存
        case Opcode::LDG:  return "ldg";
        case Opcode::LDS:  return "lds";
        case Opcode::STG:  return "stg";
        case Opcode::STS:  return "sts";
        case Opcode::LDC:  return "ldc";

        // 控制流
        case Opcode::BRA:   return "bra";
        case Opcode::BRX:   return "brx";
        case Opcode::CAL:   return "cal";
        case Opcode::RET:   return "ret";
        case Opcode::EXIT:  return "exit";
        case Opcode::KILL:  return "kill";
        case Opcode::YIELD: return "yield";

        // 同步
        case Opcode::BAR:    return "bar";
        case Opcode::DEPBAR: return "depbar";
        case Opcode::MEMBAR: return "membar";

        // 数据移动
        case Opcode::MOV:    return "mov";
        case Opcode::MOV32I: return "mov32i";
        case Opcode::SEL:    return "sel";
        case Opcode::S2R:    return "s2r";

        // 转换
        case Opcode::CVT:    return "cvt";

        // 比较
        case Opcode::ISET:   return "iset";
        case Opcode::ISETP:  return "isetp";
        case Opcode::FSETP:  return "fsetp";
        case Opcode::SETP:   return "setp";

        // HunTian VAVX3
        case Opcode::VAVX3_ADD_512:   return "vavx3.add";
        case Opcode::VAVX3_MUL_512:   return "vavx3.mul";
        case Opcode::VAVX3_MAD_512:   return "vavx3.mad";
        case Opcode::VAVX3_MMA_512:   return "vavx3.mma";
        case Opcode::VAVX3_GEOM:      return "vavx3.geom";
        case Opcode::VAVX3_SHUFFLE:   return "vavx3.shuffle";
        case Opcode::VAVX3_BRAID:     return "vavx3.braid";
        case Opcode::VAVX3_LAPLACIAN: return "vavx3.laplacian";

        // HunTian 三进制
        case Opcode::TMAD:    return "tmad";
        case Opcode::TMUL:    return "tmul";
        case Opcode::TCONV:   return "tconv";
        case Opcode::TRYTE_OP: return "tryte";

        // ─── 双精度 ───
        case Opcode::DADD:   return "dadd";
        case Opcode::DMUL:   return "dmul";
        case Opcode::DFMA:   return "dfma";
        case Opcode::DMNMX:  return "dmnmx";
        case Opcode::DSET:   return "dset";
        case Opcode::DSETP:  return "dsetp";

        // ─── 浮点杂项 ───
        case Opcode::FCMP:   return "fcmp";
        case Opcode::FSWZ:   return "fswz";
        case Opcode::FCHK:   return "fchk";
        case Opcode::RRO:    return "rro";
        case Opcode::MUFU:   return "mufu";

        // ─── 整数杂项 ───
        case Opcode::ISAD:   return "isad";
        case Opcode::FLO:    return "flo";
        case Opcode::ICMP:   return "icmp";
        case Opcode::SHF:    return "shf";
        case Opcode::POPC:   return "popc";
        case Opcode::VABSDIFF: return "vabsdiff";

        // ─── 转换 ───
        case Opcode::F2F:    return "f2f";
        case Opcode::F2I:    return "f2i";
        case Opcode::I2F:    return "i2f";
        case Opcode::I2I:    return "i2i";
        case Opcode::FRND:   return "frnd";

        // ─── 移动 ───
        case Opcode::PRMT:   return "prmt";
        case Opcode::SHFL:   return "shfl";

        // ─── 谓词 ───
        case Opcode::PLOP3:  return "plop3";
        case Opcode::P2R:    return "p2r";
        case Opcode::R2P:    return "r2p";
        case Opcode::CSET:   return "cset";
        case Opcode::CSETP:  return "csetp";

        // ─── 加载/存储扩展 ───
        case Opcode::LDL:    return "ldl";
        case Opcode::STL:    return "stl";

        // ─── XMAD 扩展 ───
        case Opcode::XMAD_CHI: return "xmad.chi";

        // ─── 原子 ───
        case Opcode::ATOM:   return "atom";
        case Opcode::RED:    return "red";

        // ─── 控制流扩展 ───
        case Opcode::JMP:    return "jmp";
        case Opcode::JCAL:   return "jcal";
        case Opcode::BRK:    return "brk";
        case Opcode::CONT:   return "cont";
        case Opcode::SSY:    return "ssy";
        case Opcode::PBK:    return "pbk";
        case Opcode::PCNT:   return "pcnt";
        case Opcode::PRET:   return "pret";
        case Opcode::BPT:    return "bpt";
        case Opcode::LONGJMP: return "longjmp";

        // ─── 杂项 ───
        case Opcode::B2R:    return "b2r";
        case Opcode::LEPC:   return "lepc";

        // ─── 纹理/表面 ───
        case Opcode::TEX:    return "tex";
        case Opcode::TLD:    return "tld";
        case Opcode::TLD4:   return "tld4";
        case Opcode::TXQ:    return "txq";
        case Opcode::SULD:   return "suld";
        case Opcode::SULEA:  return "sulea";
        case Opcode::SUST:   return "sust";
        case Opcode::SURED:  return "sured";
        case Opcode::SUQ:    return "suq";

        // ─── Hopper+ Tensor ───
        case Opcode::WGMMA_ARRIVE:    return "wgmma.fence";
        case Opcode::WGMMA_COMMIT:    return "wgmma.commit";
        case Opcode::WGMMA_WAIT:      return "wgmma.wait";
        case Opcode::TMA_LOAD:        return "tma.load";
        case Opcode::TMA_STORE:       return "tma.store";
        case Opcode::MBARRIER_ARRIVE: return "mbarrier.arrive";
        case Opcode::TENSORMAP_REPLACE: return "tensormap.replace";
        case Opcode::FENCE_TENSORMAP: return "fence.tensormap";

        // ─── Fermi遗留 ───
        case Opcode::LD:     return "ld";
        case Opcode::ST:     return "st";
        case Opcode::JMX:    return "jmx";
        case Opcode::I2IP:   return "i2ip";
        case Opcode::SGXT:   return "sgxt";
        case Opcode::CCTL:   return "cctl";
        case Opcode::CCTLL:  return "cctll";

        default: return "unknown";
    }
}

// ─── 操作数个数 ───
inline int operand_count(Opcode op) {
    switch (op) {
        case Opcode::EXIT: case Opcode::RET: case Opcode::KILL:
        case Opcode::YIELD: case Opcode::BAR: case Opcode::DEPBAR:
        case Opcode::MEMBAR:
            return 0;
        case Opcode::BRA: case Opcode::BRX: case Opcode::CAL:
            return 1;
        case Opcode::MOV: case Opcode::MOV32I: case Opcode::S2R:
        case Opcode::FRCP: case Opcode::FSQRT: case Opcode::BREV:
        case Opcode::ISUB: case Opcode::IMUL: case Opcode::FADD:
        case Opcode::FMUL: case Opcode::LOP32I: case Opcode::CVT:
        case Opcode::SHL: case Opcode::SHR:
        case Opcode::LDG: case Opcode::LDS: case Opcode::STG:
        case Opcode::STS: case Opcode::LDC:
        case Opcode::F2F: case Opcode::F2I: case Opcode::I2F:
        case Opcode::I2I: case Opcode::FRND: case Opcode::CSET:
        case Opcode::STL: case Opcode::LDL:
        case Opcode::FLO: case Opcode::POPC: case Opcode::PRMT:
        case Opcode::SHFL: case Opcode::P2R:  case Opcode::R2P:
        case Opcode::B2R: case Opcode::LEPC:
        case Opcode::FSWZ: case Opcode::RRO: case Opcode::MUFU:
        case Opcode::ISAD: case Opcode::VABSDIFF:
        case Opcode::DMUL: case Opcode::FCHK: case Opcode::FCMP:
            return 2;
        // 控制流扩展: 1 operands
        case Opcode::JMP: case Opcode::JCAL: case Opcode::SSY:
        case Opcode::PBK: case Opcode::PCNT: case Opcode::PRET:
        case Opcode::BPT: case Opcode::LONGJMP:
            return 1;
        case Opcode::CONT:
            return 0;
        default:
            return 3;
    }
}

// ─── 解码单个操作数为字符串 ───
inline std::string operand_str(uint16_t enc, int /*index*/, Opcode /*op*/) {
    if (decode_op_is_reg(enc)) {
        return "R" + std::to_string(decode_op_reg(enc));
    }
    if (decode_op_is_imm(enc)) {
        return std::to_string(decode_op_imm(enc));
    }
    if (decode_op_is_mem(enc)) {
        uint8_t base = decode_op_base(enc);
        int8_t off = decode_op_offset(enc);
        if (off >= 0)
            return "[R" + std::to_string(base) + " + " + std::to_string(off) + "]";
        else
            return "[R" + std::to_string(base) + " - " + std::to_string(-off) + "]";
    }
    if (decode_op_is_mems(enc)) {
        int16_t off = decode_op_mems_offset(enc);
        return "[R0 + " + std::to_string(off) + "]";
    }
    return "?";
}

} // namespace sass

#endif // SASS_OPCODE_TABLE_H
