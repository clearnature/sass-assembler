/* HunTian SASS — Pascal 真实 SASS 反汇编器
 *
 * 输入: 64-bit Pascal SASS hex
 * 输出: 可读文本 "FFMA R0, R1, R2, RZ"
 *
 * 编码布局 (PTX GP106 验证):
 *   Byte7 [63:56] = opcode family
 *   Byte0          = Rd (目标寄存器)
 *   Byte1 [15:8]   = Ra (源寄存器1)
 *   Byte2 [23:16]  = Rb (源寄存器2)
 *   Byte3-6        = 修饰/立即数/常量
 */

#pragma once
#include <cstdint>
#include <string>
#include <cstdio>
#include "diagnostic.h"

namespace sass {

// ─── Pascal opcode family → 助记符 ───
struct PascalOpcodeInfo {
    uint8_t op_byte;
    const char* mnemonic;
    int operand_count;
    bool has_imm;  // 使用立即数而非寄存器
};

// 22 个已验证的 opcode 家族 (来自 pascal_backend.h)
static const PascalOpcodeInfo pascal_op_table[] = {
    {0xe3, "EXIT",   0, false}, {0xe2, "BRA",    1, true},
    {0x59, "FFMA",   4, false}, {0x4e, "XMAD",   4, false},
    {0x4f, "XMAD.MRG",3,false}, {0x5b, "XMAD.PSL",4,false},
    {0x4c, "MOV",    2, true},  {0x01, "MOV32I", 2, true},
    {0x5c, "I2F",    2, false}, {0xee, "STG.E",  2, false},
    {0xef, "LDG.E",  2, false}, {0xf0, "FADD",   3, false},
    {0x50, "MUFU",   2, false}, {0x53, "DFMA",   4, false},
    {0x4b, "ISETP",  3, false}, {0x48, "FSET",   2, false},
    {0x36, "BFI",    3, false}, {0x38, "SHR",    2, false},
    {0x3c, "LOP3",   3, false}, {0x04, "LOP32I", 2, true},
    {0xed, "ATOM",   2, false}, {0xeb, "SULD",   2, false},
    {0xd8, "TEX",    1, false}, {0xda, "TLD",    1, false},
    {0x00, nullptr,  0, false}, // sentinel
};

// ─── 核心反汇编函数 ───
inline DisasmResult pascal_disasm(SASSWord w) {
    DisasmResult r;
    uint8_t op = (w >> 56) & 0xFF;

    // 查找 opcode 家族
    const PascalOpcodeInfo* info = nullptr;
    for (int i = 0; pascal_op_table[i].mnemonic; i++) {
        if (pascal_op_table[i].op_byte == op) { info = &pascal_op_table[i]; break; }
    }

    if (!info) {
        char buf[32]; snprintf(buf, sizeof(buf), "UNKNOWN_0x%02x", op);
        r.text = buf;
        r.diags.push_back(Diagnostic::warn(
            std::string("unknown opcode byte 0x") + std::to_string(op)));
        return r;
    }

    // 解码操作数
    uint8_t rd = w & 0xFF;
    uint8_t ra = (w >> 8) & 0xFF;
    uint8_t rb = (w >> 16) & 0xFF;
    uint8_t rc = (w >> 24) & 0xFF;

    char buf[128];
    switch (info->operand_count) {
        case 0:
            snprintf(buf, sizeof(buf), "%s", info->mnemonic); break;
        case 1:
            snprintf(buf, sizeof(buf), "%s 0x%x", info->mnemonic,
                (info->has_imm ? (int)(w & 0xFFFFFFFF) : rd)); break;
        case 2:
            if (info->has_imm)
                snprintf(buf, sizeof(buf), "%s R%d, 0x%lx", info->mnemonic, rd, (w>>16)&0xFFFFFFFF);
            else
                snprintf(buf, sizeof(buf), "%s R%d, R%d", info->mnemonic, rd, ra);
            break;
        case 3:
            snprintf(buf, sizeof(buf), "%s R%d, R%d, R%d", info->mnemonic, rd, ra, rb);
            break;
        case 4:
            snprintf(buf, sizeof(buf), "%s R%d, R%d, R%d, %s",
                info->mnemonic, rd, ra, rb, (rc==0xFF)?"RZ":("R"+std::to_string(rc)).c_str());
            break;
        default: snprintf(buf, sizeof(buf), "%s", info->mnemonic);
    }
    r.text = buf;
    return r;
}

// ─── 批量反汇编 ───
inline std::vector<DisasmResult> pascal_disasm_block(const std::vector<SASSWord>& code) {
    std::vector<DisasmResult> lines; lines.reserve(code.size());
    for (auto w : code) lines.push_back(pascal_disasm(w));
    return lines;
}

// ─── 集成到 PascalBackend::disassemble ───
inline std::string pascal_disassemble(SASSWord w) {
    return pascal_disasm(w).text;
}

} // namespace sass
