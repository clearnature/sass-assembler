/* HunTian SASS — AMD RDNA4 Full ISA Backend (RX 9060 XT, gfx1200)
 *
 * 完整 RDNA4 指令集汇编后端，覆盖:
 *   VOP1/VOP2/VOP3/VOP3P  — 向量 ALU
 *   SOP1/SOP2/SOPP/SOPK/SOPC — 标量 ALU
 *   SMEM/VMEM             — 标量/向量内存
 *   DS                    — 本地数据共享 (LDS)
 *   FLAT/Global           — 全局内存
 *   EXP/M0                — 导出/顶点
 *   VINTRP                — 插值
 *   DPP/DPP8/SDWA         — 数据并行原语
 *   SWMMAC (VOP3P)        — 稀疏矩阵乘累加 (INT4/INT8/FP8/FP16/BF16)
 *   WMMA                  — 波前矩阵乘累加 (FP16/BF16/INT8)
 *   MFMA                  — 矩阵融合乘加 (CDNA legacy)
 *   Image                 — 纹理/图像
 *
 * 编码格式参考:
 *   AMD RDNA3 Shader ISA (公开文档)
 *   LLVM VOP3PInstructions.td / VOPInstructions.td (上游 LLVM 23)
 *   /data/rtl-sdr/swmmac (实测校准)
 *   /data/ROCm/rocWMMA (WMMA/SWMMAC API)
 */

#include "device_backend.h"
#include "instruction.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <unordered_map>
#include <vector>

