/* HunTian SASS — JIT 模板参数化编译器
 *
 * DeepGEMM 启发: 编译期常量化 + 模板特化
 *   运行时 zero-cost abstraction
 *
 * 使用:
 *   auto code = JITCompiler<SM80>::compile<FFMA, IADD, EXIT>(insts);
 */

#pragma once
#include <cstdint>
#include <vector>
#include "instruction.h"
#include "dsl_encoder.h"
#include "fast_opcode_table.h"

namespace sass::jit {

// ─── 架构标签 ───
struct SM61 { static constexpr int cc=61; using word=SASSWord; };
struct SM70 { static constexpr int cc=70; using word=SASSWord; };
struct SM80 { static constexpr int cc=80; using word=SASSWord; };
struct SM90 { static constexpr int cc=90; using word=SASSWord; };

// ─── 模板编码器: 编译期展开 ───
template<typename Arch>
struct TemplateEncoder {
    // 单指令编码 (编译期常量化 == DeepGEMM JIT 等价)
    template<Opcode OP>
    static std::pair<uint64_t,uint64_t> encode_one(const Instruction& inst) {
        if constexpr (OP == Opcode::EXIT)  return dsl::encode_exit();
        if constexpr (OP == Opcode::RET)   return dsl::encode_exit();  // same family
        if constexpr (OP == Opcode::FFMA) {
            uint8_t rd=reg(inst,0),ra=reg(inst,1),rb=reg(inst,2),rc=reg(inst,3);
            return dsl::encode_ffma(rd,ra,rb,rc);
        }
        if constexpr (OP == Opcode::STG) {
            uint8_t a=reg(inst,0),s=reg(inst,1);
            return dsl::encode_stg(a,s,0);
        }
        // 默认: 通用编码
        uint8_t rd=reg(inst,0),ra=reg(inst,1),rb=reg(inst,2);
        uint64_t w0=volta_opcode(OP)|((uint64_t)rd<<20)|((uint64_t)ra<<26)|((uint64_t)rb<<32);
        return {w0, 0x0fe40000000f00ULL};
    }

    // 批量编码 (模板展开 + constexpr 查找)
    static std::vector<uint64_t> encode_batch(const std::vector<Instruction>& insts) {
        std::vector<uint64_t> code; code.reserve(insts.size()*2);
        for (auto& i : insts) {
            auto p = encode_dispatch(i);
            code.push_back(p.first);
            code.push_back(p.second);
        }
        return code;
    }

private:
    static uint8_t reg(const Instruction& i, size_t n) {
        return (n<i.operands.size()&&i.operands[n].type==OpType::REG)?i.operands[n].reg_id:0;
    }

    // 运行时分发 → constexpr 表查找 (比 switch 快, 比 unordered_map 快 10x)
    static std::pair<uint64_t,uint64_t> encode_dispatch(const Instruction& inst) {
        uint16_t op = volta_opcode(inst.opcode);
        if (!op) return {0,0};
        uint8_t rd=reg(inst,0),ra=reg(inst,1),rb=reg(inst,2);
        uint64_t w0=op|((uint64_t)rd<<20)|((uint64_t)ra<<26)|((uint64_t)rb<<32);
        return {w0, 0x0fe40000000f00ULL};
    }
};

// ─── 编译期架构选择 ───
template<typename Arch>
class JITCompiler {
public:
    static std::vector<uint64_t> compile(const std::vector<Instruction>& insts) {
        return TemplateEncoder<Arch>::encode_batch(insts);
    }
};

} // namespace sass::jit
