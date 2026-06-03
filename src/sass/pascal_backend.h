/* ============================================================================
 * HunTian SASS — PascalBackend (轻量版, 编码器已拆分)
 *
 * 编码: src/sass/encoder/pascal_encoder.h
 * 反汇编: src/sass/disassembler/pascal_disasm.h
 * ============================================================================ */

#include "device_backend.h"
#include "disassembler/pascal_disasm.h"
#include "encoder/pascal_encoder.h"
#include "optimizer.h"
#include "safe_guard.h"
#include "instruction.h"
#include "opcode_table.h"
#include <cstdint>
#include <cstring>

namespace sass {

class PascalBackend : public IDeviceBackend {
public:
    ArchInfo arch() const override { return {"Pascal GP106","sm_61",61,6}; }
    bool opt_reg_alloc = false;
    mutable std::vector<Diagnostic> diags_;

    const std::vector<Diagnostic>& last_diagnostics() const override { return diags_; }

    std::vector<SASSWord> encode(const ManifoldResult& result) override {
        diags_.clear();
        auto seq = result.optimized_sequence;
        if (seq.empty()) { diags_.push_back(Diagnostic::warn("empty program")); return {}; }
        auto vr = safe::validate(seq);
        if (!vr.ok) { diags_.push_back(Diagnostic::error(vr.message)); return {}; }
        if (!vr.has_terminator) diags_.push_back(Diagnostic::warn("no EXIT/RET terminator"));
        if (vr.max_reg_seen >= 48) diags_.push_back(Diagnostic::warn(
            "high register pressure: R"+std::to_string(vr.max_reg_seen)));

        optimizer::optimize_p1(seq);
        if (opt_reg_alloc) optimizer::reg_allocate(seq);

        std::vector<SASSWord> code; code.reserve(seq.size());
        for (size_t i=0;i<seq.size();i++) {
            if (can_fuse(seq,i)) { code.push_back(encode_vavx3(seq,i,0)); i+=7; }
            else code.push_back(encode_instruction(seq[i]));
        }
        return code;
    }

    std::vector<SASSWord> encode_with_labels(const ManifoldResult& r,
        const std::unordered_map<std::string,uint32_t>&) override { return encode(r); }

    std::string disassemble(SASSWord w) const override { return pascal_disassemble(w); }

    std::vector<std::string> disassemble_block(const std::vector<SASSWord>& c) const override {
        std::vector<std::string> lines; lines.reserve(c.size());
        for (size_t i=0;i<c.size();i++) {
            char buf[80]; snprintf(buf,sizeof(buf),"/*%04zx*/ ",i*8);
            lines.push_back(std::string(buf)+pascal_disassemble(c[i]));
        }
        return lines;
    }

    uint8_t opcode_byte(Opcode op) const override {
        Instruction inst; inst.opcode=op;
        return (const_cast<PascalBackend*>(this)->encode_instruction(inst)>>56)&0xFF;
    }
    const char* opcode_name(Opcode op) const override { return ::sass::opcode_name(op); }
    int operand_count(Opcode op) const override { return ::sass::operand_count(op); }
    bool supports(Opcode) const override { return true; }
    size_t total_instructions() const override { return 108; }

private:
    static uint8_t reg(const Instruction& i, size_t n) { return pascal::reg(i,n); }
    static bool has_reg(const Instruction& i, size_t n) { return pascal::has_reg(i,n); }

