/* HunTian SASS — 统一 ILP 模型系统
 *
 * 每个硬件架构独立建模，捕获:
 *   1. 指令延迟表 (每指令周期数)
 *   2. 执行端口模型 (端口冲突检测)
 *   3. 内存层次延迟 (L1/L2/L3/DRAM)
 *   4. 流水线参数 (发射宽度/ROB/物理寄存器)
 *   5. 硬件特殊特性 (warp/OoO/SMT)
 *
 * 数据来源:
 *   Pascal:  /data/rtl-sdr/ptx_gp106 (实测)
 *   Broadwell: /data/rtl-sdr/cpu_probe (实测)
 *   Volta+:   denvdis SM80/SM120 数据
 *   Zen4/5:   Intel SDM + AMD PPR (公开)
 */

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include "instruction.h"

namespace sass::ilp {

// ═══ 指令类型 (用于延迟查表) ═══
enum class InsnClass {
    FP32_ADD, FP32_MUL, FP32_FMA, FP32_DIV, FP32_SQRT,
    INT_ADD,  INT_MUL,  INT_SHIFT, INT_DIV,
    LD_GLOBAL, LD_SHARED, LD_CONST, LD_LOCAL, LD_L1, LD_L2, LD_L3, LD_DRAM,
    ST_GLOBAL, ST_SHARED, ST_LOCAL, ST_L1,
    BRANCH,   BARRIER,  SYNC,    NOP,
    TEXTURE,  SURFACE,  ATOMIC,  CONV,
    V512_FMA,  // VAVX3
    UNKNOWN
};

// 延迟条目: 指令→周期数 (多级缓存分别建模)
struct LatencyEntry {
    InsnClass iclass;
    int base_cycles;     // 基础延迟
    int throughput;      // 每周期可发射数 (倒数=间隔)
    int port_mask;       // 执行端口掩码
    const char* desc;
};

// ═══ 硬件架构参数 ═══
struct HardwareModel {
    std::string name;
    std::string arch;       // "sm_61", "x86_64", "zen4"
    int generation;

    // 流水线
    int issue_width;        // 每周期最多发射
    int retire_width;       // 每周期最多退休
    bool out_of_order;      // 乱序执行?
    int rob_size;           // Reorder Buffer (OoO 才有)
    int phys_regs;          // 物理寄存器数 (OoO 才有)

    // 执行单元
    int fma_units;          // FMA 单元数
    int fp_ports;           // 浮点端口数
    int int_ports;          // 整数端口数
    int ld_ports;           // 加载端口数
    int st_ports;           // 存储端口数

    // 内存层次 (周期)
    int l1_latency;
    int l2_latency;
    int l3_latency;
    int dram_latency;

    // 内存层次 (大小 KB)
    int l1_size_kb;         // 每核心
    int l2_size_kb;
    int l3_size_kb;

    // GPU 特殊 (替代CPU的OoO/ROB)
    int warps_per_sm;           // 每SM最大warp数 (0=CPU)
    int warp_size;              // warp线程数 (0=CPU)
    int sm_count;               // SM数 / CPU核心数
    int warp_inst_buffer;       // 每warp指令缓冲 (~4-8条)
    int scoreboard_regs;        // Scoreboard跟踪的寄存器数
    int max_active_warps;       // 实际可并发warp数
    bool zero_overhead_switch;  // 零开销warp切换? (GPU: true)
    bool has_tensor_cores;
    bool has_shared_memory;
    int smem_banks;

    // 每个指令类的延迟表
    std::vector<LatencyEntry> latency_table;

