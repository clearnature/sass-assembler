/* HunTian SASS — 稳定性防护层 */

#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include "instruction.h"

namespace sass::safe {

// ─── 寄存器边界检查 ───
inline uint8_t check_reg(uint8_t id, const char* ctx="reg") {
    if (id > 255) throw std::out_of_range(
        std::string(ctx) + " out of range: " + std::to_string(id));
    return id;
}

inline uint8_t safe_reg(const Instruction& inst, size_t idx) {
    if (idx >= inst.operands.size()) return 0;
    if (inst.operands[idx].type != OpType::REG) return 0;
    return check_reg(inst.operands[idx].reg_id, "operand");
}

// ─── 寄存器计数验证 ───
inline bool validate_register_count(const std::vector<Instruction>& insts, int max_regs=64) {
    int max_seen = 0;
    for (auto& inst : insts)
        for (auto& op : inst.operands)
            if (op.type == OpType::REG && op.reg_id != 255 && op.reg_id > max_seen)
                max_seen = op.reg_id;
    return max_seen < max_regs;
}

// ─── 指令序列完整性 ───
inline bool has_terminator(const std::vector<Instruction>& insts) {
    for (auto& inst : insts)
        if (inst.opcode == Opcode::EXIT || inst.opcode == Opcode::RET)
            return true;
    return false;
}

// ─── 内存安全检查 ───
inline bool validate_instruction(const Instruction& inst) {
    // 操作数数量合理
    if (inst.operands.size() > 4) return false;
    // 寄存器在合法范围
    for (auto& op : inst.operands) {
        if (op.type == OpType::REG && op.reg_id > 255) return false;
        if (op.type == OpType::VAVX3_REG && op.vavx3_id > 127) return false;
    }
    return true;
}

// ─── 完整程序验证 ───
struct ValidationResult {
    bool ok = true;
    int errors = 0;
    int warnings = 0;
    int reg_count = 0;
    int max_reg_seen = 0;
    bool has_terminator = false;
    std::string message;
};

inline ValidationResult validate(const std::vector<Instruction>& insts) {
    ValidationResult r;
    if (insts.empty()) { r.ok=false; r.errors++; r.message="empty program"; return r; }

    for (size_t i=0;i<insts.size();i++) {
        if (!validate_instruction(insts[i])) {
            r.ok=false; r.errors++;
            r.message="invalid instruction at index "+std::to_string(i);
            return r;
        }
        for (auto& op : insts[i].operands) {
            if (op.type==OpType::REG && op.reg_id!=255 && op.reg_id>r.max_reg_seen)
                r.max_reg_seen=op.reg_id;
        }
    }

    r.reg_count = r.max_reg_seen + 1;
    r.has_terminator = has_terminator(insts);
    if (!r.has_terminator) r.warnings++;
    if (r.max_reg_seen >= 64) r.warnings++;

    return r;
}

} // namespace sass::safe
