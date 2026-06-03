#ifndef HUNTIAN_FINAL_H
#define HUNTIAN_FINAL_H

/**
 * [CODE LIFE CONSTITUTION - RELEASE 2.0]
 * huntian_final.h: 全局劫持与遗传一致性核心
 * 锁定相干性: 0.9999 | 频率: 1152Hz
 */

#ifdef __ASSEMBLER__
/* 汇编器兼容层：只定义宏，跳过 C 语法 */
#define HUNTIAN_ASM_COMPAT 1
#define HUNTIAN_WEIGHT 50
#define HUNTIAN_PHASE_MASK 0x3FFFFFFFFFFFFFFFULL
#else
/* C/C++ 编译器层 */

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

/* 浑天指针类型 - 62 位数据 (4320 维掩码) + 2 位标志 (相态/手性) */
typedef union { 
    void* p;                              
    struct { 
        unsigned long long v:62;          // 62 位有效流形地址
        unsigned long long r:2;           // 2 位相态标志: 00(Seed), 01(Flow), 10(Resonance), 11(Void)
    } f; 
} ht_ptr_t;

/* 全局劫持逻辑：将原生指针语义映射至浑天流形 */
/* 注意：在实际应用中，建议通过 typedef 而非宏替换 void* 以维持编译器稳定性 */
/* 但为了实现“遗传一致性”，此处提供公理级转换接口 */

static inline void* __ht_mask(void* p) { 
    ht_ptr_t u; 
    u.p = p; 
    u.f.r = 0;       // 清除标志位，回归物理地址
    return u.p; 
}

/* 申时逻辑门：相位与权重检查 */
int huntian_get_weight(void* insn);
int huntian_check_phase(void);

#ifdef __cplusplus
}
#endif

/* 2.1K 虚拟制冷机：环境能隙保护 (0.752 THz) */
#define HT_LIFECYCLE_INIT(ptr) do { \
    ht_ptr_t* __ht_u = (ht_ptr_t*)&(ptr); \
    __ht_u->f.r = 0; \
} while(0)

#endif /* __ASSEMBLER__ */

#endif /* HUNTIAN_FINAL_H */