    // 查表
    int latency_for(InsnClass ic) const {
        for (auto& e : latency_table)
            if (e.iclass == ic) return e.base_cycles;
        return 1; // 默认
    }
};

// ═══════════════════════════════════════════════════════════
// Pascal GP106 (SM 6.1) — 来自 /data/rtl-sdr/ptx_gp106 实测
// ═══════════════════════════════════════════════════════════
inline HardwareModel pascal_gp106_model() {
    HardwareModel m;
    m.name = "Pascal GP106";
    m.arch = "sm_61";
    m.generation = 6;
    m.issue_width = 2;       // 每warp调度器
    m.retire_width = 2;
    m.out_of_order = false;
    m.rob_size = 0;
    m.phys_regs = 255;       // R0-R254
    m.fma_units = 128;       // 每SM 128 FP32 cores
    m.fp_ports = 2;          // 每warp调度器
    m.int_ports = 2;
    m.ld_ports = 1;
    m.st_ports = 1;
    m.l1_latency = 0;        // GPU 无传统L1 (shared mem替代)
    m.l2_latency = 200;      // ~300-500 周期, 取平均值
    m.l3_latency = 0;
    m.dram_latency = 350;    // 300-500
    m.l1_size_kb = 0;
    m.l2_size_kb = 1536;     // 1.5MB L2 (GP106 实测)
    m.l3_size_kb = 0;
    m.warps_per_sm = 64;     // 最多64 warps/SM
    m.warp_size = 32;
    m.sm_count = 10;         // GTX 1060
    m.warp_inst_buffer = 4;  // 每warp 4条指令缓冲
    m.scoreboard_regs = 256; // R0-R255
    m.max_active_warps = 64; // 理论64, 实际由寄存器/共享内存限制
    m.zero_overhead_switch = true;  // GPU零开销warp切换
    m.has_tensor_cores = false;
    m.has_shared_memory = true;
    m.smem_banks = 32;       // 32 banks (实测 stride=32 → 4.85x)

    m.latency_table = {
        {InsnClass::FP32_FMA,   4, 1, 0x3, "FFMA"},
        {InsnClass::FP32_ADD,   4, 1, 0x3, "FADD/FMUL"},
        {InsnClass::FP32_SQRT,  8, 4, 0x1, "MUFU.RSQ"},
        {InsnClass::FP32_DIV,   16,8, 0x1, "MUFU.RCP"},
        {InsnClass::INT_ADD,    4, 1, 0x3, "IADD/SHL"},
        {InsnClass::INT_MUL,    4, 1, 0x3, "IMAD/IMUL"},
        {InsnClass::INT_SHIFT,  4, 1, 0x3, "SHF"},
        {InsnClass::LD_GLOBAL,  350,1, 0x4, "LDG"},
        {InsnClass::LD_SHARED,  8,  1, 0x8, "LDS"},
        {InsnClass::LD_CONST,   4,  1, 0x4, "LDC"},
        {InsnClass::ST_GLOBAL,  10, 1, 0x4, "STG"},
        {InsnClass::ST_SHARED,  5,  1, 0x8, "STS"},
        {InsnClass::BRANCH,     4,  1, 0x1, "BRA"},
        {InsnClass::BARRIER,    4,  2, 0x1, "BAR"},
        {InsnClass::SYNC,       4,  2, 0x1, "DEPBAR"},
        {InsnClass::NOP,        1,  4, 0xF, "NOP"},
        {InsnClass::CONV,       4,  1, 0x3, "I2F/F2I"},
        {InsnClass::TEXTURE,    100,1, 0x1, "TEX"},
        {InsnClass::ATOMIC,     50, 2, 0x4, "ATOM"},
        {InsnClass::V512_FMA,   1,  8, 0xF, "VAVX3"},  // 虚拟指令
    };
    return m;
}

// ═══════════════════════════════════════════════════════════
// Volta GV100 (SM 7.0) — 来自 denvdis + cuobjdump
// ═══════════════════════════════════════════════════════════
inline HardwareModel volta_gv100_model() {
    auto m = pascal_gp106_model();
    m.name = "Volta GV100";
    m.arch = "sm_70";
    m.generation = 7;
    m.warps_per_sm = 64;
    m.sm_count = 80;
    m.warp_inst_buffer = 4;
    m.scoreboard_regs = 256;
    m.max_active_warps = 64;
    m.zero_overhead_switch = true;
    m.has_tensor_cores = true;
    m.l2_size_kb = 6144;  // 6MB L2
    // Volta: tensor core HMMA, 128-bit指令
    m.latency_table.push_back({InsnClass::V512_FMA, 1, 4, 0xF, "HMMA tensor"});
    return m;
}

// ═══════════════════════════════════════════════════════════
// Ampere GA100 (SM 8.0) — 来自 denvdis SM80
// ═══════════════════════════════════════════════════════════
inline HardwareModel ampere_ga100_model() {
    auto m = volta_gv100_model();
    m.name = "Ampere GA100";
    m.arch = "sm_80";
    m.generation = 8;
    m.l2_size_kb = 40960; // 40MB L2
    m.sm_count = 108;
    // Ampere: FP16/BF16 native, MMA.SYNC
    return m;
}

// ═══════════════════════════════════════════════════════════
// Hopper GH100 (SM 9.0) — 来自 DeepGEMM + denvdis SM120
// ═══════════════════════════════════════════════════════════
inline HardwareModel hopper_gh100_model() {
    auto m = ampere_ga100_model();
    m.name = "Hopper GH100";
    m.arch = "sm_90";
    m.generation = 9;
    m.warps_per_sm = 64;
    m.sm_count = 132;
    m.l2_size_kb = 51200;  // 50MB L2
    m.has_tensor_cores = true;
    // Hopper: WGMMA, TMA, FP8
    return m;
}

// ═══════════════════════════════════════════════════════════
// Intel Broadwell-EP — 来自 /data/rtl-sdr/cpu_probe 实测
// ═══════════════════════════════════════════════════════════
inline HardwareModel broadwell_model() {
    HardwareModel m;
    m.name = "Intel Broadwell-EP";
    m.arch = "x86_64";
    m.generation = 5;
    m.issue_width = 4;
    m.retire_width = 4;
    m.out_of_order = true;
    m.rob_size = 192;
    m.phys_regs = 168;
    m.fma_units = 2;        // Port 0,1
    m.fp_ports = 3;         // Port 0,1,5
    m.int_ports = 2;        // Port 6,7
    m.ld_ports = 2;         // Port 2,3
    m.st_ports = 1;         // Port 4
    m.l1_latency = 4;       // L1d: 4-cycle hit
    m.l2_latency = 12;      // L2: 12-cycle hit
    m.l3_latency = 42;      // L3: 42-cycle hit (ring bus)
    m.dram_latency = 200;   // ~70ns @ 2.3GHz = 160cyc
    m.l1_size_kb = 32;      // 每核32KB L1d
    m.l2_size_kb = 256;     // 每核256KB L2
    m.l3_size_kb = 46080;   // 45MB 共享L3
    m.warps_per_sm = 0;     // GPU 参数不适用
    m.warp_size = 0;
    m.sm_count = 18;        // 18核 / 36线程
    m.has_tensor_cores = false;
    m.has_shared_memory = false;
    m.smem_banks = 0;

    m.latency_table = {
        {InsnClass::FP32_FMA,   5, 2, 0x3, "VFMADD231PS"},  // 实测 5c, 2/cycle
        {InsnClass::FP32_ADD,   3, 3, 0x7, "VADDPS"},        // 3c
        {InsnClass::FP32_MUL,   3, 2, 0x3, "VMULPS"},        // 3c
        {InsnClass::FP32_SQRT,  12,1, 0x1, "VSQRTPS"},       // 12c
        {InsnClass::FP32_DIV,   14,1, 0x1, "VDIVPS"},        // 14c
        {InsnClass::INT_ADD,    1, 4, 0xF, "VPADDD"},        // 1c
        {InsnClass::INT_MUL,    5, 1, 0x3, "VPMULLD"},       // 5c
        {InsnClass::INT_SHIFT,  1, 2, 0x3, "VPSLLVD"},       // 1c
        {InsnClass::LD_L1,      4, 2, 0x6, "L1d hit"},       // 4c
        {InsnClass::LD_L2,      12,1, 0x6, "L2 hit"},        // 12c
        {InsnClass::LD_L3,      42,1, 0x6, "L3 hit"},        // 42c
        {InsnClass::LD_DRAM,    160,1,0x6, "DRAM"},          // ~160c
        {InsnClass::ST_L1,      1, 1, 0x8, "store"},          // 1c
        {InsnClass::BRANCH,     1, 1, 0x1, "jmp"},            // 预测命中
        {InsnClass::BARRIER,    33,1, 0x1, "mfence"},         // 33c
        {InsnClass::NOP,        1, 4, 0xF, "nop"},
        {InsnClass::CONV,       4, 1, 0x3, "VCVTDQ2PS"},
    };
    return m;
}

// ═══════════════════════════════════════════════════════════
// AMD Zen4 — 来自 AMD PPR + Agner Fog
// ═══════════════════════════════════════════════════════════
inline HardwareModel zen4_model() {
    auto m = broadwell_model();
    m.name = "AMD Zen4";
    m.arch = "zen4";
    m.generation = 4;
    m.issue_width = 6;
    m.retire_width = 8;
    m.rob_size = 320;
    m.phys_regs = 224;
    m.fma_units = 4;
    m.fp_ports = 6;
    m.int_ports = 4;
    m.ld_ports = 3;
    m.st_ports = 2;
    m.l1_latency = 4;
    m.l2_latency = 12;
    m.l3_latency = 50;     // 更大L3, 略慢
    m.dram_latency = 220;  // DDR5 ~90ns @5GHz = 450c... 取平均值
    m.l1_size_kb = 32;
    m.l2_size_kb = 1024;   // 每核1MB L2
    m.l3_size_kb = 32768;  // 32MB 共享L3
    m.sm_count = 8;        // 8核 (典型 Ryzen 7)
    // Zen4 FMA: 4c latency, 1/cycle per port
    for (auto& e : m.latency_table) {
        if (e.iclass == InsnClass::FP32_FMA) {
            e.base_cycles = 4;   // Zen4: 4c (AVX-512 double-pumped)
            e.throughput = 2;    // 2 FMA/cycle (2×256-bit)
        }
    }
    return m;
}

// ═══════════════════════════════════════════════════════════
// AMD Zen5 — AVX-512 全宽度
// ═══════════════════════════════════════════════════════════
inline HardwareModel zen5_model() {
    auto m = zen4_model();
    m.name = "AMD Zen5";
    m.arch = "zen5";
    m.generation = 5;
    m.issue_width = 8;
    m.rob_size = 448;
    m.phys_regs = 256;
    m.fma_units = 4;       // 512-bit 原生
    m.l2_size_kb = 1024;
    m.l3_size_kb = 65536;  // 64MB 共享L3
    m.sm_count = 16;       // 16核 (典型 Ryzen 9)
    for (auto& e : m.latency_table) {
        if (e.iclass == InsnClass::FP32_FMA) {
            e.base_cycles = 3;   // Zen5: 3c
            e.throughput = 2;    // 4 FMA/cycle
        }
    }
    return m;
}

// ═══════════════════════════════════════════════════════════
// AMD RDNA4 — 来自 /data/rtl-sdr/swmmac 实测
// ═══════════════════════════════════════════════════════════
inline HardwareModel rdna4_model() {
    HardwareModel m;
    m.name = "AMD RDNA4 (RX 9060 XT)";
    m.arch = "gfx1200";
    m.generation = 12;
    m.issue_width = 1;        // 每SIMD 1条/周期
    m.retire_width = 1;
    m.out_of_order = false;   // GPU in-order
    m.rob_size = 0;
    m.phys_regs = 256;
    m.fma_units = 64;         // 每CU 64 FP32 (32 CUs ×64 = 2048)
    m.fp_ports = 2;
    m.int_ports = 2;
    m.ld_ports = 1;
    m.st_ports = 1;
    m.l1_latency = 0;
    m.l2_latency = 100;       // Infinity Cache ~100c
    m.l3_latency = 0;
    m.dram_latency = 300;     // VRAM ~300c
    m.l1_size_kb = 0;
    m.l2_size_kb = 65536;     // 64MB Infinity Cache (RX 9060 XT)
    m.l3_size_kb = 0;
    m.warps_per_sm = 64;      // 每CU 64 wavefronts
    m.warp_size = 32;         // Wave32
    m.sm_count = 32;          // 32 CUs (retail, rocminfo确认)
    // Peak: 32×2×32×2×2×3.0GHz=24.6 TFLOPS (rocminfo max 3000MHz)
    // Official: 25.6 TFLOPS @ ~3.125GHz boost
    m.warp_inst_buffer = 4;
    m.scoreboard_regs = 256;
    m.max_active_warps = 64;
    m.zero_overhead_switch = true;
    m.has_tensor_cores = false; // 使用 WMMA 而非 Tensor Cores
    m.has_shared_memory = true;
    m.smem_banks = 32;

    m.latency_table = {
        {InsnClass::FP32_FMA,   4, 1, 0x3, "v_fmac_f32"},
        {InsnClass::FP32_ADD,   4, 1, 0x3, "v_add_f32"},
        {InsnClass::FP32_MUL,   4, 1, 0x3, "v_mul_f32"},
        {InsnClass::FP32_SQRT,  16,1, 0x1, "v_sqrt_f32"},
        {InsnClass::INT_ADD,    4, 1, 0x3, "v_add_u32"},
        {InsnClass::INT_MUL,    16,1, 0x3, "v_mul_lo_u32"},
        {InsnClass::LD_GLOBAL,  300,1, 0x4, "global_load"},
        {InsnClass::LD_SHARED,  20, 1, 0x8, "ds_read"},
        {InsnClass::ST_GLOBAL,  300,1, 0x4, "global_store"},
        {InsnClass::ST_SHARED,  20, 1, 0x8, "ds_write"},
        {InsnClass::BRANCH,     4,  1, 0x1, "s_branch"},
        {InsnClass::BARRIER,    20, 1, 0x1, "s_barrier"},
        {InsnClass::NOP,        1,  4, 0xF, "s_nop"},
        {InsnClass::CONV,       4,  1, 0x3, "v_cvt_f32_i32"},
    };
    return m;
}

// ═══════════════════════════════════════════════════════════
// ARM Cortex-A78 (in-order, 最需要ILP)
// ═══════════════════════════════════════════════════════════
inline HardwareModel cortex_a78_model() {
    HardwareModel m;
    m.name = "ARM Cortex-A78";
    m.arch = "arm64";
    m.generation = 8;
    m.issue_width = 4;
    m.retire_width = 4;
    m.out_of_order = true;   // A78 有 OoO (小R0B)
    m.rob_size = 160;
    m.phys_regs = 128;
    m.fma_units = 2;         // NEON 128-bit, 2 FMA/cycle
    m.fp_ports = 2;
    m.int_ports = 3;
    m.ld_ports = 2;
    m.st_ports = 2;
    m.l1_latency = 3;
    m.l2_latency = 10;
    m.l3_latency = 30;
    m.dram_latency = 150;
    m.l1_size_kb = 64;
    m.l2_size_kb = 512;
    m.l3_size_kb = 4096;
    m.sm_count = 8;
    m.latency_table = {
        {InsnClass::FP32_FMA,   4, 2, 0x3, "FMLA (NEON)"},
        {InsnClass::FP32_ADD,   3, 2, 0x3, "FADD"},
        {InsnClass::FP32_MUL,   3, 2, 0x3, "FMUL"},
        {InsnClass::INT_ADD,    1, 4, 0x7, "ADD"},
        {InsnClass::INT_MUL,    3, 1, 0x3, "MUL"},
        {InsnClass::LD_L1,      3, 2, 0x6, "LDR (L1)"},
        {InsnClass::ST_L1,      1, 1, 0x8, "STR"},
        {InsnClass::BRANCH,     1, 1, 0x1, "B"},
        {InsnClass::BARRIER,    10,1, 0x1, "DMB"},
        {InsnClass::NOP,        1, 4, 0xF, "NOP"},
    };
    return m;
}

// ═══ 架构→模型 注册表 ═══
inline const HardwareModel& model_for(const std::string& sm) {
    static std::unordered_map<std::string,HardwareModel> cache;
    if (auto it=cache.find(sm); it!=cache.end()) return it->second;

    if (sm=="sm_61"||sm=="sm_60"||sm=="sm_62") cache[sm]=pascal_gp106_model();
    else if (sm=="sm_70"||sm=="sm_72") cache[sm]=volta_gv100_model();
    else if (sm=="sm_75") cache[sm]=volta_gv100_model();
    else if (sm=="sm_80"||sm=="sm_86") cache[sm]=ampere_ga100_model();
    else if (sm=="sm_89") cache[sm]=ampere_ga100_model();
    else if (sm=="sm_90") cache[sm]=hopper_gh100_model();
    else if (sm=="sm_100"||sm=="sm_101"||sm=="sm_120") cache[sm]=hopper_gh100_model();
    else if (sm=="x86_64"||sm=="cpu"||sm=="broadwell") cache[sm]=broadwell_model();
    else if (sm=="zen3"||sm=="zen4"||sm=="amd64"||sm=="amd") cache[sm]=zen4_model();
    else if (sm=="zen5") cache[sm]=zen5_model();
    else if (sm=="arm64"||sm=="a78") cache[sm]=cortex_a78_model();
    else if (sm=="gfx1200"||sm=="rdna4"||sm=="rx9060") cache[sm]=rdna4_model();
    else cache[sm]=pascal_gp106_model(); // 默认
    return cache[sm];
}

// ═══ ILP 调度器 (使用架构模型) ═══
class ILPSchedulerV2 {
    const HardwareModel* model_;
    std::vector<int> last_writer;     // 每寄存器最后写入周期
    std::vector<int> port_busy;       // 每端口最后使用周期

public:
    explicit ILPSchedulerV2(const HardwareModel& m)
        : model_(&m), last_writer(256, -1), port_busy(8, 0) {}

