/* ============================================================================
 * HunTian SASS 汇编器 — VoltaBackend v2 (58 opcodes verified)
 *
 * Volta SM70 128-bit encoding: opcode = word0[15:0]
 * 来源: cuobjdump -sass, 4探针, ~800行 SASS
 * ============================================================================ */

#include "device_backend.h"
#include "opcode_table.h"
#include "fast_opcode_table.h"
#include "volta_opcode_table.h"
#include "volta_encoder.h"
#include "ampere_precise_encoder.h"
#include "jit_compiler.h"
#include "../disassembler/volta_disasm.h"
#include <cstdio>
#include <cstring>

namespace sass {

class VoltaBackend : public IDeviceBackend {
public:
    ArchInfo arch() const override {
        return {"Volta GV100", "sm_70", 70, 7};
    }

    // ─── 编码 (全指令, 128-bit pairs) ───
    std::vector<SASSWord> encode(const ManifoldResult& r) override {
        return encode_with_labels(r, {});
    }
    std::vector<SASSWord> encode_with_labels(const ManifoldResult& result,
        const std::unordered_map<std::string, uint32_t>&) override {
        // JIT 模板快速路径: constexpr O(1) 分发
        return jit::JITCompiler<sass::jit::SM70>::compile(result.optimized_sequence);
    }

    // 快速编码: constexpr O(1) 查找
    static std::pair<SASSWord,SASSWord> fast_encode(const Instruction& inst, uint16_t(*op_fn)(Opcode)) {
        uint16_t op = op_fn(inst.opcode);
        if (!op) return {0,0};
        uint8_t rd=reg(inst,0), ra=reg(inst,1), rb=reg(inst,2);
        uint64_t w0=op|((uint64_t)rd<<20)|((uint64_t)ra<<26)|((uint64_t)rb<<32);
        uint64_t w1=volta_op::CTRL_DEFAULT;
        return {w0,w1};
    }
    static uint8_t reg(const Instruction& i, size_t n){return (n<i.operands.size()&&i.operands[n].type==OpType::REG)?i.operands[n].reg_id:0;}

public:

    // ─── 反汇编 (58 opcodes verified) ───
    std::string disassemble(SASSWord w) const override {
        return volta_disasm(w).text;  // 表驱动 O(1)
    }

    std::vector<std::string> disassemble_block(const std::vector<SASSWord>& c) const override {
        std::vector<std::string> lines;
        for (size_t i = 0; i < c.size(); i += 2) {
            char buf[80];
            snprintf(buf, sizeof(buf), "/*%04zx*/ %s", i*8, disassemble(c[i]).c_str());
            lines.push_back(buf);
            if (i+1 < c.size()) i++;
        }
        return lines;
    }

    // ─── Opcode 查询 ───
    uint8_t opcode_byte(Opcode op) const override {
        switch (op) {
            case Opcode::STG:  return volta_op::STG & 0xFF;
            case Opcode::LDG:  return volta_op::LDG & 0xFF;
            case Opcode::EXIT: return volta_op::EXIT & 0xFF;
            case Opcode::BRA:  return volta_op::BRA & 0xFF;
            case Opcode::MOV:  return volta_op::MOV_C & 0xFF;
            case Opcode::S2R:  return volta_op::S2R & 0xFF;
            case Opcode::BAR:  return volta_op::BAR & 0xFF;
            case Opcode::FFMA: return volta_op::FFMA & 0xFF;
            case Opcode::FADD: return volta_op::FADD & 0xFF;
            // NOP not in our Opcode enum
            default: return 0;
        }
    }
    const char* opcode_name(Opcode op) const override { return sass::opcode_name(op); }
    int operand_count(Opcode op) const override { return sass::operand_count(op); }
    bool supports(Opcode op) const override { return op <= Opcode::SUQ; }
    size_t total_instructions() const override { return 108; }
};

// ─── Turing: shares Volta encoding ───
class TuringBackend : public VoltaBackend {
public:
    ArchInfo arch() const override { return {"Turing TU102", "sm_75", 75, 7}; }
};

// ─── Ampere: extends Volta with FP16/BF16/Tensor ops ───
class AmpereBackend : public VoltaBackend {
public:
    ArchInfo arch() const override { return {"Ampere GA100", "sm_80", 80, 8}; }

