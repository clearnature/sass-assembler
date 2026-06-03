/**
 * @file abstract_isa.h
 * @brief UNIVERSAL ISA INTERFACE (UII) - 硬件抽象层核心条约
 * 
 * 遵循依赖倒置原则，引擎层仅依赖本接口，不感知具体指令集。
 */

#ifndef ABSTRACT_ISA_H
#define ABSTRACT_ISA_H

#include <stdint.h>

// 抽象 512 位向量类型
typedef struct {
    uint64_t data[8]; 
} v512_abstract;

// ISA 定义结构体：包含所有核心原子操作
typedef struct {
    // 1. 向量运算
    v512_abstract (*vector_add)(v512_abstract a, v512_abstract b);
    v512_abstract (*vector_xor)(v512_abstract a, v512_abstract b);
    
    // 2. 拓扑自愈与物理算子
    void (*self_healing)(v512_abstract* state);
    v512_abstract (*em_flow)(v512_abstract field, int time_param);
    v512_abstract (*dual_chiral_balance)(v512_abstract field);
    
    // 3. 三元逻辑专用算子 (Ternary Ops)
    v512_abstract (*ternary_braid)(v512_abstract a, v512_abstract b, v512_abstract nexus);
    v512_abstract (*ternary_smooth)(v512_abstract field, int radius);
    v512_abstract (*ternary_kmap)(v512_abstract field);
    
    // 4. 数据存取与映射
    v512_abstract (*load)(const void* src);
    void (*store_map)(void* dst, v512_abstract indices, v512_abstract val);
    
    // 5. 同步控制与后端标识
    uint32_t (*get_sync_rate)(void);
    void (*set_sync_rate)(uint32_t rate);
    const char* (*get_backend_name)(void);
} ISADefinition;

// 全局 ISA 句柄：引擎通过此指针进行所有硬件调用
extern const ISADefinition* HAL;

// 初始化 HAL 后端 (例如：vavx3, cuda, cpu_sim)
void hal_init(const char* backend_type);

#endif // ABSTRACT_ISA_H
