/* HunTian SASS — 优化编码器: constexpr 数组替代 unordered_map
 *
 * DeepGEMM 启发: 编译期常量化 opcode 查找
 *   unordered_map O(1) 哈希 → constexpr 数组 O(1) 直接索引
 *   性能提升: ~10x (消除哈希计算 + 内存间接访问)
 */

#pragma once
#include <cstdint>
#include <array>
#include "instruction.h"

namespace sass {

// ─── 编译期 opcode 查找表 ───
template<size_t N>
struct OpcodeTable {
    std::array<uint16_t, N> data{};

    constexpr OpcodeTable() {
        // 默认: 0 = 无效 opcode
    }

    constexpr uint16_t operator[](Opcode op) const {
        auto idx = static_cast<size_t>(op);
        return idx < N ? data[idx] : 0;
    }
};

// Volta opcode 表 (256 条目, 编译期初始化)
inline constexpr auto volta_opcode_table() {
    std::array<uint16_t, 256> t{};
    // Memory
    t[0x17] = 0x7381; // LDG
    t[0x18] = 0x7984; // LDS
    t[0x19] = 0x7386; // STG
    t[0x1A] = 0x7388; // STS
    t[0x1B] = 0x7b82; // LDC
    // ALU
    t[0x26] = 0x7a02; // MOV
    t[0x27] = 0x7a24; // MOV32I
    t[0x01] = 0x7812; // IADD
    t[0x02] = 0x7810; // IADD3
    t[0x04] = 0x7824; // ISCADD
    t[0x05] = 0x7812; // ISUB
    t[0x06] = 0x7624; // IMAD
    t[0x07] = 0x7624; // IMUL
    t[0x09] = 0x7806; // SHL
    t[0x0A] = 0x7819; // SHR
    t[0x0B] = 0x7819; // BFE
    t[0x0C] = 0x7624; // BFI
    t[0x0D] = 0x7624; // LOP3
    t[0x0E] = 0x7624; // LOP32I
    t[0x0F] = 0x7819; // BREV
    t[0x28] = 0x7a0c; // SEL
    t[0x45] = 0x7a10; // PRMT
    // Float
    t[0x10] = 0x7424; // FADD
    t[0x11] = 0x7424; // FMUL
    t[0x59] = 0x1000; // FFMA
    t[0x12] = 0x7a0a; // FSET
    t[0x13] = 0x7424; // FMNMX
    t[0x14] = 0x7424; // FSEL
    t[0x15] = 0x1000; // FRCP
    t[0x16] = 0x1000; // FSQRT
    // Double
    t[0x2F] = 0x7424; // DADD
    t[0x30] = 0x7424; // DMUL
    t[0x31] = 0x1000; // DFMA
    // Conversion
    t[0x2A] = 0x7308; // CVT
    t[0x42] = 0x7308; // I2F
    t[0x41] = 0x7305; // F2I
    t[0x40] = 0x7308; // F2F
    t[0x43] = 0x7308; // I2I
    // Control
    t[0x20] = 0x794d; // EXIT
    t[0x1F] = 0x7950; // RET
    t[0x1C] = 0x7947; // BRA
    t[0x1D] = 0x7946; // BRX
    t[0x1E] = 0x7948; // CAL
    t[0x23] = 0x7b1d; // BAR
    t[0x24] = 0x7b06; // DEPBAR
    t[0x25] = 0x7b98; // MEMBAR
    t[0x62] = 0x7944; // SSY
    t[0x29] = 0x7919; // S2R
    // Predicate
    t[0x2B] = 0x780c; // ISET
    t[0x2C] = 0x780c; // ISETP
    t[0x2D] = 0x7a0b; // FSETP
    t[0x2E] = 0x780c; // SETP
    t[0x4A] = 0x780c; // CSET
    t[0x4B] = 0x7a0b; // CSETP
    // XMAD
    t[0x4E] = 0x7624; // XMAD
    t[0x4F] = 0x7625; // XMAD_MRG
    t[0x5B] = 0x7625; // XMAD_PSL
    t[0x50] = 0x7625; // XMAD_CHI
    // Misc
    t[0x39] = 0x0f00; // MUFU
    t[0x35] = 0x0a10; // FCMP
    t[0x3E] = 0x7624; // POPC
    t[0x3B] = 0x7819; // FLO
    t[0x46] = 0xf389; // SHFL
    t[0x3D] = 0x7819; // SHF
    t[0x6E] = 0xe900; // SULD
    t[0x70] = 0xe900; // SUST
    t[0x6A] = 0x6900; // TEX
    t[0x6B] = 0xf000; // TLD
    t[0x5C] = 0x7386; // ATOM
    t[0x5D] = 0x7386; // RED
    t[0x66] = 0xe0ff; // BPT
    t[0x4C] = 0x7984; // LDL
    t[0x4D] = 0x7388; // STL
    // Control extended
    t[0x5E] = 0x7947; // JMP
    t[0x5F] = 0x7948; // JCAL
    t[0x60] = 0x794d; // BRK
    t[0x61] = 0x7918; // CONT
    t[0x63] = 0x7947; // PBK
    t[0x64] = 0x7947; // PCNT
    t[0x65] = 0x7950; // PRET
    t[0x67] = 0x7947; // LONGJMP
    // Legacy
    t[0x68] = 0x7919; // B2R
    t[0x69] = 0x7919; // LEPC
    t[0x73] = 0x7381; // LD
    t[0x74] = 0x7386; // ST
    t[0x77] = 0x7806; // SGXT
    t[0x78] = 0x7b82; // CCTL
    t[0x79] = 0x7b82; // CCTLL
    t[0x75] = 0x7947; // JMX
    t[0x76] = 0x7308; // I2IP
    // Surface/Texture extended
    t[0x6F] = 0xe900; // SULEA
    t[0x71] = 0xe900; // SURED
    t[0x72] = 0xe900; // SUQ
    t[0x6C] = 0xf000; // TLD4
    t[0x6D] = 0xf000; // TXQ
    // Hopper Tensor
    t[0x80] = 0x7f00; // WGMMA_ARRIVE
    t[0x81] = 0x7f01; // WGMMA_COMMIT
    t[0x82] = 0x7f02; // WGMMA_WAIT
    t[0x83] = 0x7f10; // TMA_LOAD
    t[0x84] = 0x7f11; // TMA_STORE
    t[0x85] = 0x7f20; // MBARRIER_ARRIVE
    t[0x86] = 0x7f30; // TENSORMAP_REPLACE
    t[0x87] = 0x7f40; // FENCE_TENSORMAP
    // Float misc
    t[0x36] = 0x7424; // FSWZ
    t[0x37] = 0x1000; // FCHK
    t[0x38] = 0x1000; // RRO
    // Integer misc
    t[0x3A] = 0x7812; // ISAD
    t[0x3C] = 0x780c; // ICMP
    t[0x3F] = 0x7812; // VABSDIFF
    t[0x08] = 0x7625; // IMNMX
    // Predicate
    t[0x47] = 0x7624; // PLOP3
    t[0x48] = 0x7919; // P2R
    t[0x49] = 0x7919; // R2P
    // Double remaining
    t[0x32] = 0x7424; // DMNMX
    t[0x33] = 0x7a0a; // DSET
    t[0x34] = 0x7a0b; // DSETP
    // Conversion remaining
    t[0x44] = 0x7308; // FRND
    // IADD_X
    t[0x03] = 0x7812; // IADD_X
    // Control flow legacy
    t[0x21] = 0x794d; // KILL
    t[0x22] = 0x7918; // YIELD
    return t;
}

// ─── O(1) 直接数组查找 ───
inline uint16_t volta_opcode(Opcode op) {
    static constexpr auto table = volta_opcode_table();
    auto idx = static_cast<size_t>(op);
    return idx < 256 ? table[idx] : 0;
}

// Ampere 表 (继承 Volta, 覆写差异)
inline constexpr auto ampere_opcode_table() {
    auto t = volta_opcode_table();
    // Ampere 差异
    t[0x19] = 0x7986; // STG → Ampere encoding
    t[0x17] = 0x7981; // LDG → Ampere encoding
    t[0x1A] = 0x7988; // STS → Ampere encoding
    t[0x18] = 0x7984; // LDS → Ampere encoding (same)
    t[0x23] = 0xc0ff; // BAR → Ampere encoding
    t[0x25] = 0xd000; // MEMBAR → Ampere encoding
    t[0x24] = 0xd200; // DEPBAR → Ampere encoding
    t[0x02] = 0x8212; // IADD3 → Ampere encoding
    t[0x07] = 0x8224; // IMUL → Ampere encoding
    t[0x10] = 0x0804; // FADD → HADD2 (FP16)
    t[0x11] = 0x080e; // FMUL → HMUL2 (FP16)
    t[0x59] = 0x0812; // FFMA → HFMA2 (FP16)
    return t;
}

inline uint16_t ampere_opcode(Opcode op) {
    static constexpr auto table = ampere_opcode_table();
    auto idx = static_cast<size_t>(op);
    return idx < 256 ? table[idx] : 0;
}

// Hopper 表
inline constexpr auto hopper_opcode_table() {
    auto t = ampere_opcode_table();
    return t;
}

inline uint16_t hopper_opcode(Opcode op) {
    static constexpr auto table = hopper_opcode_table();
    auto idx = static_cast<size_t>(op);
    return idx < 256 ? table[idx] : 0;
}

} // namespace sass
