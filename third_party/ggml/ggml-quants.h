// phase3/ggml/ggml-quants.h — 三进制量化头文件
// 参考: llama.cpp ggml-quants.h + ggml-common.h
// 格式:
//   TQ1_0: 标准三进制 (1.6875 bpw), 同 llama.cpp block_tq1_0
//   TQT_0: 环面三进制 (Krystal Spiral + SovereignBlock), 同 BitNet block_tqt_0
#ifndef PHASE3_GGML_QUANTS_H
#define PHASE3_GGML_QUANTS_H

#include <stdint.h>
#include <stddef.h>

#define QK_K 256

// ══════════════════════════════════════════════════════════════
// TQ1_0 — 标准三进制 (同 llama.cpp block_tq1_0)
// 1.6875 bpw: 5 trit/byte (3^5=243<256) + 4 trit/byte
// ══════════════════════════════════════════════════════════════
typedef struct {
    uint8_t qs[(QK_K - 4 * QK_K / 64) / 5]; // 48B: 5 trit/byte
    uint8_t qh[QK_K / 64];                   //  4B: 4 trit/byte
    uint16_t d;                               //  2B: fp16 scale
} block_tq1_0;  // 共 54 字节

// ══════════════════════════════════════════════════════════════
// TQT_0 — 环面三进制 (Krystal Spiral + SovereignBlock)
// 同 BitNet block_tqt_0, +10B RDNA4对齐填充
// ══════════════════════════════════════════════════════════════
typedef struct {
    uint8_t qs[48];          // 240 trit: 5 trit/byte × 48
    uint8_t qh[4];           //  16 trit: 4 trit/byte × 4
    uint16_t d;              // fp16 scale
    uint8_t padding[10];     // RDNA4 64字节对齐
} block_tqt_0;  // 共 64 字节

// ══════════════════════════════════════════════════════════════
// 函数声明
// ══════════════════════════════════════════════════════════════

// TQ1_0: 标准三进制 (同 llama.cpp)
void quantize_row_tq1_0_ref(const float* x, block_tq1_0* y, int64_t k);
void dequantize_row_tq1_0(const block_tq1_0* x, float* y, int64_t k);
size_t quantize_tq1_0(const float* src, void* dst, int64_t nrows, int64_t n_per_row);

// TQT_0: 环面三进制 (Krystal Spiral)
void quantize_row_tqt_0_ref(const float* x, block_tqt_0* y, int64_t k);
void dequantize_row_tqt_0(const block_tqt_0* x, float* y, int64_t k);

// GF(3) 矩阵乘法: y = (W @ x) % 3, 纯整数域
void gf3_matmul(const uint8_t* x, const uint8_t* w,
                int N, int out_dim, int in_dim, uint8_t* y);

// LCM 桥: acc = (acc × 177147) >> 16
int32_t lcm_bridge(int32_t acc);

#endif // PHASE3_GGML_QUANTS_H