    // 调度指令序列
    void schedule(std::vector<Instruction>& insts) {
        if (insts.empty()) return;
        int cycle = 0;
        size_t issued = 0;
        std::vector<bool> done(insts.size(), false);
        std::vector<Instruction> result;

        while (issued < insts.size()) {
            int slot_issued = 0;
            for (size_t i=0;i<insts.size()&&slot_issued<model_->issue_width;i++) {
                if (done[i]) continue;
                if (can_issue(insts[i], cycle)) {
                    issue(insts[i], cycle);
                    result.push_back(insts[i]);
                    done[i]=true; issued++; slot_issued++;
                }
            }
            if (slot_issued==0) {  // 强制发射一条
                for (size_t i=0;i<insts.size();i++)
                    if (!done[i]) { issue(insts[i],cycle); result.push_back(insts[i]);
                        done[i]=true; issued++; break; }
            }
            cycle++;
        }
        insts = std::move(result);
    }

    // 估算周期数
    int estimate_cycles(const std::vector<Instruction>& insts) {
        int cycle=0; size_t issued=0;
        std::vector<bool> done(insts.size(),false);
        while (issued<insts.size()) {
            int slot_issued=0;
            for (size_t i=0;i<insts.size()&&slot_issued<model_->issue_width;i++)
                if (!done[i]&&can_issue(insts[i],cycle))
                    { done[i]=true; issued++; slot_issued++; }
            if (slot_issued==0) { done[issued]=true; issued++; }
            cycle++;
        }
        return cycle;
    }

private:
    bool can_issue(const Instruction& inst, int cycle) const {
        // 检查源操作数就绪
        for (size_t i=1;i<inst.operands.size();i++) {
            if (inst.operands[i].type!=OpType::REG) continue;
            int lw=last_writer[inst.operands[i].reg_id];
            if (lw>=0) {
                InsnClass ic=classify(inst.opcode);
                int latency=model_->latency_for(ic);
                if (cycle-lw<latency) return false;
            }
        }
        return true;
    }

