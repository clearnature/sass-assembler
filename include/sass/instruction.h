/* ============================================================================
 * HunTian SASS 汇编器 — 指令 AST
 * ============================================================================ */

#ifndef SASS_INSTRUCTION_H
#define SASS_INSTRUCTION_H

#include <cstdint>
#include <vector>
#include <string>
#include <span>

namespace sass {

// ─── 三进制类型 (内联定义，避免 third_party 冲突) ───
enum Trit : int8_t { TRIT_NEG = -1, TRIT_ZERO = 0, TRIT_POS = 1 };

enum class OpType : uint8_t { REG, VAVX3_REG, IMM, MEM, TRIT_REG, LABEL };

struct Operand {
    OpType type;
    union {
        uint8_t  reg_id;
        uint8_t  vavx3_id;
        int64_t  imm_val;
        Trit     trit_id;
        struct { uint8_t base; int16_t offset; } mem;
    };
    std::string label_name;
};

enum class Opcode : uint16_t {
    // ─── 整数算术 (0x01-0x0F) ───
    IADD    = 0x01,
    IADD3   = 0x02,
    IADD_X  = 0x03,  // 带进位加法
    ISCADD  = 0x04,  // 缩放加法
    ISUB    = 0x05,
    IMAD    = 0x06,
    IMUL    = 0x07,
    IMNMX   = 0x08,  // 整数 min/max
    SHL     = 0x09,
    SHR     = 0x0A,
    BFE     = 0x0B,  // 位域提取
    BFI     = 0x0C,  // 位域插入
    LOP3    = 0x0D,  // 三操作数逻辑
    LOP32I  = 0x0E,  // 立即数逻辑
    BREV    = 0x0F,  // 位反转

    // ─── 浮点算术 (0x10-0x16) ───
    FADD    = 0x10,
    FMUL    = 0x11,
    FSET    = 0x12,  // 浮点比较设置
    FMNMX   = 0x13,  // 浮点 min/max
    FSEL    = 0x14,
    FRCP    = 0x15,  // 倒数
    FSQRT   = 0x16,

    // ─── 内存操作 (0x17-0x1B) ───
    LDG     = 0x17,  // 全局加载
    LDS     = 0x18,  // 共享加载
    STG     = 0x19,  // 全局存储
    STS     = 0x1A,  // 共享存储
    LDC     = 0x1B,  // 常量加载

    // ─── 控制流 (0x1C-0x22) ───
    BRA     = 0x1C,
    BRX     = 0x1D,  // 间接分支
    CAL     = 0x1E,  // 调用
    RET     = 0x1F,  // 返回
    EXIT    = 0x20,
    KILL    = 0x21,  // 杀死线程
    YIELD   = 0x22,

    // ─── 同步/屏障 (0x23-0x25) ───
    BAR     = 0x23,
    DEPBAR  = 0x24,  // 依赖屏障
    MEMBAR  = 0x25,  // 内存屏障

    // ─── 数据移动 (0x26-0x29) ───
    MOV     = 0x26,
    MOV32I  = 0x27,
    SEL     = 0x28,
    S2R     = 0x29,  // 特殊→通用寄存器

    // ─── 类型转换 ───
    CVT     = 0x2A,

    // ─── 比较/谓词 (0x2B-0x2E) ───
    ISET    = 0x2B,
    ISETP   = 0x2C,
    FSETP   = 0x2D,
    SETP    = 0x2E,

    // ─── 双精度浮点 (0x2F-0x34) ───
    DADD    = 0x2F,
    DMUL    = 0x30,
    DFMA    = 0x31,
    DMNMX   = 0x32,
    DSET    = 0x33,
    DSETP   = 0x34,

    // ─── 浮点杂项 (0x35-0x39) ───
    FCMP    = 0x35,
    FSWZ    = 0x36,
    FCHK    = 0x37,
    RRO     = 0x38,
    MUFU    = 0x39,

    // ─── 整数杂项 (0x3A-0x3F) ───
    ISAD    = 0x3A,
    FLO     = 0x3B,
    ICMP    = 0x3C,
    SHF     = 0x3D,
    POPC    = 0x3E,
    VABSDIFF = 0x3F,

    // ─── 转换 (0x40-0x44) ───
    F2F     = 0x40,
    F2I     = 0x41,
    I2F     = 0x42,
    I2I     = 0x43,
    FRND    = 0x44,

    // ─── 移动 (0x45-0x46) ───
    PRMT    = 0x45,
    SHFL    = 0x46,

    // ─── 谓词 (0x47-0x4A) ───
    PLOP3   = 0x47,
    P2R     = 0x48,
    R2P     = 0x49,
    CSET    = 0x4A,
    CSETP   = 0x4B,

    // ─── 加载/存储扩展 (0x4C-0x4D) ───
    LDL     = 0x4C,
    STL     = 0x4D,