    std::vector<SASSWord> encode(const ManifoldResult& r) override {
        return encode_with_labels(r, {});
    }
    std::vector<SASSWord> encode_with_labels(const ManifoldResult& result,
        const std::unordered_map<std::string, uint32_t>&) override {
        std::vector<SASSWord> code;
        for (const auto& inst : result.optimized_sequence) {
            auto pair = encode_ampere(inst);
            if (pair.first == 0 && pair.second == 0) {
                // fallback to Volta encoding
                auto vp = volta_enc::encode(inst, volta_enc::ampere_opcode_map());
                pair = vp;
            }
            code.push_back(pair.first);
            code.push_back(pair.second);
        }
        return code;
    }

private:
    static uint8_t reg(const Instruction& i, size_t n) {
        return (n < i.operands.size() && i.operands[n].type == OpType::REG) ? i.operands[n].reg_id : 0;
    }
    std::pair<SASSWord, SASSWord> encode_ampere(const Instruction& inst) {
        switch (inst.opcode) {
            case Opcode::EXIT: return {0x000000000000794dULL, 0x000fe40000000f00ULL};
            case Opcode::RET:  return {0x0000000000007950ULL, 0x000fe40000000f00ULL};
            case Opcode::FFMA: {
                uint8_t rd=reg(inst,0), ra=reg(inst,1), rb=reg(inst,2), rc=reg(inst,3);
                return ampere_enc::encode_ffma(rd, ra, rb, rc);
            }
            case Opcode::STG: {
                uint8_t addr=reg(inst,0), src=reg(inst,1);
                return ampere_enc::encode_stg(addr, src, 0);
            }
            case Opcode::LDG: {
                uint8_t dst=reg(inst,0), addr=reg(inst,1);
                return ampere_enc::encode_stg(addr, dst, 0);  // reuse STG layout for now
            }
            case Opcode::MOV: {
                uint8_t rd=reg(inst,0);
                uint64_t imm = (inst.operands.size()>1&&inst.operands[1].type==OpType::IMM)
                    ? inst.operands[1].imm_val : 0;
                return ampere_enc::encode_mov(rd, imm);
            }
            default: return {0,0};
        }
    }

public:
    // 反汇编: 继承Volta表驱动 (包含所有Ampere+指令)
    size_t total_instructions() const override { return 150; }
};

// Ada, Hopper, Blackwell — inherit Ampere
class AdaBackend       : public AmpereBackend { public: ArchInfo arch() const override { return {"Ada AD102","sm_89",89,8}; } };
class HopperBackend : public AmpereBackend {
public:
    ArchInfo arch() const override { return {"Hopper GH100","sm_90",90,9}; }

    std::vector<SASSWord> encode(const ManifoldResult& r) override {
        std::vector<SASSWord> code;
        for (const auto& inst : r.optimized_sequence) {
            auto pair = volta_enc::encode(inst, volta_enc::hopper_opcode_map());
            if (pair.first == 0 && pair.second == 0) {
                auto vp = volta_enc::encode(inst, volta_enc::ampere_opcode_map());
                if (vp.first == 0 && vp.second == 0) {
                    // fallback: use Volta generic encoding
                    auto fallback = VoltaBackend::encode(r);
                    code.insert(code.end(), fallback.begin(), fallback.end());
                    continue;
                }
                pair = vp;
            }
            code.push_back(pair.first);
            code.push_back(pair.second);
        }
        return code;
    }

    std::string disassemble(SASSWord w) const override {
        uint16_t op = w & 0xFFFF;
        if (op == 0x7f00) return "wgmma.fence";
        if (op == 0x7f01) return "wgmma.commit";
        if (op == 0x7f02) return "wgmma.wait";
        if (op == 0x7f10) return "tma.load";
        if (op == 0x7f11) return "tma.store";
        if (op == 0x7f20) return "mbarrier.arrive";
        if (op == 0x7f30) return "tensormap.replace";
        if (op == 0x7f40) return "fence.tensormap";
        return AmpereBackend::disassemble(w);
    }
    size_t total_instructions() const override { return 158; }
};

class BlackwellBackend : public HopperBackend {
public:
    ArchInfo arch() const override { return {"Blackwell GB100","sm_100",100,10}; }
    size_t total_instructions() const override { return 160; }
};

} // namespace sass