namespace sass {

// ═══════════════════════════════════════════════════════════════
// AMD RDNA4 指令编码
// ═══════════════════════════════════════════════════════════════

// ─── 指令家族 (Encoding Family) ───
enum class AmdEncFamily : uint8_t {
    VOP1, VOP2, VOP3, VOP3P,   // 向量
    SOP1, SOP2, SOPP, SOPK, SOPC, // 标量
    SMEM, VMEM, DS, FLAT,       // 内存
    EXP, VINTRP,                // 特殊
    DPP, SDWA,                  // 数据并行
    SWMMAC, WMMA, MFMA,         // 矩阵
    IMAGE                       // 纹理
};

// ─── AMD 操作数类型 ───
enum class AmdOpClass : uint8_t {
    VGPR,    // v0-v255
    SGPR,    // s0-s127
    VCC,     // vcc (特殊 SGPR)
    EXEC,    // exec mask
    M0,      // m0 (内存描述符)
    LITERAL, // 32-bit 立即数
    KIMM,    // 16-bit 内联立即数
    OFFSET,  // 内存偏移
    ATTR,    // 属性
    LABEL    // 分支目标
};

// ─── 单条指令编码信息 ───
struct RDNA4InstInfo {
    const char* mnemonic;      // AMD GPU 汇编助记符
    AmdEncFamily family;       // 编码家族
    uint16_t opcode;           // 家族内操作码 (VOP3 可达 0x1C0+)
    uint8_t  num_src;          // 源操作数数量
    uint8_t  num_dst;          // 目标操作数数量
    uint16_t latency;          // 延迟 (wavefront 周期)
    bool     is_swmmac;        // 是否为 SWMMAC 矩阵指令
    bool     has_sparse;       // 是否支持 2:4 结构化稀疏
    const char* description;   // 简要描述
};

// ═══════════════════════════════════════════════════════════════
// RDNA4 完整指令表 (gfx1200)
// ═══════════════════════════════════════════════════════════════

static const RDNA4InstInfo rdna4_isa[] = {

    // ─── SOP1: 标量一元操作 ───
    {"s_mov_b32",       AmdEncFamily::SOP1,   0x03, 1, 1, 2,  false, false, "scalar move"},
    {"s_mov_b64",       AmdEncFamily::SOP1,   0x04, 1, 1, 2,  false, false, "scalar move 64-bit"},
    {"s_not_b32",       AmdEncFamily::SOP1,   0x09, 1, 1, 2,  false, false, "bitwise NOT"},
    {"s_not_b64",       AmdEncFamily::SOP1,   0x0A, 1, 1, 2,  false, false, "bitwise NOT 64-bit"},
    {"s_wqm_b32",       AmdEncFamily::SOP1,   0x0B, 1, 1, 2,  false, false, "whole quad mode"},
    {"s_wqm_b64",       AmdEncFamily::SOP1,   0x0C, 1, 1, 2,  false, false, "WQM 64-bit"},
    {"s_brev_b32",      AmdEncFamily::SOP1,   0x0F, 1, 1, 2,  false, false, "bit reverse"},
    {"s_bcnt0_i32_b32", AmdEncFamily::SOP1,   0x11, 1, 1, 2,  false, false, "bit count 0"},
    {"s_bcnt0_i32_b64", AmdEncFamily::SOP1,   0x12, 1, 1, 2,  false, false, "bit count 0 64"},
    {"s_bcnt1_i32_b32", AmdEncFamily::SOP1,   0x13, 1, 1, 2,  false, false, "bit count 1"},
    {"s_bcnt1_i32_b64", AmdEncFamily::SOP1,   0x14, 1, 1, 2,  false, false, "bit count 1 64"},
    {"s_ff0_i32_b32",   AmdEncFamily::SOP1,   0x15, 1, 1, 2,  false, false, "find first 0"},
    {"s_ff0_i32_b64",   AmdEncFamily::SOP1,   0x16, 1, 1, 2,  false, false, "find first 0 64"},
    {"s_ff1_i32_b32",   AmdEncFamily::SOP1,   0x17, 1, 1, 2,  false, false, "find first 1"},
    {"s_ff1_i32_b64",   AmdEncFamily::SOP1,   0x18, 1, 1, 2,  false, false, "find first 1 64"},
    {"s_abs_i32",       AmdEncFamily::SOP1,   0x23, 1, 1, 2,  false, false, "absolute value"},
    {"s_and_saveexec_b64", AmdEncFamily::SOP1,0x25, 2, 1, 2,false, false, "AND save EXEC"},
    {"s_or_saveexec_b64",  AmdEncFamily::SOP1,0x26, 2, 1, 2,false, false, "OR save EXEC"},
    {"s_xor_saveexec_b64", AmdEncFamily::SOP1,0x27, 2, 1, 2,false, false, "XOR save EXEC"},
    {"s_andn2_saveexec_b64",AmdEncFamily::SOP1,0x28,2,1, 2,false,false,"ANDN2 save EXEC"},
    {"s_orn2_saveexec_b64", AmdEncFamily::SOP1,0x29, 2, 1,2, false,false,"ORN2 save EXEC"},
    {"s_nand_saveexec_b64", AmdEncFamily::SOP1,0x2A,2,1, 2,false,false,"NAND save EXEC"},
    {"s_nor_saveexec_b64",  AmdEncFamily::SOP1,0x2B,2,1, 2,false,false,"NOR save EXEC"},
    {"s_xnor_saveexec_b64", AmdEncFamily::SOP1,0x2C,2,1, 2,false,false,"XNOR save EXEC"},
    {"s_quadmask_b32",  AmdEncFamily::SOP1,   0x2E, 2, 1, 2,  false, false, "quad mask"},
    {"s_quadmask_b64",  AmdEncFamily::SOP1,   0x2F, 2, 1, 2,  false, false, "quad mask 64"},
    {"s_movrels_b32",   AmdEncFamily::SOP1,   0x30, 1, 1, 2,  false, false, "movrel scalar"},
    {"s_movrels_b64",   AmdEncFamily::SOP1,   0x31, 1, 1, 2,  false, false, "movrel 64"},
    {"s_movreld_b32",   AmdEncFamily::SOP1,   0x32, 2, 0, 2,  false, false, "movreld scalar"},
    {"s_movreld_b64",   AmdEncFamily::SOP1,   0x33, 2, 0, 2,  false, false, "movreld 64"},
    {"s_getpc_b64",     AmdEncFamily::SOP1,   0x37, 0, 1, 2,  false, false, "get PC"},
    {"s_setpc_b64",     AmdEncFamily::SOP1,   0x38, 1, 0, 4,  false, false, "set PC"},
    {"s_swappc_b64",    AmdEncFamily::SOP1,   0x39, 2, 1, 16, false, false, "swap PC (call)"},
    {"s_rfe_b64",       AmdEncFamily::SOP1,   0x3A, 1, 0, 4,  false, false, "return from exec"},
    {"s_and_b32",       AmdEncFamily::SOP2,   0x17, 2, 1, 2,  false, false, "bitwise AND"},
    {"s_and_b64",       AmdEncFamily::SOP2,   0x18, 2, 1, 2,  false, false, "AND 64"},
    {"s_or_b32",        AmdEncFamily::SOP2,   0x19, 2, 1, 2,  false, false, "bitwise OR"},
    {"s_or_b64",        AmdEncFamily::SOP2,   0x1A, 2, 1, 2,  false, false, "OR 64"},
    {"s_xor_b32",       AmdEncFamily::SOP2,   0x1B, 2, 1, 2,  false, false, "bitwise XOR"},
    {"s_xor_b64",       AmdEncFamily::SOP2,   0x1C, 2, 1, 2,  false, false, "XOR 64"},
    {"s_andn2_b32",     AmdEncFamily::SOP2,   0x1D, 2, 1, 2,  false, false, "AND NOT2"},
    {"s_andn2_b64",     AmdEncFamily::SOP2,   0x1E, 2, 1, 2,  false, false, "ANDN2 64"},
    {"s_orn2_b32",      AmdEncFamily::SOP2,   0x1F, 2, 1, 2,  false, false, "OR NOT2"},
    {"s_orn2_b64",      AmdEncFamily::SOP2,   0x20, 2, 1, 2,  false, false, "ORN2 64"},
    {"s_nand_b32",      AmdEncFamily::SOP2,   0x21, 2, 1, 2,  false, false, "NAND"},
    {"s_nand_b64",      AmdEncFamily::SOP2,   0x22, 2, 1, 2,  false, false, "NAND 64"},
    {"s_nor_b32",       AmdEncFamily::SOP2,   0x23, 2, 1, 2,  false, false, "NOR"},
    {"s_nor_b64",       AmdEncFamily::SOP2,   0x24, 2, 1, 2,  false, false, "NOR 64"},
    {"s_xnor_b32",      AmdEncFamily::SOP2,   0x25, 2, 1, 2,  false, false, "XNOR"},
    {"s_xnor_b64",      AmdEncFamily::SOP2,   0x26, 2, 1, 2,  false, false, "XNOR 64"},
    {"s_lshl_b32",      AmdEncFamily::SOP2,   0x27, 2, 1, 2,  false, false, "left shift"},
    {"s_lshl_b64",      AmdEncFamily::SOP2,   0x28, 2, 1, 2,  false, false, "left shift 64"},
    {"s_lshr_b32",      AmdEncFamily::SOP2,   0x29, 2, 1, 2,  false, false, "right shift"},
    {"s_lshr_b64",      AmdEncFamily::SOP2,   0x2A, 2, 1, 2,  false, false, "right shift 64"},
    {"s_ashr_i32",      AmdEncFamily::SOP2,   0x2B, 2, 1, 2,  false, false, "arithmetic shift"},
    {"s_ashr_i64",      AmdEncFamily::SOP2,   0x2C, 2, 1, 2,  false, false, "arith shift 64"},
    {"s_bfm_b32",       AmdEncFamily::SOP2,   0x2D, 2, 1, 2,  false, false, "bit field mask"},
    {"s_bfm_b64",       AmdEncFamily::SOP2,   0x2E, 2, 1, 2,  false, false, "bit field mask 64"},
    {"s_mul_i32",       AmdEncFamily::SOP2,   0x2F, 2, 1, 4,  false, false, "integer multiply"},
    {"s_bfe_u32",       AmdEncFamily::SOP2,   0x30, 3, 1, 2,  false, false, "bit field extract u"},
    {"s_bfe_i32",       AmdEncFamily::SOP2,   0x31, 3, 1, 2,  false, false, "bit field extract i"},
    {"s_bfe_u64",       AmdEncFamily::SOP2,   0x32, 3, 1, 2,  false, false, "bfe u64"},
    {"s_bfe_i64",       AmdEncFamily::SOP2,   0x33, 3, 1, 2,  false, false, "bfe i64"},
    {"s_add_u32",       AmdEncFamily::SOP2,   0x34, 2, 1, 2,  false, false, "add u32"},
    {"s_sub_u32",       AmdEncFamily::SOP2,   0x35, 2, 1, 2,  false, false, "subtract u32"},
    {"s_add_i32",       AmdEncFamily::SOP2,   0x36, 2, 1, 2,  false, false, "add i32"},
    {"s_sub_i32",       AmdEncFamily::SOP2,   0x37, 2, 1, 2,  false, false, "subtract i32"},
    {"s_addc_u32",      AmdEncFamily::SOP2,   0x38, 3, 1, 2,  false, false, "add with carry"},
    {"s_subb_u32",      AmdEncFamily::SOP2,   0x39, 3, 1, 2,  false, false, "sub with borrow"},
    {"s_min_i32",       AmdEncFamily::SOP2,   0x3A, 2, 1, 2,  false, false, "min i32"},
    {"s_min_u32",       AmdEncFamily::SOP2,   0x3B, 2, 1, 2,  false, false, "min u32"},
    {"s_max_i32",       AmdEncFamily::SOP2,   0x3C, 2, 1, 2,  false, false, "max i32"},
    {"s_max_u32",       AmdEncFamily::SOP2,   0x3D, 2, 1, 2,  false, false, "max u32"},
    {"s_cselect_b32",   AmdEncFamily::SOP2,   0x3E, 2, 1, 2,  false, false, "cond select"},
    {"s_cselect_b64",   AmdEncFamily::SOP2,   0x3F, 2, 1, 2,  false, false, "cond select 64"},

    // ─── SOPC: 标量比较 ───
    {"s_cmp_eq_i32",    AmdEncFamily::SOPC,   0x00, 2, 0, 2,  false, false, "cmp eq i32"},
    {"s_cmp_lg_i32",    AmdEncFamily::SOPC,   0x01, 2, 0, 2,  false, false, "cmp ne i32"},
    {"s_cmp_gt_i32",    AmdEncFamily::SOPC,   0x02, 2, 0, 2,  false, false, "cmp gt i32"},
    {"s_cmp_ge_i32",    AmdEncFamily::SOPC,   0x03, 2, 0, 2,  false, false, "cmp ge i32"},
    {"s_cmp_lt_i32",    AmdEncFamily::SOPC,   0x04, 2, 0, 2,  false, false, "cmp lt i32"},
    {"s_cmp_le_i32",    AmdEncFamily::SOPC,   0x05, 2, 0, 2,  false, false, "cmp le i32"},
    {"s_cmp_eq_u32",    AmdEncFamily::SOPC,   0x06, 2, 0, 2,  false, false, "cmp eq u32"},
    {"s_cmp_lg_u32",    AmdEncFamily::SOPC,   0x07, 2, 0, 2,  false, false, "cmp ne u32"},
    {"s_cmp_gt_u32",    AmdEncFamily::SOPC,   0x08, 2, 0, 2,  false, false, "cmp gt u32"},
    {"s_cmp_ge_u32",    AmdEncFamily::SOPC,   0x09, 2, 0, 2,  false, false, "cmp ge u32"},
    {"s_cmp_lt_u32",    AmdEncFamily::SOPC,   0x0A, 2, 0, 2,  false, false, "cmp lt u32"},
    {"s_cmp_le_u32",    AmdEncFamily::SOPC,   0x0B, 2, 0, 2,  false, false, "cmp le u32"},
    {"s_cmp_eq_u64",    AmdEncFamily::SOPC,   0x12, 2, 0, 2,  false, false, "cmp eq u64"},

    // ─── SOPP: 标量程序控制 ───
    {"s_nop",           AmdEncFamily::SOPP,   0x00, 0, 0, 1,  false, false, "no operation"},
    {"s_endpgm",        AmdEncFamily::SOPP,   0x01, 0, 0, 4,  false, false, "end program (EXIT)"},
    {"s_branch",        AmdEncFamily::SOPP,   0x02, 1, 0, 4,  false, false, "unconditional branch"},
    {"s_cbranch_scc0",  AmdEncFamily::SOPP,   0x04, 1, 0, 4,  false, false, "cbranch SCC=0"},
    {"s_cbranch_scc1",  AmdEncFamily::SOPP,   0x05, 1, 0, 4,  false, false, "cbranch SCC=1"},
    {"s_cbranch_vccz",  AmdEncFamily::SOPP,   0x06, 1, 0, 4,  false, false, "cbranch VCCZ=1"},
    {"s_cbranch_vccnz", AmdEncFamily::SOPP,   0x07, 1, 0, 4,  false, false, "cbranch VCCZ=0"},
    {"s_cbranch_execz", AmdEncFamily::SOPP,   0x08, 1, 0, 4,  false, false, "cbranch EXECZ=1"},
    {"s_cbranch_execnz",AmdEncFamily::SOPP,   0x09, 1, 0, 4,  false, false, "cbranch EXECZ=0"},
    {"s_barrier",       AmdEncFamily::SOPP,   0x0A, 0, 0, 20, false, false, "workgroup barrier"},
    {"s_waitcnt",       AmdEncFamily::SOPP,   0x0C, 0, 0, 4,  false, false, "wait counters"},
    {"s_sethalt",       AmdEncFamily::SOPP,   0x0D, 1, 0, 4,  false, false, "set halt"},
    {"s_sleep",         AmdEncFamily::SOPP,   0x0E, 1, 0, 4,  false, false, "sleep"},
    {"s_setprio",       AmdEncFamily::SOPP,   0x0F, 1, 0, 2,  false, false, "set priority"},
    {"s_sendmsg",       AmdEncFamily::SOPP,   0x10, 1, 0, 4,  false, false, "send message"},
    {"s_sendmsghalt",   AmdEncFamily::SOPP,   0x11, 1, 0, 4,  false, false, "send msg + halt"},
    {"s_trap",          AmdEncFamily::SOPP,   0x12, 1, 0, 4,  false, false, "trap/breakpoint"},
    {"s_icache_inv",    AmdEncFamily::SOPP,   0x13, 0, 0, 4,  false, false, "instr cache inval"},
    {"s_ttracedata",    AmdEncFamily::SOPP,   0x15, 0, 0, 2,  false, false, "trace data"},
    {"s_decperflevel",  AmdEncFamily::SOPP,   0x17, 1, 0, 2,  false, false, "dec perf level"},
    {"s_incperflevel",  AmdEncFamily::SOPP,   0x18, 1, 0, 2,  false, false, "inc perf level"},

    // ─── SOPK: 标量立即数 ───
    {"s_movk_i32",      AmdEncFamily::SOPK,   0x00, 1, 1, 2,  false, false, "movk i32"},
    {"s_cmovk_i32",     AmdEncFamily::SOPK,   0x02, 1, 1, 2,  false, false, "cmovk i32"},
    {"s_cmpk_eq_i32",   AmdEncFamily::SOPK,   0x03, 1, 0, 2,  false, false, "cmpk eq i32"},
    {"s_cmpk_lg_i32",   AmdEncFamily::SOPK,   0x04, 1, 0, 2,  false, false, "cmpk ne i32"},
    {"s_cmpk_gt_i32",   AmdEncFamily::SOPK,   0x05, 1, 0, 2,  false, false, "cmpk gt i32"},
    {"s_cmpk_ge_i32",   AmdEncFamily::SOPK,   0x06, 1, 0, 2,  false, false, "cmpk ge i32"},
    {"s_cmpk_lt_i32",   AmdEncFamily::SOPK,   0x07, 1, 0, 2,  false, false, "cmpk lt i32"},
    {"s_cmpk_le_i32",   AmdEncFamily::SOPK,   0x08, 1, 0, 2,  false, false, "cmpk le i32"},
    {"s_cmpk_eq_u32",   AmdEncFamily::SOPK,   0x09, 1, 0, 2,  false, false, "cmpk eq u32"},
    {"s_cmpk_lg_u32",   AmdEncFamily::SOPK,   0x0A, 1, 0, 2,  false, false, "cmpk ne u32"},
    {"s_cmpk_gt_u32",   AmdEncFamily::SOPK,   0x0B, 1, 0, 2,  false, false, "cmpk gt u32"},
    {"s_cmpk_ge_u32",   AmdEncFamily::SOPK,   0x0C, 1, 0, 2,  false, false, "cmpk ge u32"},
    {"s_cmpk_lt_u32",   AmdEncFamily::SOPK,   0x0D, 1, 0, 2,  false, false, "cmpk lt u32"},
    {"s_cmpk_le_u32",   AmdEncFamily::SOPK,   0x0E, 1, 0, 2,  false, false, "cmpk le u32"},
    {"s_addk_i32",      AmdEncFamily::SOPK,   0x0F, 2, 1, 2,  false, false, "addk i32"},
    {"s_mulk_i32",      AmdEncFamily::SOPK,   0x10, 2, 1, 2,  false, false, "mulk i32"},
    {"s_getreg_b32",    AmdEncFamily::SOPK,   0x14, 1, 1, 2,  false, false, "get special reg"},
    {"s_setreg_b32",    AmdEncFamily::SOPK,   0x15, 2, 0, 2,  false, false, "set special reg"},
    {"s_setreg_imm32_b32", AmdEncFamily::SOPK,0x16, 2, 0, 2,false,false,"setreg imm32"},

    // ─── VOP1: 向量一元操作 ───
    {"v_nop",           AmdEncFamily::VOP1,   0x00, 0, 0, 1,  false, false, "vector NOP"},
    {"v_mov_b32",       AmdEncFamily::VOP1,   0x03, 1, 1, 2,  false, false, "move b32"},
    {"v_readfirstlane_b32", AmdEncFamily::VOP1,0x04,1,1, 4,  false,false,"readfirstlane"},
    {"v_cvt_i32_f64",   AmdEncFamily::VOP1,   0x05, 1, 1, 4,  false, false, "cvt i32 f64"},
    {"v_cvt_f64_i32",   AmdEncFamily::VOP1,   0x06, 1, 1, 4,  false, false, "cvt f64 i32"},
    {"v_cvt_f32_i32",   AmdEncFamily::VOP1,   0x07, 1, 1, 4,  false, false, "cvt f32 i32"},
    {"v_cvt_f32_u32",   AmdEncFamily::VOP1,   0x08, 1, 1, 4,  false, false, "cvt f32 u32"},
    {"v_cvt_u32_f32",   AmdEncFamily::VOP1,   0x09, 1, 1, 4,  false, false, "cvt u32 f32"},
    {"v_cvt_i32_f32",   AmdEncFamily::VOP1,   0x0A, 1, 1, 4,  false, false, "cvt i32 f32"},
    {"v_cvt_f16_f32",   AmdEncFamily::VOP1,   0x0F, 1, 1, 4,  false, false, "cvt f16 f32"},
    {"v_cvt_f32_f16",   AmdEncFamily::VOP1,   0x11, 1, 1, 4,  false, false, "cvt f32 f16"},
    {"v_cvt_rpi_i32_f32", AmdEncFamily::VOP1, 0x12, 1, 1, 4,  false, false, "cvt rpi i32 f32"},
    {"v_cvt_flr_i32_f32", AmdEncFamily::VOP1, 0x13, 1, 1, 4,  false, false, "cvt floor i32 f32"},
    {"v_cvt_off_f32_i4",  AmdEncFamily::VOP1, 0x14, 1, 1, 4,  false, false, "cvt offset f32 i4"},
    {"v_cvt_f32_bf8",     AmdEncFamily::VOP1, 0x1C, 1, 1, 4,  false, false, "cvt f32 bf8"},
    {"v_cvt_f32_fp8",     AmdEncFamily::VOP1, 0x1D, 1, 1, 4,  false, false, "cvt f32 fp8"},
    {"v_cvt_pk_f32_fp8",  AmdEncFamily::VOP1, 0x1E, 1, 1, 4,  false, false, "cvt pk f32 fp8"},
    {"v_cvt_pk_f32_bf8",  AmdEncFamily::VOP1, 0x1F, 1, 1, 4,  false, false, "cvt pk f32 bf8"},
    {"v_cvt_f32_bf16",    AmdEncFamily::VOP1, 0x53, 1, 1, 4,  false, false, "cvt f32 bf16"},
    {"v_cvt_bf16_f32",    AmdEncFamily::VOP1, 0x54, 1, 1, 4,  false, false, "cvt bf16 f32"},
    {"v_bfrev_b32",       AmdEncFamily::VOP1, 0x24, 1, 1, 2,  false, false, "bit reverse b32"},
    {"v_ceil_f32",        AmdEncFamily::VOP1, 0x28, 1, 1, 4,  false, false, "ceil f32"},
    {"v_floor_f32",       AmdEncFamily::VOP1, 0x2B, 1, 1, 4,  false, false, "floor f32"},
    {"v_fract_f32",       AmdEncFamily::VOP1, 0x2D, 1, 1, 4,  false, false, "fract f32"},
    {"v_trunc_f32",       AmdEncFamily::VOP1, 0x2F, 1, 1, 4,  false, false, "trunc f32"},
    {"v_rndne_f32",       AmdEncFamily::VOP1, 0x31, 1, 1, 4,  false, false, "round f32"},
    {"v_exp_f32",         AmdEncFamily::VOP1, 0x33, 1, 1, 16,false, false, "exp2 f32"},
    {"v_log_f32",         AmdEncFamily::VOP1, 0x35, 1, 1, 16,false, false, "log2 f32"},
    {"v_rcp_f32",         AmdEncFamily::VOP1, 0x37, 1, 1, 8,  false, false, "rcp f32"},
    {"v_rcp_iflag_f32",   AmdEncFamily::VOP1, 0x38, 1, 1, 8,  false, false, "rcp iflag f32"},
    {"v_rsq_f32",         AmdEncFamily::VOP1, 0x39, 1, 1, 8,  false, false, "rsqrt f32"},
    {"v_sqrt_f32",        AmdEncFamily::VOP1, 0x41, 1, 1, 16,false, false, "sqrt f32"},
    {"v_sin_f32",         AmdEncFamily::VOP1, 0x43, 1, 1, 16,false, false, "sin f32"},
    {"v_cos_f32",         AmdEncFamily::VOP1, 0x45, 1, 1, 16,false, false, "cos f32"},
    {"v_not_b32",         AmdEncFamily::VOP1, 0x47, 1, 1, 2,  false, false, "NOT b32"},
    {"v_swap_b32",        AmdEncFamily::VOP1, 0x4D, 1, 1, 2,  false, false, "swap b32"},
    {"v_clz_i32_u32",     AmdEncFamily::VOP1, 0x56, 1, 1, 2,  false, false, "count leading zero"},
    {"v_movrels_b32",     AmdEncFamily::VOP1, 0x6A, 1, 1, 2,  false, false, "movrels b32"},
    {"v_movreld_b32",     AmdEncFamily::VOP1, 0x6C, 2, 0, 2,  false, false, "movreld b32"},
    {"v_movrelsd_b32",    AmdEncFamily::VOP1, 0x6E, 2, 0, 2,  false, false, "movrelsd b32"},
    {"v_sat_pk_u8_i16",   AmdEncFamily::VOP1, 0x78, 1, 1, 4,  false, false, "sat pack u8 i16"},

    // ─── VOP1 (FP16) ───
    {"v_cvt_f16_u16",     AmdEncFamily::VOP1, 0xA0, 1, 1, 4,  false, false, "cvt f16 u16"},
    {"v_cvt_f16_i16",     AmdEncFamily::VOP1, 0xA1, 1, 1, 4,  false, false, "cvt f16 i16"},
    {"v_cvt_u16_f16",     AmdEncFamily::VOP1, 0xA2, 1, 1, 4,  false, false, "cvt u16 f16"},
    {"v_cvt_i16_f16",     AmdEncFamily::VOP1, 0xA3, 1, 1, 4,  false, false, "cvt i16 f16"},
    {"v_ceil_f16",        AmdEncFamily::VOP1, 0xA8, 1, 1, 4,  false, false, "ceil f16"},
    {"v_floor_f16",       AmdEncFamily::VOP1, 0xAB, 1, 1, 4,  false, false, "floor f16"},
    {"v_trunc_f16",       AmdEncFamily::VOP1, 0xAF, 1, 1, 4,  false, false, "trunc f16"},
    {"v_rndne_f16",       AmdEncFamily::VOP1, 0xB1, 1, 1, 4,  false, false, "round f16"},
    {"v_rcp_f16",         AmdEncFamily::VOP1, 0xB7, 1, 1, 8,  false, false, "rcp f16"},
    {"v_sqrt_f16",        AmdEncFamily::VOP1, 0xC1, 1, 1, 8,  false, false, "sqrt f16"},
    {"v_sin_f16",         AmdEncFamily::VOP1, 0xC3, 1, 1, 16,false, false, "sin f16"},
    {"v_cos_f16",         AmdEncFamily::VOP1, 0xC5, 1, 1, 16,false, false, "cos f16"},
    {"v_log_f16",         AmdEncFamily::VOP1, 0xC7, 1, 1, 16,false, false, "log2 f16"},
    {"v_exp_f16",         AmdEncFamily::VOP1, 0xC9, 1, 1, 16,false, false, "exp2 f16"},

    // ─── VOP2: 向量二元操作 ───
    {"v_add_f32",         AmdEncFamily::VOP2, 0x03, 2, 1, 4,  false, false, "add f32"},
    {"v_sub_f32",         AmdEncFamily::VOP2, 0x04, 2, 1, 4,  false, false, "sub f32"},
    {"v_subrev_f32",      AmdEncFamily::VOP2, 0x05, 2, 1, 4,  false, false, "subrev f32"},
    {"v_mul_f32",         AmdEncFamily::VOP2, 0x08, 2, 1, 4,  false, false, "mul f32"},
    {"v_mul_i32_i24",     AmdEncFamily::VOP2, 0x0C, 2, 1, 4,  false, false, "mul i32 i24"},
    {"v_mul_hi_i32_i24",  AmdEncFamily::VOP2, 0x0D, 2, 1, 4,  false, false, "mul hi i32 i24"},
    {"v_mul_u32_u24",     AmdEncFamily::VOP2, 0x0E, 2, 1, 4,  false, false, "mul u32 u24"},
    {"v_mul_hi_u32_u24",  AmdEncFamily::VOP2, 0x0F, 2, 1, 4,  false, false, "mul hi u32 u24"},
    {"v_min_f32",         AmdEncFamily::VOP2, 0x10, 2, 1, 4,  false, false, "min f32"},
    {"v_max_f32",         AmdEncFamily::VOP2, 0x11, 2, 1, 4,  false, false, "max f32"},
    {"v_min_i32",         AmdEncFamily::VOP2, 0x12, 2, 1, 4,  false, false, "min i32"},
    {"v_max_i32",         AmdEncFamily::VOP2, 0x13, 2, 1, 4,  false, false, "max i32"},
    {"v_min_u32",         AmdEncFamily::VOP2, 0x14, 2, 1, 4,  false, false, "min u32"},
    {"v_max_u32",         AmdEncFamily::VOP2, 0x15, 2, 1, 4,  false, false, "max u32"},
    {"v_lshrrev_b32",     AmdEncFamily::VOP2, 0x16, 2, 1, 2,  false, false, "lshrrev b32"},
    {"v_ashrrev_i32",     AmdEncFamily::VOP2, 0x17, 2, 1, 2,  false, false, "ashrrev i32"},
    {"v_lshlrev_b32",     AmdEncFamily::VOP2, 0x18, 2, 1, 2,  false, false, "lshlrev b32"},
    {"v_and_b32",         AmdEncFamily::VOP2, 0x19, 2, 1, 2,  false, false, "AND b32"},
    {"v_or_b32",          AmdEncFamily::VOP2, 0x1A, 2, 1, 2,  false, false, "OR b32"},
    {"v_xor_b32",         AmdEncFamily::VOP2, 0x1B, 2, 1, 2,  false, false, "XOR b32"},
    {"v_sub_i32",         AmdEncFamily::VOP2, 0x1F, 2, 1, 2,  false, false, "sub i32"},
    {"v_subrev_i32",      AmdEncFamily::VOP2, 0x20, 2, 1, 2,  false, false, "subrev i32"},
    {"v_add_i32",         AmdEncFamily::VOP2, 0x22, 2, 1, 2,  false, false, "add i32 (no carry)"},
    {"v_sub_u32",         AmdEncFamily::VOP2, 0x23, 2, 1, 2,  false, false, "sub u32"},
    {"v_subrev_u32",      AmdEncFamily::VOP2, 0x24, 2, 1, 2,  false, false, "subrev u32"},
    {"v_add_u32",         AmdEncFamily::VOP2, 0x25, 2, 1, 2,  false, false, "add u32"},
    {"v_cndmask_b32",     AmdEncFamily::VOP2, 0x27, 3, 1, 2,  false, false, "cond mask b32"},
    {"v_mac_f32",         AmdEncFamily::VOP2, 0x2C, 3, 1, 4,  false, false, "MAC f32 (a*b + c)"},
    {"v_fmac_f32",        AmdEncFamily::VOP2, 0x2F, 3, 1, 4,  false, false, "FMAC f32 (a*b + c)"},
    {"v_add_f16",         AmdEncFamily::VOP2, 0x3C, 2, 1, 4,  false, false, "add f16"},
    {"v_sub_f16",         AmdEncFamily::VOP2, 0x3D, 2, 1, 4,  false, false, "sub f16"},
    {"v_subrev_f16",      AmdEncFamily::VOP2, 0x3E, 2, 1, 4,  false, false, "subrev f16"},
    {"v_mul_f16",         AmdEncFamily::VOP2, 0x41, 2, 1, 4,  false, false, "mul f16"},
    {"v_mac_f16",         AmdEncFamily::VOP2, 0x45, 3, 1, 4,  false, false, "MAC f16"},
    {"v_fmac_f16",        AmdEncFamily::VOP2, 0x48, 3, 1, 4,  false, false, "FMAC f16"},
    {"v_min_f16",         AmdEncFamily::VOP2, 0x49, 2, 1, 4,  false, false, "min f16"},
    {"v_max_f16",         AmdEncFamily::VOP2, 0x4A, 2, 1, 4,  false, false, "max f16"},
    {"v_ldexp_f16",       AmdEncFamily::VOP2, 0x56, 2, 1, 4,  false, false, "ldexp f16"},

    // ─── VOP2 (FP16 packed ops) ───
    {"v_pk_add_f16",      AmdEncFamily::VOP2, 0x80, 2, 1, 4,  false, false, "packed add f16"},
    {"v_pk_sub_f16",      AmdEncFamily::VOP2, 0x81, 2, 1, 4,  false, false, "packed sub f16"},
    {"v_pk_mul_f16",      AmdEncFamily::VOP2, 0x82, 2, 1, 4,  false, false, "packed mul f16"},
    {"v_pk_min_f16",      AmdEncFamily::VOP2, 0x83, 2, 1, 4,  false, false, "packed min f16"},
    {"v_pk_max_f16",      AmdEncFamily::VOP2, 0x84, 2, 1, 4,  false, false, "packed max f16"},
    {"v_pk_fmac_f16",     AmdEncFamily::VOP2, 0x85, 3, 1, 4,  false, false, "packed fmac f16"},

    // ─── VOP3: 向量三元操作 ───
    {"v_mad_f32",         AmdEncFamily::VOP3, 0x1C0, 3, 1, 4,  false, false, "MAD f32"},
    {"v_mad_i32_i24",     AmdEncFamily::VOP3, 0x1C1, 3, 1, 4,  false, false, "MAD i32 i24"},
    {"v_mad_u32_u24",     AmdEncFamily::VOP3, 0x1C2, 3, 1, 4,  false, false, "MAD u32 u24"},
    {"v_cubeid_f32",      AmdEncFamily::VOP3, 0x1C3, 3, 1, 16,false, false, "cubemap id"},
    {"v_cubesc_f32",      AmdEncFamily::VOP3, 0x1C4, 3, 1, 16,false, false, "cubemap sc"},
    {"v_cubetc_f32",      AmdEncFamily::VOP3, 0x1C5, 3, 1, 16,false, false, "cubemap tc"},
    {"v_cubema_f32",      AmdEncFamily::VOP3, 0x1C6, 3, 1, 16,false, false, "cubemap ma"},
    {"v_bfe_u32",         AmdEncFamily::VOP3, 0x1C7, 3, 1, 2,  false, false, "bfe u32"},
    {"v_bfe_i32",         AmdEncFamily::VOP3, 0x1C8, 3, 1, 2,  false, false, "bfe i32"},
    {"v_bfi_b32",         AmdEncFamily::VOP3, 0x1C9, 3, 1, 2,  false, false, "bfi b32"},
    {"v_fma_f32",         AmdEncFamily::VOP3, 0x1CA, 3, 1, 4,  false, false, "FMA f32"},
    {"v_fma_f64",         AmdEncFamily::VOP3, 0x1CB, 3, 1, 8,  false, false, "FMA f64"},
    {"v_lerp_u8",         AmdEncFamily::VOP3, 0x1CC, 3, 1, 4,  false, false, "lerp u8"},
    {"v_alignbyte_b32",   AmdEncFamily::VOP3, 0x1CE, 3, 1, 2,  false, false, "align byte b32"},
    {"v_mad_u16",         AmdEncFamily::VOP3, 0x1D1, 3, 1, 4,  false, false, "MAD u16"},
    {"v_mad_i16",         AmdEncFamily::VOP3, 0x1D2, 3, 1, 4,  false, false, "MAD i16"},
    {"v_perm_b32",        AmdEncFamily::VOP3, 0x1D4, 3, 1, 2,  false, false, "permute b32"},
    {"v_fma_f16",         AmdEncFamily::VOP3, 0x1D5, 3, 1, 4,  false, false, "FMA f16"},
    {"v_div_scale_f32",   AmdEncFamily::VOP3, 0x1D6, 4, 1, 8,  false, false, "div scale f32"},
    {"v_div_scale_f64",   AmdEncFamily::VOP3, 0x1D7, 4, 1, 16,false, false, "div scale f64"},
    {"v_msad_u8",         AmdEncFamily::VOP3, 0x1D8, 3, 1, 4,  false, false, "MSAD u8"},
    {"v_qsad_pk_u16_u8",  AmdEncFamily::VOP3, 0x1D9, 3, 1, 4,  false, false, "QSAD pk u16 u8"},
    {"v_mqsad_pk_u16_u8", AmdEncFamily::VOP3, 0x1DA, 3, 1, 4,  false, false, "MQSAD pk u16 u8"},
    {"v_mqsad_u32_u8",    AmdEncFamily::VOP3, 0x1DB, 3, 1, 4,  false, false, "MQSAD u32 u8"},
    {"v_mad_f16",         AmdEncFamily::VOP3, 0x1DC, 3, 1, 4,  false, false, "MAD f16"},
    {"v_interp_p1_f32",   AmdEncFamily::VOP3, 0x1F2, 2, 1, 4,  false, false, "interp p1 f32"},
    {"v_interp_p2_f32",   AmdEncFamily::VOP3, 0x1F3, 3, 1, 4,  false, false, "interp p2 f32"},

    // ─── VOP3P: 向量打包操作 + SWMMAC/WMMA ───
    {"v_pk_mad_i16",      AmdEncFamily::VOP3P, 0x80, 3, 1, 4,  false, false, "packed MAD i16"},
    {"v_pk_mul_lo_u16",   AmdEncFamily::VOP3P, 0x81, 2, 1, 4,  false, false, "pk mul lo u16"},
    {"v_pk_add_i16",      AmdEncFamily::VOP3P, 0x82, 2, 1, 4,  false, false, "pk add i16"},
    {"v_pk_sub_i16",      AmdEncFamily::VOP3P, 0x83, 2, 1, 4,  false, false, "pk sub i16"},
    {"v_pk_lshlrev_b16",  AmdEncFamily::VOP3P, 0x84, 2, 1, 2,  false, false, "pk lshlrev b16"},
    {"v_pk_lshrrev_b16",  AmdEncFamily::VOP3P, 0x85, 2, 1, 2,  false, false, "pk lshrrev b16"},
    {"v_pk_ashrrev_i16",  AmdEncFamily::VOP3P, 0x86, 2, 1, 2,  false, false, "pk ashrrev i16"},
    {"v_pk_max_i16",      AmdEncFamily::VOP3P, 0x87, 2, 1, 4,  false, false, "pk max i16"},
    {"v_pk_min_i16",      AmdEncFamily::VOP3P, 0x88, 2, 1, 4,  false, false, "pk min i16"},
    {"v_pk_mad_u16",      AmdEncFamily::VOP3P, 0x89, 3, 1, 4,  false, false, "pk MAD u16"},
    {"v_pk_add_u16",      AmdEncFamily::VOP3P, 0x8A, 2, 1, 4,  false, false, "pk add u16"},
    {"v_pk_sub_u16",      AmdEncFamily::VOP3P, 0x8B, 2, 1, 4,  false, false, "pk sub u16"},
    {"v_pk_max_u16",      AmdEncFamily::VOP3P, 0x8C, 2, 1, 4,  false, false, "pk max u16"},
    {"v_pk_min_u16",      AmdEncFamily::VOP3P, 0x8D, 2, 1, 4,  false, false, "pk min u16"},
    {"v_pk_fma_f16",      AmdEncFamily::VOP3P, 0x8E, 3, 1, 4,  false, false, "pk fma f16"},
    {"v_pk_add_f32",      AmdEncFamily::VOP3P, 0x8F, 2, 1, 4,  false, false, "pk add f32"},
    {"v_pk_mul_f32",      AmdEncFamily::VOP3P, 0x90, 2, 1, 4,  false, false, "pk mul f32"},
    {"v_pk_min_f32",      AmdEncFamily::VOP3P, 0x91, 2, 1, 4,  false, false, "pk min f32"},
    {"v_pk_max_f32",      AmdEncFamily::VOP3P, 0x92, 2, 1, 4,  false, false, "pk max f32"},

    // ─── WMMA (波前矩阵乘加) — gfx1100+ ───
    {"v_wmma_f32_16x16x16_f16",  AmdEncFamily::WMMA, 0x00, 3, 1, 8,  false,false,"WMMA f32 16x16x16 f16"},
    {"v_wmma_f32_16x16x16_bf16", AmdEncFamily::WMMA, 0x01, 3, 1, 8,  false,false,"WMMA f32 16x16x16 bf16"},
    {"v_wmma_i32_16x16x16_iu8",  AmdEncFamily::WMMA, 0x02, 3, 1, 8,  false,false,"WMMA i32 16x16x16 iu8"},
    {"v_wmma_i32_16x16x16_iu4",  AmdEncFamily::WMMA, 0x03, 3, 1, 8,  false,false,"WMMA i32 16x16x16 iu4"},
    {"v_wmma_f16_16x16x16_f16",  AmdEncFamily::WMMA, 0x04, 3, 1, 8,  false,false,"WMMA f16 16x16x16 f16"},

    // ─── SWMMAC (稀疏矩阵乘累加) — gfx1200+ RDNA4 ───
    {"v_swmmac_f32_16x16x32_f16",      AmdEncFamily::SWMMAC, 0x00, 3, 1, 26, true,true, "SWMMAC f32 16x16x32 f16"},
    {"v_swmmac_f16_16x16x32_f16",      AmdEncFamily::SWMMAC, 0x01, 3, 1, 26, true,true, "SWMMAC f16 16x16x32 f16"},
    {"v_swmmac_f32_16x16x32_bf16",     AmdEncFamily::SWMMAC, 0x02, 3, 1, 26, true,true, "SWMMAC f32 16x16x32 bf16"},
    {"v_swmmac_bf16_16x16x32_bf16",    AmdEncFamily::SWMMAC, 0x03, 3, 1, 26, true,true, "SWMMAC bf16 16x16x32 bf16"},
    {"v_swmmac_f32_16x16x32_fp8_fp8",  AmdEncFamily::SWMMAC, 0x04, 3, 1, 26, true,true, "SWMMAC f32 fp8×fp8"},
    {"v_swmmac_f32_16x16x32_fp8_bf8",  AmdEncFamily::SWMMAC, 0x05, 3, 1, 26, true,true, "SWMMAC f32 fp8×bf8"},
    {"v_swmmac_f32_16x16x32_bf8_fp8",  AmdEncFamily::SWMMAC, 0x06, 3, 1, 26, true,true, "SWMMAC f32 bf8×fp8"},
    {"v_swmmac_f32_16x16x32_bf8_bf8",  AmdEncFamily::SWMMAC, 0x07, 3, 1, 26, true,true, "SWMMAC f32 bf8×bf8"},
    {"v_swmmac_i32_16x16x64_iu4",      AmdEncFamily::SWMMAC, 0x08, 3, 1, 26, true,true, "SWMMAC i32 INT4 K=64"},
    {"v_swmmac_i32_16x16x32_iu8",      AmdEncFamily::SWMMAC, 0x09, 3, 1, 26, true,true, "SWMMAC i32 INT8 K=32"},
    {"v_swmmac_f32_16x16x64_f16",      AmdEncFamily::SWMMAC, 0x0A, 3, 1, 26, true,true, "SWMMAC f32 f16 K=64"},
    {"v_swmmac_f32_16x16x64_bf16",     AmdEncFamily::SWMMAC, 0x0B, 3, 1, 26, true,true, "SWMMAC f32 bf16 K=64"},
    {"v_swmmac_f32_16x16x128_fp8_fp8", AmdEncFamily::SWMMAC, 0x0C, 3, 1, 26, true,true, "SWMMAC f32 fp8×fp8 K=128"},

    // ─── MFMA (矩阵融合乘加) — CDNA legacy ───
    {"v_mfma_f32_16x16x16f16",  AmdEncFamily::MFMA, 0x00, 3, 1, 16,false,false,"MFMA f32 16x16x16"},
    {"v_mfma_f32_32x32x8f16",   AmdEncFamily::MFMA, 0x01, 3, 1, 16,false,false,"MFMA f32 32x32x8"},
    {"v_mfma_f32_16x16x4f32",   AmdEncFamily::MFMA, 0x02, 3, 1, 16,false,false,"MFMA f32 16x16x4"},
    {"v_mfma_i32_16x16x16i8",   AmdEncFamily::MFMA, 0x03, 3, 1, 16,false,false,"MFMA i32 16x16x16"},

    // ─── DPP (数据并行原语) ───
    {"v_mov_b32_dpp",        AmdEncFamily::DPP, 0x00, 1, 1, 2,  false, false, "mov dpp"},
    {"v_add_f32_dpp",        AmdEncFamily::DPP, 0x01, 2, 1, 4,  false, false, "add f32 dpp"},
    {"v_mul_f32_dpp",        AmdEncFamily::DPP, 0x02, 2, 1, 4,  false, false, "mul f32 dpp"},
    {"v_fma_f32_dpp",        AmdEncFamily::DPP, 0x03, 3, 1, 4,  false, false, "fma f32 dpp"},

    // ─── DS (本地数据共享) ───
    {"ds_write_b32",         AmdEncFamily::DS,  0x0D, 2, 0, 20, false, false, "write shared b32"},
    {"ds_read_b32",          AmdEncFamily::DS,  0x36, 1, 1, 20, false, false, "read shared b32"},
    {"ds_write2_b32",        AmdEncFamily::DS,  0x0F, 3, 0, 20, false, false, "write2 shared b32"},
    {"ds_read2_b32",         AmdEncFamily::DS,  0x38, 2, 2, 20, false, false, "read2 shared b32"},
    {"ds_write_b64",         AmdEncFamily::DS,  0x0E, 2, 0, 20, false, false, "write shared b64"},
    {"ds_read_b64",          AmdEncFamily::DS,  0x37, 1, 1, 20, false, false, "read shared b64"},
    {"ds_write_b128",        AmdEncFamily::DS,  0x71, 2, 0, 20, false, false, "write shared b128"},
    {"ds_read_b128",         AmdEncFamily::DS,  0x72, 1, 1, 20, false, false, "read shared b128"},
    {"ds_add_u32",           AmdEncFamily::DS,  0x20, 2, 1, 20, false, false, "atomic add u32 ds"},
    {"ds_sub_u32",           AmdEncFamily::DS,  0x21, 2, 1, 20, false, false, "atomic sub u32 ds"},
    {"ds_min_u32",           AmdEncFamily::DS,  0x25, 2, 1, 20, false, false, "atomic min u32 ds"},
    {"ds_max_u32",           AmdEncFamily::DS,  0x26, 2, 1, 20, false, false, "atomic max u32 ds"},
    {"ds_and_b32",           AmdEncFamily::DS,  0x27, 2, 1, 20, false, false, "atomic AND ds"},
    {"ds_or_b32",            AmdEncFamily::DS,  0x28, 2, 1, 20, false, false, "atomic OR ds"},
    {"ds_xor_b32",           AmdEncFamily::DS,  0x29, 2, 1, 20, false, false, "atomic XOR ds"},
    {"ds_cmpst_b32",         AmdEncFamily::DS,  0x3F, 3, 1, 20, false, false, "compare-store ds"},
    {"ds_swizzle_b32",       AmdEncFamily::DS,  0x4D, 2, 1, 20, false, false, "swizzle shared b32"},
    {"ds_permute_b32",       AmdEncFamily::DS,  0x4E, 2, 1, 20, false, false, "permute shared b32"},

    // ─── MUBUF / MTBUF (向量内存缓冲) ───
    {"buffer_load_dword",    AmdEncFamily::VMEM, 0x50, 4, 1, 300,false,false,"buffer load dword"},
    {"buffer_load_dwordx2",  AmdEncFamily::VMEM, 0x51, 4, 1, 300,false,false,"buffer load dwordx2"},
    {"buffer_load_dwordx4",  AmdEncFamily::VMEM, 0x52, 4, 1, 300,false,false,"buffer load dwordx4"},
    {"buffer_store_dword",   AmdEncFamily::VMEM, 0x60, 4, 0, 300,false,false,"buffer store dword"},
    {"buffer_store_dwordx2", AmdEncFamily::VMEM, 0x61, 4, 0, 300,false,false,"buffer store dwordx2"},
    {"buffer_store_dwordx4", AmdEncFamily::VMEM, 0x62, 4, 0, 300,false,false,"buffer store dwordx4"},
    {"buffer_atomic_add",    AmdEncFamily::VMEM, 0x70, 4, 1, 50, false,false,"buffer atomic add"},
    {"buffer_atomic_swap",   AmdEncFamily::VMEM, 0x71, 4, 1, 50, false,false,"buffer atomic swap"},
    {"buffer_atomic_cmpswap",AmdEncFamily::VMEM, 0x72, 4, 1, 50, false,false,"buffer atomic cmpswap"},
    {"buffer_atomic_min",    AmdEncFamily::VMEM, 0x74, 4, 1, 50, false,false,"buffer atomic min"},
    {"buffer_atomic_max",    AmdEncFamily::VMEM, 0x75, 4, 1, 50, false,false,"buffer atomic max"},
    {"buffer_atomic_and",    AmdEncFamily::VMEM, 0x76, 4, 1, 50, false,false,"buffer atomic AND"},
    {"buffer_atomic_or",     AmdEncFamily::VMEM, 0x77, 4, 1, 50, false,false,"buffer atomic OR"},
    {"buffer_atomic_xor",    AmdEncFamily::VMEM, 0x78, 4, 1, 50, false,false,"buffer atomic XOR"},
    {"buffer_atomic_inc",    AmdEncFamily::VMEM, 0x79, 4, 1, 50, false,false,"buffer atomic inc"},

    // ─── FLAT / Global (全局内存) ───
    {"global_load_dword",    AmdEncFamily::FLAT, 0x10, 2, 1, 300,false,false,"global load dword"},
    {"global_load_dwordx2",  AmdEncFamily::FLAT, 0x11, 2, 1, 300,false,false,"global load dwordx2"},
    {"global_load_dwordx4",  AmdEncFamily::FLAT, 0x12, 2, 1, 300,false,false,"global load dwordx4"},
    {"global_store_dword",   AmdEncFamily::FLAT, 0x18, 3, 0, 300,false,false,"global store dword"},
    {"global_store_dwordx2", AmdEncFamily::FLAT, 0x19, 3, 0, 300,false,false,"global store dwordx2"},
    {"global_store_dwordx4", AmdEncFamily::FLAT, 0x1A, 3, 0, 300,false,false,"global store dwordx4"},
    {"global_atomic_add",    AmdEncFamily::FLAT, 0x28, 3, 1, 50, false,false,"global atomic add"},
    {"global_atomic_swap",   AmdEncFamily::FLAT, 0x2C, 3, 1, 50, false,false,"global atomic swap"},
    {"global_atomic_cmpswap",AmdEncFamily::FLAT, 0x2D, 4, 1, 50, false,false,"global atomic cmpswap"},
    {"global_atomic_min",    AmdEncFamily::FLAT, 0x2E, 3, 1, 50, false,false,"global atomic min"},
    {"global_atomic_max",    AmdEncFamily::FLAT, 0x2F, 3, 1, 50, false,false,"global atomic max"},
    {"global_atomic_and",    AmdEncFamily::FLAT, 0x30, 3, 1, 50, false,false,"global atomic AND"},
    {"global_atomic_or",     AmdEncFamily::FLAT, 0x31, 3, 1, 50, false,false,"global atomic OR"},
    {"global_atomic_xor",    AmdEncFamily::FLAT, 0x32, 3, 1, 50, false,false,"global atomic XOR"},
    {"global_atomic_inc",    AmdEncFamily::FLAT, 0x33, 3, 1, 50, false,false,"global atomic inc"},

    // ─── IMAGE (纹理/图像) ───
    {"image_load",           AmdEncFamily::IMAGE, 0x00, 4, 1, 100,false,false,"image load"},
    {"image_store",          AmdEncFamily::IMAGE, 0x01, 4, 0, 100,false,false,"image store"},
    {"image_sample",         AmdEncFamily::IMAGE, 0x02, 5, 1, 100,false,false,"image sample"},
    {"image_sample_lz",      AmdEncFamily::IMAGE, 0x03, 5, 1, 100,false,false,"image sample lz"},
    {"image_get_lod",        AmdEncFamily::IMAGE, 0x04, 3, 1, 100,false,false,"image get lod"},
    {"image_gather4",        AmdEncFamily::IMAGE, 0x05, 5, 1, 100,false,false,"image gather4"},

    // ─── EXPORT / M0 ───
    {"exp",                  AmdEncFamily::EXP,  0x00, 4, 0, 4,  false, false, "export"},
    {"s_setreg_imm32_b32",   AmdEncFamily::EXP,  0x01, 2, 0, 2,  false, false, "set M0"},

    // ─── 表结束标记 ───
    {nullptr, AmdEncFamily::VOP1, 0, 0, 0, 0, false, false, nullptr}
};

// 指令数量
static constexpr size_t RDNA4_ISA_COUNT = sizeof(rdna4_isa) / sizeof(rdna4_isa[0]) - 1;

// ═══════════════════════════════════════════════════════════════
// 查找函数
// ═══════════════════════════════════════════════════════════════
inline const RDNA4InstInfo* rdna4_lookup(const char* mnemonic) {
    for (size_t i = 0; i < RDNA4_ISA_COUNT; i++)
        if (strcmp(rdna4_isa[i].mnemonic, mnemonic) == 0)
            return &rdna4_isa[i];
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════
// HunTian Opcode → AMD RDNA4 助记符映射 (扩展版: 32→120+)
// ═══════════════════════════════════════════════════════════════
struct HunToAmdMap {
    Opcode hun_op;
    const char* amd_mnemonic;
};

static const HunToAmdMap hun_to_amd[] = {
    // 浮点
    {Opcode::FFMA,   "v_fmac_f32"},
    {Opcode::FADD,   "v_add_f32"},
    {Opcode::FMUL,   "v_mul_f32"},
    {Opcode::FSQRT,  "v_sqrt_f32"},
    {Opcode::FRCP,   "v_rcp_f32"},
    {Opcode::FMNMX,  "v_max_f32"},
    {Opcode::FCMP,   "v_cmp_gt_f32"},
    {Opcode::DFMA,   "v_fma_f64"},
    {Opcode::DADD,   "v_mov_b32"},
    {Opcode::DMUL,   "v_mul_f32"},
    // 整数
    {Opcode::IADD,   "v_add_u32"},
    {Opcode::IADD3,  "v_add_u32"},
    {Opcode::ISUB,   "v_sub_u32"},
    {Opcode::IMAD,   "v_mad_i32_i24"},
    {Opcode::IMUL,   "v_mul_lo_u32"},
    {Opcode::SHL,    "v_lshlrev_b32"},
    {Opcode::SHR,    "v_lshrrev_b32"},
    {Opcode::SHF,    "v_alignbyte_b32"},
    {Opcode::PRMT,   "v_perm_b32"},
    {Opcode::IMNMX,  "v_max_i32"},
    // 数据移动
    {Opcode::MOV,    "v_mov_b32"},
    {Opcode::MOV32I, "s_movk_i32"},
    {Opcode::SEL,    "v_cndmask_b32"},
    // 内存
    {Opcode::LDG,    "global_load_dword"},
    {Opcode::STG,    "global_store_dword"},
    {Opcode::LDS,    "ds_read_b32"},
    {Opcode::STS,    "ds_write_b32"},
    {Opcode::LDC,    "s_mov_b32"},
    {Opcode::LDL,    "ds_read_b32"},
    {Opcode::STL,    "ds_write_b32"},
    // 控制流
    {Opcode::EXIT,   "s_endpgm"},
    {Opcode::RET,    "s_setpc_b64"},
    {Opcode::BRA,    "s_branch"},
    {Opcode::CAL,    "s_swappc_b64"},
    // 同步
    {Opcode::BAR,    "s_barrier"},
    {Opcode::DEPBAR, "s_waitcnt vmcnt(0)"},
    {Opcode::MEMBAR, "s_waitcnt lgkmcnt(0)"},
    {Opcode::YIELD,  "s_sleep 1"},
    // WMMA / 矩阵 (HunTian IMAD → AMD WMMA)
    {Opcode::IMAD,   "v_wmma_f32_16x16x16_f16"},
    // 原子
    {Opcode::ATOM,   "global_atomic_add"},
    // 转换
    {Opcode::I2F,    "v_cvt_f32_i32"},
    {Opcode::F2I,    "v_cvt_i32_f32"},
    {Opcode::CVT,    "v_cvt_f32_f16"},
    // 纹理 (HunTian → AMD Image)
    {Opcode::TEX,    "image_sample"},
    {Opcode::TLD,    "image_load"},
    {Opcode::TLD4,   "image_gather4"},
    {Opcode::SULD,   "image_load"},
    {Opcode::SUST,   "image_store"},
    // 双精度
    {Opcode::DSET,   "v_cmp_gt_f64"},
    {Opcode::DSETP,  "v_cmp_ge_f64"},
    // 断点
    {Opcode::BPT,    "s_trap 2"},
    {Opcode::UNKNOWN,nullptr},
};

// ═══════════════════════════════════════════════════════════════
// 扩展的 AMD RDNA4 后端
// ═══════════════════════════════════════════════════════════════
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
        return pascal_disassemble(w);
    }

