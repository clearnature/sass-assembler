/* ============================================================================
 * 浑天三进制类型系统 - HunTian Ternary Type System
 * 
 * 基于高维流形视角：
 * - Trit 不是比特的扩展，是拓扑态的基本单元
 * - 1.58-bit 信息量：log₂(3) ≈ 1.585
 * - 三进制状态 {-1, 0, +1} 对应手性相位
 * ============================================================================ */

#ifndef HUNTIAN_TERNARY_TYPES_H
#define HUNTIAN_TERNARY_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════
 * 1. Trit（三进制位）基础定义
 * ══════════════════════════════════════════════════════════════════════ */

/* Trit 枚举：拓扑态的三种相位 */
typedef enum {
    TRIT_NEG  = -1,   /* 反手性态 / 阴态 / 负相位 */
    TRIT_ZERO =  0,   /* 中性态 / 零相位 / 拓扑平衡点 */
    TRIT_POS  =  1,   /* 正手性态 / 阳态 / 正相位 */
} Trit;

/* Trit 别名命名（高维视角） */
#define TRIT_YIN    TRIT_NEG    /* 阴态 */
#define TRIT_NEUTRAL TRIT_ZERO  /* 中性态 */
#define TRIT_YANG   TRIT_POS    /* 阳态 */

/* Trit 数值范围检查 */
#define TRIT_VALID(t) ((t) >= TRIT_NEG && (t) <= TRIT_POS)

/* Trit 信息量：1.58 bit */
#define TRIT_INFO_BITS 1.584962500721156  /* log₂(3) */

/* ══════════════════════════════════════════════════════════════════════
 * 2. Tryte（三进制字节）定义
 * ══════════════════════════════════════════════════════════════════════ */

/* Tryte：6个 Trit 组成的三进制字节
 * 
 * 高维视角：
 * - 不是"数据容器"，是"拓扑态组合"
 * - 6 Trit = 3⁶ = 729 种状态
 * - 信息量：6 × 1.585 = 9.51 bit（约等于1个二进制字节）
 */
#define TRYTE_TRITS 6
#define TRYTE_STATES 729  /* 3⁶ */

/* Tryte 结构：手性态组合 */
typedef struct {
    Trit trits[TRYTE_TRITS];
} Tryte;

/* Tryte 数值范围：-364 到 +364
 * 
 * 编码公式：
 * value = Σ(trit[i] × 3^i)，i = 0..5
 * 
 * 最大正值：+1×3⁰ + +1×3¹ + ... + +1×3⁵ = (3⁶-1)/2 = 364
 * 最大负值：对称 -364
 */
#define TRYTE_MAX_VALUE 364
#define TRYTE_MIN_VALUE -364

/* Tryte 转换函数 */

/* Tryte 转 整数（3进制权重展开） */
static inline int32_t tryte_to_int(Tryte t) {
    int32_t value = 0;
    int32_t power = 1;
    for (int i = 0; i < TRYTE_TRITS; i++) {
        value += t.trits[i] * power;
        power *= 3;
    }
    return value;
}

/* 整数 转 Tryte（3进制分解） */
static inline Tryte int_to_tryte(int32_t value) {
    Tryte result;
    int32_t remaining = value;
    
    /* 检查范围 */
    if (value > TRYTE_MAX_VALUE) remaining = TRYTE_MAX_VALUE;
    if (value < TRYTE_MIN_VALUE) remaining = TRYTE_MIN_VALUE;
    
    /* 平衡三进制转换 */
    for (int i = 0; i < TRYTE_TRITS; i++) {
        int32_t remainder = remaining % 3;
        remaining /= 3;
        
        /* 平衡三进制修正 */
        if (remainder == 2) {
            result.trits[i] = TRIT_NEG;
            remaining += 1;
        } else if (remainder == -2) {
            result.trits[i] = TRIT_POS;
            remaining -= 1;
        } else {
            result.trits[i] = (Trit)remainder;
        }
    }
    
    return result;
}

/* ══════════════════════════════════════════════════════════════════════
 * 3. Trint（三进制整数）定义 - 3-12-36 分层结构
 * ══════════════════════════════════════════════════════════════════════ */

/* 3-12-36 分层进制系统
 * 
 * 高维视角：
 * - 3: 手性层（{-1, 0, +1}）
 * - 12: 螺旋层（十二律相位）
 * - 36: 量子态层（三十六天罡）
 * 
 * Trint12: 12 Trit = 3¹² 种状态
 * Trint36: 36 Trit = 3³⁶ 种状态
 */

#define TRINT12_TRITS 12
#define TRINT36_TRITS 36

/* Trint12：12 Trit 三进制整数
 * 信息量：12 × 1.585 = 19.02 bit
 * 范围：-(3¹²-1)/2 到 +(3¹²-1)/2 ≈ -265720 到 +265720
 */
typedef struct {
    Trit trits[TRINT12_TRITS];
} Trint12;

#define TRINT12_MAX_VALUE 265720  /* (3¹²-1)/2 */

/* Trint36：36 Trit 三进制整数（对应量子态层）
 * 信息量：36 × 1.585 = 57.06 bit
 * 这是浑天系统的核心数据单元
 */
typedef struct {
    Trit trits[TRINT36_TRITS];
} Trint36;

