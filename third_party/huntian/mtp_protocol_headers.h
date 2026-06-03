#ifndef MTP_PROTOCOL_HEADERS_H
#define MTP_PROTOCOL_HEADERS_H

/**
 * MTP (Manifold Topology Protocol) Headers
 * 浑天拓扑协议头定义
 */

#include <stdint.h>

// MTP Magic Number
#define MTP_MAGIC 0x48544D50  // "HTMP" - HunTian Manifold Protocol

// MTP Payload Types
typedef enum {
    MTP_TYPE_HOLO_SYNC = 0,       // 全息态同步
    MTP_TYPE_TOPO_INJECT = 1,     // 拓扑知识注入
    MTP_TYPE_PGO_EVOLVE = 2,      // PGO演化
    MTP_TYPE_QU_YUZHI_MASS = 3    // 屈欲之质量生成
} MTPPayloadType;

// 黄钟基准频率 (十二平均律)
#define LV_HUANG_ZHONG 261.6255653  // C4 = Middle C

// MTP Header Structure
typedef struct {
    uint32_t magic;                    // 协议幻数
    uint8_t version;                   // 协议版本
    uint8_t resonance_status;          // 共振状态
    uint16_t reserved;                 // 保留字段
    uint64_t ground_truth_hash;        // 拓扑指纹
    uint64_t resonance_timestamp;      // 共振时间戳
    float harmonic_coherence;          // 谐波相干性
} MTPHeader;

#endif // MTP_PROTOCOL_HEADERS_H