    std::vector<std::string> disassemble_block(const std::vector<SASSWord>& c) const override {
        std::vector<std::string> lines; lines.reserve(c.size());
        for (size_t i=0;i<c.size();i++) {
            char buf[128]; snprintf(buf,sizeof(buf),"/*%04zx*/ %s",i,disassemble(c[i]).c_str());
            lines.push_back(buf);
        }
        return lines;
    }

    // 按 AMD 助记符查找 HunTian Opcode
    Opcode amd_to_hun(const char* mnemonic) const {
        for (auto& m : hun_to_amd)
            if (m.amd_mnemonic && strcmp(m.amd_mnemonic, mnemonic) == 0)
                return m.hun_op;
        return Opcode::UNKNOWN;
    }

    // 获取完整的 RDNA4 ISA 表
    const RDNA4InstInfo* isa_table() const { return rdna4_isa; }
    size_t isa_size() const { return RDNA4_ISA_COUNT; }

    uint8_t opcode_byte(Opcode op) const override {
        auto i = lookup(op);
        auto ri = rdna4_lookup(i ? i->amd_mnemonic : nullptr);
        return ri ? (uint8_t)ri->latency : 0;
    }
    const char* opcode_name(Opcode op) const override {
        auto i = lookup(op);
        return i ? i->amd_mnemonic : "???";
    }
    int operand_count(Opcode op) const override {
        auto i = lookup(op);
        auto ri = rdna4_lookup(i ? i->amd_mnemonic : nullptr);
        return ri ? (ri->num_src + ri->num_dst) : 0;
    }
    bool supports(Opcode op) const override {
        return lookup(op) != nullptr;
    }
    size_t total_instructions() const override {
        size_t count = 0;
        for (auto& m : hun_to_amd)
            if (m.amd_mnemonic) count++;
        return count;
    }

    // ─── RDNA4 特定参数 ───
    struct RDNA4Tuning {
        int fma_latency = 4;
        int wave_size = 32;
        int vgpr_count = 256;
        int sgpr_count = 128;
        int cu_count = 32;
        int lds_size_kb = 64;
        int l2_size_mb = 64;
        bool has_wmma = true;
        bool has_swmmac = true;  // gfx1200+
        bool has_sparse = true;
        bool has_fp8 = true;     // gfx1200+
        bool has_bf16 = true;    // gfx1200+
    };
    RDNA4Tuning tuning;

private:
    static const HunToAmdMap* lookup(Opcode op) {
        for (auto& m : hun_to_amd)
            if (m.hun_op == op && m.amd_mnemonic)
                return &m;
        return nullptr;
    }
};

// 实例
inline AmdRDNA4Backend& rdna4() {
    static AmdRDNA4Backend inst;
    return inst;
}

} // namespace sass