/* ══════════════════════════════════════════════════════════════════════
 * 4. V-AVX3 512位向量（标准定义）
 * ══════════════════════════════════════════════════════════════════════ */

/* V-AVX3 512位向量
 * 
 * 高维视角：
 * - 512位 = 16 Tryte（不是字节）
 * - 每个 Tryte = 6 Trit
 * - 总 Trit 数 = 16 × 6 = 96 Trit
 * - 信息量 = 96 × 1.585 = 152.16 bit（约152位二进制等效）
 */
#define VAVX3_TRYTE_COUNT 16
#define VAVX3_TRIT_COUNT  96  /* 16 × 6 */

/* V-AVX3 向量结构 */
typedef struct {
    Tryte trytes[VAVX3_TRYTE_COUNT];  /* 16个Tryte */
} vavx3_vector_t;

/* 快速整数访问版本（兼容现有代码） */
typedef union {
    Trit    trits[VAVX3_TRIT_COUNT];   /* 96 Trit 直接访问 */
    Tryte   trytes[VAVX3_TRYTE_COUNT]; /* 16 Tryte 结构访问 */
    int32_t values[VAVX3_TRYTE_COUNT]; /* 16个数值（±364） */
    int64_t raw[8];                    /* 原始64位存储 */
} vavx3_512_t;

/* ══════════════════════════════════════════════════════════════════════
 * 5. 三进制分层进制转换
 * ══════════════════════════════════════════════════════════════════════ */

/* 3进制到12进制转换
 * 
 * 高维视角：
 * - 不是简单的数值转换
 * - 是螺旋层的相位编码
 * - 12进制对应十二律
 */
typedef struct {
    int32_t spiral_phase;    /* 螺旋相位 0-11 */
    Trit   chirality;        /* 手性修正 */
} Spiral12;

/* Trit 序列 转 Spiral12 */
static inline Spiral12 trits_to_spiral12(Trit* trits, int count) {
    Spiral12 result;
    
    /* 计算螺旋相位：模12 */
    int32_t phase = 0;
    int32_t power = 1;
    for (int i = 0; i < count && i < 4; i++) {  /* 用前4 Trit */
        phase += trits[i] * power;
        power *= 3;
    }
    result.spiral_phase = phase % 12;
    
    /* 手性修正：最后一个 Trit */
    result.chirality = (count > 0) ? trits[count - 1] : TRIT_ZERO;
    
    return result;
}

/* 12进制到36进制转换
 * 
 * 高维视角：
 * - 36进制对应量子态层（三十六天罡）
 * - 3个 Spiral12 组成 1个 Quantum36
 */
typedef struct {
    Spiral12 spirals[3];     /* 3个螺旋相位 */
    int32_t quantum_state;   /* 量子态索引 0-35 */
} Quantum36;

/* Trit 序列 转 Quantum36 */
static inline Quantum36 trits_to_quantum36(Trit* trits, int count) {
    Quantum36 result;
    
    /* 分成3组，每组4 Trit */
    for (int g = 0; g < 3; g++) {
        Trit group[4] = {TRIT_ZERO, TRIT_ZERO, TRIT_ZERO, TRIT_ZERO};
        for (int i = 0; i < 4 && (g*4 + i) < count; i++) {
            group[i] = trits[g * 4 + i];
        }
        result.spirals[g] = trits_to_spiral12(group, 4);
    }
    
    /* 计算量子态索引 */
    result.quantum_state = (result.spirals[0].spiral_phase * 12 + 
                            result.spirals[1].spiral_phase) % 36;
    
    return result;
}

/* ══════════════════════════════════════════════════════════════════════
 * 6. 三进制编码/解码宏
 * ══════════════════════════════════════════════════════════════════════ */

/* Trit 字符编码 */
#define TRIT_CHAR(t) (((t) == TRIT_POS) ? '+' : ((t) == TRIT_NEG) ? '-' : '0')

/* Trit 字符解码 */
#define CHAR_TO_TRIT(c) (((c) == '+') ? TRIT_POS : ((c) == '-') ? TRIT_NEG : TRIT_ZERO)

/* Trit 二进制编码（2位编码）
 * 
 * 编码规则：
 * TRIT_NEG (-1) → 00
 * TRIT_ZERO (0) → 01
 * TRIT_POS (+1) → 10
 * 保留：11（溢出/错误）
 */
#define TRIT_TO_BINARY(t) (((t) == TRIT_POS) ? 0b10 : ((t) == TRIT_NEG) ? 0b00 : 0b01)

/* 二进制转 Trit */
static inline Trit binary_to_trit(uint8_t b) {
    switch (b & 0b11) {
        case 0b00: return TRIT_NEG;
        case 0b01: return TRIT_ZERO;
        case 0b10: return TRIT_POS;
        default:   return TRIT_ZERO;  /* 错误态归零 */
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 7. 常用常数
 * ══════════════════════════════════════════════════════════════════════ */

/* 黄金分割 */
#define PHI_GOLDEN 1.618034

/* 相干因子 */
#define COHERENCE_FACTOR 0.397

/* 陈数 */
#define CHERN_NUMBER 2

/* 熵旋耦合常数 */
#define KAPPA_ENTROPY 0.85

#endif /* HUNTIAN_TERNARY_TYPES_H */