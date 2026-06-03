/* HunTian SASS — AMD RDNA4 后端 (RX 9060 XT, gfx1200)
 *
 * AMD GPU 架构:
 *   RDNA4 = Wave32 + WMMA (矩阵乘加) + 2:4 稀疏
 *   Wavefront = 32 lanes (同NVIDIA warp)
 *   VGPR = 256 向量寄存器
 *   SGPR = 128 标量寄存器
 *
 * 指令编码 (AMD GPU 汇编):
 *   v_fmac_f32  → 浮点乘加
 *   global_load → 全局加载
 *   s_endpgm    → 程序结束 (同EXIT)
 *   s_waitcnt   → 等待计数器 (同BAR/DEPBAR)
 *
 * 数据来源:
 *   /data/rtl-sdr/swmmac (本地benchmark)
 *   /data/ROCm/rocWMMA (WMMA API)
 *   AMD GPU ISA docs
 */

#include "device_backend.h"
#include "instruction.h"
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace sass {

// AMD GPU 指令映射
struct AmdOpInfo {
    Opcode hun_op;
    const char* amd_mnemonic;  // AMD GPU 汇编
    int operands;
    int latency;      // RDNA4 延迟 (wavefront级)
    const char* notes;
};

// 基于 AMD RDNA ISA + rocWMMA 文档
static const AmdOpInfo amd_op_table[] = {
    // 浮点
    {Opcode::FFMA,  "v_fmac_f32",       3, 4, "FMA: 4c latency"},
    {Opcode::FADD,  "v_add_f32",        2, 4, "FP32 add"},
    {Opcode::FMUL,  "v_mul_f32",        2, 4, "FP32 mul"},
    {Opcode::FSQRT, "v_sqrt_f32",       2, 16,""},
    {Opcode::FRCP,  "v_rcp_f32",        2, 8, ""},
    {Opcode::FMNMX, "v_max_f32/v_min_f32",3,4,""},
    {Opcode::FCMP,  "v_cmp_gt_f32",     3, 4, ""},
    {Opcode::DFMA,  "v_fmac_f64",       3, 8, "双精度FMA"},
    // 整数
    {Opcode::IADD,  "v_add_u32",        2, 4, ""},
    {Opcode::IMUL,  "v_mul_lo_u32",     2, 16,""},
    {Opcode::SHL,   "v_lshlrev_b32",    3, 4, ""},
    {Opcode::SHR,   "v_lshrrev_b32",    3, 4, ""},
    // 数据移动
    {Opcode::MOV,   "v_mov_b32",        2, 4, ""},
    // 内存
    {Opcode::LDG,   "global_load_dword", 2, 300,"VRAM ~300c"},
    {Opcode::STG,   "global_store_dword",2, 300,""},
    {Opcode::LDS,   "ds_read_b32",      2, 20, "LDS ~20c"},
    {Opcode::STS,   "ds_write_b32",     2, 20, ""},
    // 控制流
    {Opcode::EXIT,  "s_endpgm",         0, 4, ""},
    {Opcode::RET,   "s_endpgm",         0, 4, ""},
    {Opcode::BRA,   "s_branch",         1, 4, ""},
    {Opcode::CAL,   "s_swappc_b64",     1, 16,""},
    // 同步
    {Opcode::BAR,   "s_barrier",        0, 20, "workgroup barrier"},
    {Opcode::DEPBAR,"s_waitcnt vmcnt(0)",0,4,"等待内存完成"},
    {Opcode::MEMBAR,"s_waitcnt lgkmcnt(0)",0,4,""},
    // WMMA (矩阵乘加)
    {Opcode::IMAD,  "v_wmma_f32_16x16x16_f16",3,8,"WMMA FP16"},
    // 转换
    {Opcode::I2F,   "v_cvt_f32_i32",    2, 4, ""},
    {Opcode::F2I,   "v_cvt_i32_f32",    2, 4, ""},
    {Opcode::CVT,   "v_cvt_f32_f16",    2, 4, ""},
    // 杂项
    {Opcode::BPT,   "s_trap 2",         0, 4, "breakpoint"},
    {Opcode::UNKNOWN,nullptr,0,0,nullptr},
};

class AmdRDNA4Backend : public IDeviceBackend {
public:
    ArchInfo arch() const override {
        return {"AMD RDNA4", "gfx1200", 1200, 12};
    }

    std::vector<SASSWord> encode(const ManifoldResult& result) override {
        std::vector<SASSWord> code;
        code.reserve(result.optimized_sequence.size());
        for (auto& inst : result.optimized_sequence) {
            auto info = lookup(inst.opcode);
            if (info) {
                // AMD标记: 0xE0 << 56
                SASSWord w = 0xE0ULL << 56;
                w |= (uint64_t)(inst.opcode) << 48;
                code.push_back(w);
            }
        }
        return code;
    }

    std::vector<SASSWord> encode_with_labels(const ManifoldResult& r,
        const std::unordered_map<std::string,uint32_t>&) override {
        return encode(r);
    }

    std::string disassemble(SASSWord w) const override {
        uint8_t tag = (w >> 56) & 0xFF;
        if (tag == 0xE0) {
            Opcode op = static_cast<Opcode>((w >> 48) & 0xFF);
            auto info = lookup(op);
            return info ? info->amd_mnemonic : "???";
        }
        return pascal_disassemble(w);  // 回退到 Pascal
    }

    std::vector<std::string> disassemble_block(const std::vector<SASSWord>& c) const override {
        std::vector<std::string> lines; lines.reserve(c.size());
        for (size_t i=0;i<c.size();i++) {
            char buf[128]; snprintf(buf,sizeof(buf),"/*%04zx*/ %s",i,disassemble(c[i]).c_str());
            lines.push_back(buf);
        }
        return lines;
    }

    uint8_t opcode_byte(Opcode op) const override { auto i=lookup(op); return i?i->latency:0; }
    const char* opcode_name(Opcode op) const override { auto i=lookup(op); return i?i->amd_mnemonic:"???"; }
    int operand_count(Opcode op) const override { auto i=lookup(op); return i?i->operands:0; }
    bool supports(Opcode op) const override { return lookup(op)!=nullptr; }
    size_t total_instructions() const override { return 32; }

    // ─── RDNA4 特定参数 ───
    struct RDNA4Tuning {
        int fma_latency = 4;       // RDNA4: 4c
        int wave_size = 32;        // Wave32
        int vgpr_count = 256;      // 向量寄存器
        int sgpr_count = 128;      // 标量寄存器
        int cu_count = 32;         // RX 9060 XT: 32 CU (16 WGP ×2)  swmmac docs确认
        int lds_size_kb = 64;      // 每CU 64KB LDS
        int l2_size_mb = 64;       // 64MB L2 (Infinity Cache)
        bool has_wmma = true;      // WMMA (v_wmma_*)
        bool has_sparse = true;    // 2:4 稀疏
    };
    RDNA4Tuning tuning;

private:
    static const AmdOpInfo* lookup(Opcode op) {
        for (int i=0;amd_op_table[i].amd_mnemonic;i++)
            if (amd_op_table[i].hun_op==op) return &amd_op_table[i];
        return nullptr;
    }
};

} // namespace sass