    // ─── XMAD 族 (0x4E-0x5B) ───
    XMAD       = 0x4E,
    XMAD_MRG   = 0x4F,
    XMAD_CHI   = 0x50,
    FFMA       = 0x59,
    XMAD_PSL   = 0x5B,

    // ─── 原子操作 (0x5C-0x5D) ───
    ATOM    = 0x5C,
    RED     = 0x5D,

    // ─── 控制流扩展 (0x5E-0x67) ───
    JMP     = 0x5E,
    JCAL    = 0x5F,
    BRK     = 0x60,
    CONT    = 0x61,
    SSY     = 0x62,
    PBK     = 0x63,
    PCNT    = 0x64,
    PRET    = 0x65,
    BPT     = 0x66,
    LONGJMP = 0x67,

    // ─── 杂项 (0x68-0x69) ───
    B2R     = 0x68,
    LEPC    = 0x69,

    // ─── 纹理/表面 (0x6A-0x71) ───
    TEX     = 0x6A,
    TLD     = 0x6B,
    TLD4    = 0x6C,
    TXQ     = 0x6D,
    SULD    = 0x6E,
    SULEA   = 0x6F,
    SUST    = 0x70,
    SURED   = 0x71,
    SUQ     = 0x72,  // Surface Query

    // ─── Hopper+ Tensor Core (SM90+) ───
    WGMMA_ARRIVE   = 0x80,  // wgmma.fence.sync.aligned
    WGMMA_COMMIT   = 0x81,  // wgmma.commit_group.sync.aligned
    WGMMA_WAIT     = 0x82,  // wgmma.wait_group.sync.aligned N
    TMA_LOAD       = 0x83,  // cp.async.bulk.tensor (TMA load)
    TMA_STORE      = 0x84,  // cp.async.bulk.tensor (TMA store)
    MBARRIER_ARRIVE= 0x85,  // mbarrier.arrive.shared::cta.b64
    TENSORMAP_REPLACE=0x86, // tensormap.replace.tile
    FENCE_TENSORMAP= 0x87,  // fence.proxy.tensormap

    // ─── Fermi遗留 (0x73-0x79, UNVERIFIED) ───
    LD      = 0x73,  // Generic Load (Pascal: LDG替代)
    ST      = 0x74,  // Generic Store (Pascal: STG替代)
    JMX     = 0x75,  // Indirect Jump (Pascal: BRX替代)
    I2IP    = 0x76,  // Integer to Integer Pack
    SGXT    = 0x77,  // Sign Extend
    CCTL    = 0x78,  // Cache Control
    CCTLL   = 0x79,  // Cache Control Local


    // ─── AMD RDNA4 矩阵指令 (0x88-0x9F) ───
    SWMMAC_F32_F16     = 0x88,  // v_swmmac_f32_16x16x32_f16
    SWMMAC_F32_BF16    = 0x89,  // v_swmmac_f32_16x16x32_bf16
    SWMMAC_F32_FP8     = 0x8A,  // v_swmmac_f32_16x16x32_fp8
    SWMMAC_I32_IU4     = 0x8B,  // v_swmmac_i32_16x16x64_iu4
    SWMMAC_I32_IU8     = 0x8C,  // v_swmmac_i32_16x16x32_iu8
    SWMMAC_F16_F16     = 0x8D,  // v_swmmac_f16_16x16x32_f16
    SWMMAC_BF16_BF16   = 0x8E,  // v_swmmac_bf16_16x16x32_bf16
    WMMA_F32_F16       = 0x8F,  // v_wmma_f32_16x16x16_f16
    WMMA_F32_BF16      = 0x90,  // v_wmma_f32_16x16x16_bf16
    WMMA_F16_F16       = 0x91,  // v_wmma_f16_16x16x16_f16
    WMMA_BF16_BF16     = 0x92,  // v_wmma_bf16_16x16x16_bf16
    WMMA_I32_IU8       = 0x93,  // v_wmma_i32_16x16x16_iu8
    WMMA_I32_IU4       = 0x94,  // v_wmma_i32_16x16x16_iu4
    WMMA_F32_FP8_FP8   = 0x95,  // v_wmma_f32_16x16x16_fp8_fp8
    
    // ─── AMD DPP/数据并行指令 (0x96-0x9F) ───
    V_DPP_ADD          = 0x96,  // v_add_f32_dpp
    V_DPP_MUL          = 0x97,  // v_mul_f32_dpp
    V_DPP_FMA          = 0x98,  // v_fmac_f32_dpp
    V_PERMLANE16       = 0x99,  // v_permlane16_b32
    V_PERMLANEX16      = 0x9A,  // v_permlanex16_b32
    IMAGE_SAMPLE       = 0x9B,  // image_sample
    IMAGE_LOAD         = 0x9C,  // image_load
    IMAGE_STORE        = 0x9D,  // image_store
    EXPORT_MRTZ        = 0x9E,  // exp mrtz
    INTERP_P1_P2       = 0x9F,  // v_interp_p1_f32 / v_interp_p2_f32
    
