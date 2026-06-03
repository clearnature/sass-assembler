/* Volta 表驱动反汇编器 — 替代 53条 if-else */

#pragma once
#include <cstdint>
#include <string>
#include "diagnostic.h"

namespace sass {

// Volta opcode → mnemonic 映射表 (constexpr, O(1)查找)
struct VoltaOpEntry {
    uint16_t opcode;
    const char* mnemonic;
    uint8_t operands;  // 0-4
    bool is_store;     // STG/STS 特殊格式
};

static constexpr VoltaOpEntry volta_disasm_table[] = {
    // Memory: 0x73xx, 0x79xx
    {0x7386, "STG",   2, true},  {0x7986, "STG.E",  2, true},
    {0x7381, "LDG",   2, false}, {0x7981, "LDG.E",  2, false},
    {0x7388, "STS",   2, true},  {0x7988, "STS",    2, true},
    {0x7984, "LDS",   2, false}, {0x7b82, "LDC",    2, false},

    // Integer: 0x76xx
    {0x7624, "IMAD",  3, false}, {0x7625, "IMAD.WIDE", 3, false},
    {0x7630, "HADD2", 2, false}, {0x7631, "HFMA2",  3, false},

    // ALU: 0x78xx
    {0x7802, "MOV",   2, false}, {0x780c, "ISETP",  3, false},
    {0x7812, "IADD",  3, false}, {0x7810, "IADD3",  3, false},
    {0x7819, "SHF",   3, false}, {0x7806, "SHL",    2, false},
    {0x7824, "IMAD.SHL", 3, false},

    // Control: 0x79xx
    {0x7919, "S2R",   2, false}, {0x7918, "NOP",    0, false},
    {0x7947, "BRA",   1, false}, {0x794d, "EXIT",   0, false},
    {0x7950, "RET",   0, false}, {0x7944, "SSY",    0, false},
    {0x7948, "CAL",   1, false}, {0x7946, "BRX",    1, false},

    // MOV variants: 0x7axx, 0x72xx
    {0x7a02, "MOV",   2, false}, {0x7a24, "MOV32I", 2, false},
    {0x7a0c, "SEL",   2, false}, {0x7a0a, "FSET",   2, false},
    {0x7a0b, "FSETP", 2, false}, {0x7a08, "MOV",    2, false},
    {0x7a10, "PRMT",  2, false},

    // Sync: 0x7bxx
    {0x7b1d, "BAR.SYNC",0,false}, {0x7b98, "MEMBAR",0, false},
    {0x7b06, "DEPBAR",0, false},

    // Float: 0x74xx, 0x1xxx, 0x0xxx
    {0x7424, "FADD",  2, false}, {0x1000, "FFMA",   4, false},
    {0x0a10, "FCMP",  2, false}, {0x0f00, "MUFU",   2, false},

    // Conversion: 0x73xx
    {0x7308, "I2F",   2, false}, {0x7305, "F2I",   2, false},

    // Surface/Texture
    {0xe900, "SULD",  2, false}, {0x6900, "TEX",    1, false},
    {0xf000, "TLD",   1, false},

    // Misc
    {0xf389, "SHFL",  2, false}, {0xe0ff, "BPT",    0, false},

    // Ampere additions
    {0x0804, "HADD2", 2, false}, {0x080e, "HMUL2",  2, false},
    {0x0812, "HFMA2", 3, false}, {0x1800, "MMA.SYNC",3,false},
    {0x1904, "MMA.SYNC.ALIGNED",3,false},
    {0x8212, "IADD3", 3, false}, {0x8224, "IMAD",   3, false},
    {0xc0ff, "BAR",   0, false}, {0xd000, "MEMBAR", 0, false},
    {0xd200, "DEPBAR",0, false},

    // Hopper tensor
    {0x7f00, "WGMMA.FENCE",0,false},
    {0x7f01, "WGMMA.COMMIT",0,false},
    {0x7f02, "WGMMA.WAIT",0,false},
    {0x7f10, "TMA.LOAD",0,false},
    {0x7f11, "TMA.STORE",0,false},

    {0, nullptr, 0, false}, // sentinel
};

// ─── O(1) 查找 ───
inline DisasmResult volta_disasm(SASSWord w) {
    DisasmResult r;
    uint16_t op = w & 0xFFFF;

    // 表查找
    const VoltaOpEntry* entry = nullptr;
    for (int i = 0; volta_disasm_table[i].mnemonic; i++) {
        if (volta_disasm_table[i].opcode == op) { entry = &volta_disasm_table[i]; break; }
    }

    if (!entry) {
        char buf[32]; snprintf(buf, sizeof(buf), "UNKNOWN_0x%04x", op);
        r.text = buf;
        r.diags.push_back(Diagnostic::warn(
            std::string("unknown Volta opcode 0x") + std::to_string(op)));
        return r;
    }

    r.text = entry->mnemonic;
    return r;
}

} // namespace sass