    static SASSWord encode_instruction(const Instruction& inst) {
        using namespace pascal;
        switch (inst.opcode) {
            case Opcode::EXIT: return 0xe30000000007000fULL;
            case Opcode::RET:  return 0xe32000000007000fULL;
            case Opcode::BRA:  return enc_bra(inst);
            case Opcode::FFMA: return enc_ffma(inst);
            case Opcode::XMAD: return enc_xmad(inst);
            case Opcode::XMAD_MRG: return enc_xmad_mrg(inst);
            case Opcode::XMAD_PSL: return enc_xmad_psl(inst);
            case Opcode::MOV32I: return enc_mov32i(inst);
            case Opcode::LOP32I: case Opcode::BREV: return enc_lop32i(inst);
            case Opcode::LOP3: return enc_lop3(inst);
            case Opcode::BFI:  return enc_bfi(inst);
            case Opcode::SHR: case Opcode::BFE: return enc_shr(inst);
            case Opcode::MOV:case Opcode::IADD:case Opcode::IADD3:case Opcode::IADD_X:
            case Opcode::ISCADD:case Opcode::ISUB:case Opcode::IMAD:case Opcode::IMUL:
            case Opcode::IMNMX:case Opcode::SHL:case Opcode::SEL:case Opcode::DADD:
            case Opcode::DMUL:case Opcode::POPC:case Opcode::FLO:case Opcode::SHF:
            case Opcode::ISAD:case Opcode::VABSDIFF:case Opcode::PRMT: return enc_alu(inst);
            case Opcode::ISET:case Opcode::ISETP:case Opcode::FSETP:case Opcode::SETP:
            case Opcode::DSETP:case Opcode::CSETP:case Opcode::DSET:case Opcode::PLOP3:
            case Opcode::CSET: return enc_pred(inst);
            case Opcode::FSET: return enc_fset(inst);
            case Opcode::CVT:case Opcode::F2F:case Opcode::F2I:case Opcode::I2F:
            case Opcode::I2I:case Opcode::FRND:case Opcode::FSWZ:case Opcode::RRO:
                return enc_cvt(inst);
            case Opcode::STG:case Opcode::STS: return enc_mem(inst);
            case Opcode::LDG:case Opcode::LDS:case Opcode::LDC:case Opcode::MEMBAR:
            case Opcode::SHFL:case Opcode::LDL:case Opcode::STL: return enc_mem2(inst);
            case Opcode::FADD:case Opcode::FMUL:case Opcode::FSQRT:case Opcode::FRCP:
            case Opcode::FMNMX:case Opcode::FSEL:case Opcode::S2R:case Opcode::BAR:
            case Opcode::DEPBAR:case Opcode::DMNMX:case Opcode::FCMP:case Opcode::FCHK:
            case Opcode::P2R:case Opcode::R2P:case Opcode::B2R:case Opcode::LEPC:
                return enc_float(inst);
            case Opcode::MUFU: return enc_mufu(inst);
            case Opcode::DFMA: return enc_dfma(inst);
            case Opcode::ATOM:case Opcode::RED: return enc_atom(inst);
            case Opcode::BRX:case Opcode::CAL:case Opcode::JMP:case Opcode::JCAL:
            case Opcode::SSY:case Opcode::PBK:case Opcode::PCNT:case Opcode::PRET:
            case Opcode::BPT:case Opcode::LONGJMP:case Opcode::JMX: return enc_bra(inst);
            case Opcode::BRK:case Opcode::CONT: return 0xe34000000007000fULL;
            case Opcode::TEX: return enc_tex(inst);
            case Opcode::TLD:case Opcode::TLD4:case Opcode::TXQ: return enc_tld(inst);
            case Opcode::SULD:case Opcode::SULEA:case Opcode::SUST:case Opcode::SURED:
            case Opcode::SUQ: return enc_surface(inst);
            case Opcode::LD: return enc_mem2(inst);
            case Opcode::ST: return enc_mem(inst);
            case Opcode::I2IP: return enc_cvt(inst);
            case Opcode::SGXT: return enc_alu(inst);
            case Opcode::CCTL:case Opcode::CCTLL: return enc_mem2(inst);
            default: return enc_huntian_fallback(inst);
        }
    }

    static bool can_fuse(const std::vector<Instruction>& seq, size_t idx) {
        if (idx+7>=seq.size()) return false;
        Opcode base=seq[idx].opcode;
        if (base!=Opcode::FFMA&&base!=Opcode::XMAD) return false;
        for (size_t i=idx;i<idx+8;i++) if (seq[i].opcode!=base) return false;
        return true;
    }

    static SASSWord encode_vavx3(const std::vector<Instruction>& seq, size_t start, uint32_t cycle) {
        uint8_t op=(seq[start].opcode==Opcode::FFMA)?0xF1:0xF2;
        SASSWord w=0; w|=(uint64_t)op<<56; w|=8ULL<<48;
        w|=(uint64_t)(cycle&0xFF)<<40;
        uint64_t mask=0;
        for (size_t i=0;i<8&&start+i<seq.size();i++)
            for (auto& opnd:seq[start+i].operands)
                if (opnd.type==OpType::REG&&opnd.reg_id<64) mask|=(1ULL<<opnd.reg_id);
        w|=mask; return w;
    }
};

} // namespace sass