    // ─── AMD VALU 高级指令 (0xA0-0xAF) ───
    V_BFE_U32          = 0xA0,  // v_bfe_u32
    V_BFI_U32          = 0xA1,  // v_bfi_u32
    V_PERM_B32         = 0xA2,  // v_perm_b32
    V_SWAP_B32         = 0xA3,  // v_swap_b32
    V_PACK_LL          = 0xA4,  // v_pack_ll_b32
    V_PACK_LH          = 0xA5,  // v_pack_lh_b32
    V_ALIGNBIT         = 0xA6,  // v_alignbit_b32
    V_ALIGNBYTE        = 0xA7,  // v_alignbyte_b32
    V_SAD_U32          = 0xA8,  // v_sad_u32
    V_QSAD_U32         = 0xA9,  // v_qsad_u32
    V_MQSAD_U32        = 0xAA,  // v_mqsad_u32
    V_DOT4_I32_I8      = 0xAB,  // v_dot4_i32_i8
    V_DOT8_I32_I4      = 0xAC,  // v_dot8_i32_i4
    BITOP3_B32         = 0xAD,  // v_bitop3_b32
    
    // ─── AMD 标量/控制/内存指令 (0xB0-0xBF) ───
    S_GETPC_B64        = 0xB0,  // s_getpc_b64
    S_SETPC_B64        = 0xB1,  // s_setpc_b64
    S_CBRANCH          = 0xB2,  // s_cbranch
    S_SLEEP            = 0xB3,  // s_sleep
    S_SETPRIO          = 0xB4,  // s_setprio
    S_SENDMSG          = 0xB5,  // s_sendmsg
    DS_PERMUTE_B32     = 0xB6,  // ds_permute_b32
    DS_SWIZZLE_B32     = 0xB7,  // ds_swizzle_b32
    DS_BPERMUTE_B32    = 0xB8,  // ds_bpermute_b32
    V_CMPSX_F32        = 0xB9,  // v_cmpx_*_f32
    V_CNDMASK_B32      = 0xBA,  // v_cndmask_b32
    V_READLANE_B32     = 0xBB,  // v_readlane_b32
    V_WRITELANE_B32    = 0xBC,  // v_writelane_b32
    // ─── HunTian VAVX3 扩展 (0xF0-0xF7) ───
    VAVX3_ADD_512   = 0xF0,
    VAVX3_MUL_512   = 0xF1,
    VAVX3_MAD_512   = 0xF2,
    VAVX3_MMA_512   = 0xF3,
    VAVX3_GEOM      = 0xF4,
    VAVX3_SHUFFLE   = 0xF5,
    VAVX3_BRAID     = 0xF6,
    VAVX3_LAPLACIAN = 0xF7,

    // ─── HunTian 三进制扩展 (0xF8-0xFB) ───
    TMAD    = 0xF8,
    TMUL    = 0xF9,
    TCONV   = 0xFA,
    TRYTE_OP = 0xFB,

    // 未知
    UNKNOWN = 0xFFFF
};

struct Instruction {
    Opcode opcode;
    std::vector<Operand> operands;
    uint32_t scheduled_cycle = 0;
    double   complexity_score = 1.0;
    bool     is_fusable = false;
    bool     reuse[3] = {false, false, false};  // 每操作数 .reuse 标志
    
    static Instruction CreateRegOp(Opcode op, uint8_t dst, uint8_t src1, uint8_t src2) {
        Instruction inst;
        inst.opcode = op;
        inst.operands = {
            {OpType::REG, {.reg_id = dst}, ""},
            {OpType::REG, {.reg_id = src1}, ""},
            {OpType::REG, {.reg_id = src2}, ""}
        };
        return inst;
    }

    static Instruction CreateVAVX3_MMA(uint8_t dst, uint8_t a, uint8_t b, uint8_t c) {
        Instruction inst;
        inst.opcode = Opcode::VAVX3_MMA_512;
        inst.operands = {
            {OpType::VAVX3_REG, {.vavx3_id = dst}, ""},
            {OpType::VAVX3_REG, {.vavx3_id = a}, ""},
            {OpType::VAVX3_REG, {.vavx3_id = b}, ""},
            {OpType::VAVX3_REG, {.vavx3_id = c}, ""}
        };
        inst.complexity_score = 10.0;
        inst.is_fusable = true;
        return inst;
    }
    
    static Instruction CreateTernary_MAD(Trit dst, Trit a, Trit b, Trit acc) {
        Instruction inst;
        inst.opcode = Opcode::TMAD;
        inst.operands = {
            {OpType::TRIT_REG, {.trit_id = dst}, ""},
            {OpType::TRIT_REG, {.trit_id = a}, ""},
            {OpType::TRIT_REG, {.trit_id = b}, ""},
            {OpType::TRIT_REG, {.trit_id = acc}, ""}
        };
        return inst;
    }
};

using InstructionBlock = std::span<const Instruction>;

} // namespace sass

#endif // SASS_INSTRUCTION_H