    void issue(const Instruction& inst, int cycle) {
        if (!inst.operands.empty()&&inst.operands[0].type==OpType::REG)
            last_writer[inst.operands[0].reg_id]=cycle;
    }

    static InsnClass classify(Opcode op) {
        switch (op) {
            case Opcode::FFMA: case Opcode::DFMA: return InsnClass::FP32_FMA;
            case Opcode::FADD: case Opcode::FMUL: return InsnClass::FP32_ADD;
            case Opcode::FSQRT: return InsnClass::FP32_SQRT;
            case Opcode::FRCP: return InsnClass::FP32_DIV;
            case Opcode::IADD: case Opcode::ISUB: case Opcode::IADD3:
                return InsnClass::INT_ADD;
            case Opcode::IMUL: case Opcode::IMAD: return InsnClass::INT_MUL;
            case Opcode::SHL: case Opcode::SHR: case Opcode::SHF:
                return InsnClass::INT_SHIFT;
            case Opcode::LDG: case Opcode::LDC: return InsnClass::LD_GLOBAL;
            case Opcode::LDS: return InsnClass::LD_SHARED;
            case Opcode::STG: return InsnClass::ST_GLOBAL;
            case Opcode::STS: return InsnClass::ST_SHARED;
            case Opcode::BRA: case Opcode::BRX: case Opcode::CAL:
                return InsnClass::BRANCH;
            case Opcode::BAR: case Opcode::DEPBAR: return InsnClass::BARRIER;
            case Opcode::EXIT: case Opcode::RET: return InsnClass::BRANCH;
            case Opcode::I2F: case Opcode::F2I: case Opcode::CVT:
                return InsnClass::CONV;
            case Opcode::SULD: case Opcode::TEX: return InsnClass::TEXTURE;
            case Opcode::ATOM: case Opcode::RED: return InsnClass::ATOMIC;
            default: return InsnClass::UNKNOWN;
        }
    }
};

} // namespace sass::ilp
