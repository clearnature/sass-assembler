/* ============================================================================
 * HunTian SASS 汇编器 — 反汇编器
 *
 * 将 SASSWord 二进制编码解码为 .sass 可读文本。
 * 支持：寄存器/立即数/内存/标号/VAVX3/三进制 全部 55 条指令。
 * ============================================================================ */

#include "disassembler.h"
#include "opcode_table.h"
#include "operand_codec.h"
#include <vector>
#include <string>
#include <cstdio>

namespace sass {

std::string disassemble(SASSWord w) {
    uint8_t op = (w >> 56) & 0xFF;
    uint8_t cyc = (w >> 48) & 0xFF;
    uint8_t flags = (w >> 40) & 0xFF;

    // VAVX3 指令 (0xF0-0xF7)
    if (op >= 0xF0 && op <= 0xF7) {
        uint8_t count = (w >> 48) & 0xFF;
        if (count == 8) {
            return std::string("vavx3.fused_512 x") + std::to_string(cyc);
        }
        static const char* vnames[] = {
            "vavx3.add", "vavx3.mul", "vavx3.mad", "vavx3.mma",
            "vavx3.geom", "vavx3.shuffle", "vavx3.braid", "vavx3.laplacian"
        };
        Opcode vop = static_cast<Opcode>(op);
        std::string r = vnames[op - 0xF0];
        int n = operand_count(vop);
        for (int i = 0; i < n && i < 3; ++i) {
            uint16_t opd = (w >> (16 * i)) & 0xFFFF;
            std::string sep = (i == 0) ? " " : ", ";
            if (decode_op_is_reg(opd)) {
                r += sep + operand_str(opd, i, vop);
                if (flags & (1 << i)) r += ".reuse";
            }
        }
        return r;
    }

    // 三进制指令 (0xF8-0xFB)
    if (op >= 0xF8) {
        static const char* tnames[] = {"tmad", "tmul", "tconv", "tryte"};
        return std::string(tnames[op - 0xF8]);
    }

    auto opcode = static_cast<Opcode>(op);
    std::string r(opcode_name(opcode));

    uint16_t op1 = w & 0xFFFF;
    uint16_t op2 = (w >> 16) & 0xFFFF;
    uint16_t op3 = (w >> 32) & 0xFFFF;

    int n = operand_count(opcode);
    uint16_t ops[3] = {op1, op2, op3};

    // 存储类 stg/sts → [mem], src
    if (opcode == Opcode::STG || opcode == Opcode::STS) {
        r += " " + operand_str(op1, 0, opcode);
        if (n > 1 && decode_op_is_reg(op2))
            r += ", " + operand_str(op2, 1, opcode);
    }
    // 加载类 ldg/lds/ldc → dst, [mem] 或 dst, src
    else if (opcode == Opcode::LDG || opcode == Opcode::LDS || opcode == Opcode::LDC) {
        if (decode_op_is_reg(op1)) r += " " + operand_str(op1, 0, opcode);
        if (n > 1) {
            if (decode_op_is_mem(op2) || decode_op_is_mems(op2))
                r += ", " + operand_str(op2, 1, opcode);
            else if (decode_op_is_reg(op2))
                r += ", " + operand_str(op2, 1, opcode);
            else if (decode_op_is_imm(op2))
                r += ", " + operand_str(op2, 1, opcode);
        }
    }
    // 分支类: bra/brx/cal offset
    else if (opcode == Opcode::BRA || opcode == Opcode::BRX || opcode == Opcode::CAL) {
        if (n > 0) {
            if (decode_op_is_imm(op1))
                r += " " + std::to_string(decode_op_imm(op1));
            else if (decode_op_is_reg(op1))
                r += " " + operand_str(op1, 0, opcode);
        }
    }
    // 通用格式
    else {
        for (int i = 0; i < n && i < 3; ++i) {
            std::string sep = (i == 0) ? " " : ", ";
            if (decode_op_is_reg(ops[i])) {
                r += sep + operand_str(ops[i], i, opcode);
                if (flags & (1 << i)) r += ".reuse";
            } else if (decode_op_is_imm(ops[i])) {
                r += sep + operand_str(ops[i], i, opcode);
            } else if (decode_op_is_mem(ops[i])) {
                r += sep + operand_str(ops[i], i, opcode);
            }
        }
    }

    return r;
}

std::vector<std::string> disassemble_block(const std::vector<SASSWord>& code) {
    std::vector<std::string> lines;
    lines.reserve(code.size());
    char addr[16];
    for (size_t i = 0; i < code.size(); ++i) {
        std::snprintf(addr, sizeof(addr), "/*%04zx*/", i * 8);
        lines.push_back(std::string(addr) + " " + disassemble(code[i]));
    }
    return lines;
}

} // namespace sass
