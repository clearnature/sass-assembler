// phase3/ggml/ggml-quants.c — 三进制量化实现
// 参考: llama.cpp ggml-quants.c
// 编译: gcc -O3 -mavx2 -fPIC -shared ggml-quants.c -o libggml_phase3.so
#include "ggml-quants.h"
#include <math.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// ══════════════════════════════════════════════════════════════
// 常量
// ══════════════════════════════════════════════════════════════
#define HUANGZHONG  177147  // 3^11
#define ZHONGLV_SHIFT 16   // 2^16
static const uint8_t POW3[6] = {1, 3, 9, 27, 81, 243};
static const uint8_t GF3_MUL[3][3] = {{0,0,0},{0,1,2},{0,2,1}};  // 2×2=1

// ══════════════════════════════════════════════════════════════
// TQ1_0 量化: float32 → {0,1,2} 三值打包 (同 llama.cpp)
// ══════════════════════════════════════════════════════════════
void quantize_row_tq1_0_ref(const float* x, block_tq1_0* y, int64_t k) {
    assert(k % QK_K == 0);
    int64_t nb = k / QK_K;

    for (int64_t i = 0; i < nb; i++) {
        // 求尺度: 绝对值最大值
        float amax = 0.0f;
        for (int j = 0; j < QK_K; j++)
            amax = fmaxf(amax, fabsf(x[j]));

        float d = amax;
        float id = d ? 1.0f/d : 0.0f;

        // fp16 scale (直接存 float, 实际应转 fp16)
        memcpy(&y[i].d, &d, sizeof(uint16_t));

        // 5 trit/byte, 沿 32 字节
        for (int j = 0; j < 48 - 48 % 32; j += 32) {
            for (int m = 0; m < 32; m++) {
                uint8_t q = 0;
                for (int n = 0; n < 5; n++) {
                    int xi = (int)(roundf(x[m + n*32] * id)) + 1;  // {-1,0,1}→{0,1,2}
                    if (xi < 0) xi = 0;
                    if (xi > 2) xi = 2;
                    q = q * 3 + xi;
                }
                // Ceiling division: [0,242] → [0,255]
                q = ((uint16_t)q * 256 + 242) / 243;
                y[i].qs[j + m] = q;
            }
            x += 5 * 32;
        }
        // 5 trit/byte, 沿 16 字节 (剩余)
        for (int j = 48 - 48 % 32; j < 48; j += 16) {
            for (int m = 0; m < 16; m++) {
                uint8_t q = 0;
                for (int n = 0; n < 5; n++) {
                    int xi = (int)(roundf(x[m + n*16] * id)) + 1;
                    if (xi < 0) xi = 0;
                    if (xi > 2) xi = 2;
                    q = q * 3 + xi;
                }
                q = ((uint16_t)q * 256 + 242) / 243;
                y[i].qs[j + m] = q;
            }
            x += 5 * 16;
        }
        // 4 trit/byte (qh)
        for (int j = 0; j < 4; j++) {
            uint8_t q = 0;
            for (int m = 0; m < 4; m++) {
                int xi = (int)(roundf(x[j + m*4] * id)) + 1;
                if (xi < 0) xi = 0;
                if (xi > 2) xi = 2;
                q = q * 3 + xi;
            }
            q = q * 3;  // shift to MSB
            q = ((uint16_t)q * 256 + 242) / 243;
            y[i].qh[j] = q;
        }
        x += 4 * 4;
    }
}

// ══════════════════════════════════════════════════════════════
// TQ1_0 解量化: 三值解包 → float32
// ══════════════════════════════════════════════════════════════
void dequantize_row_tq1_0(const block_tq1_0* x, float* y, int64_t k) {
    assert(k % QK_K == 0);
    int64_t nb = k / QK_K;

    for (int64_t i = 0; i < nb; i++) {
        float d;
        memcpy(&d, &x[i].d, sizeof(uint16_t));  // fp16 → float (近似)

        // 5 trit/byte, 32字节段
        for (int j = 0; j < 48 - 48 % 32; j += 32) {
            for (int n = 0; n < 5; n++) {
                for (int m = 0; m < 32; m++) {
                    uint8_t q = x[i].qs[j + m] * POW3[n];
                    int16_t xi = ((uint16_t)q * 3) >> 8;  // 近似逆映射
                    *y++ = (float)(xi - 1) * d;  // {0,1,2} → {-1,0,1} × scale
                }
            }
        }
        // 5 trit/byte, 16字节段
        for (int j = 48 - 48 % 32; j < 48; j += 16) {
            for (int n = 0; n < 5; n++) {
                for (int m = 0; m < 16; m++) {
                    uint8_t q = x[i].qs[j + m] * POW3[n];
                    int16_t xi = ((uint16_t)q * 3) >> 8;
                    *y++ = (float)(xi - 1) * d;
                }
            }
        }
        // 4 trit/byte
        for (int n = 0; n < 4; n++) {
            for (int j = 0; j < 4; j++) {
                uint8_t q = x[i].qh[j] * POW3[n];
                int16_t xi = ((uint16_t)q * 3) >> 8;
                *y++ = (float)(xi - 1) * d;
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════
// TQT_0 量化 (Krystal Spiral)
// ══════════════════════════════════════════════════════════════
void quantize_row_tqt_0_ref(const float* x, block_tqt_0* y, int64_t k) {
    assert(k % QK_K == 0);
    int64_t nb = k / QK_K;

    for (int64_t i = 0; i < nb; i++) {
        float amax = 0.0f;
        for (int j = 0; j < QK_K; j++)
            amax = fmaxf(amax, fabsf(x[j]));

        float d = amax;
        float id = d ? 1.0f/d : 0.0f;
        memcpy(&y[i].d, &d, sizeof(uint16_t));

        // Krystal Spiral 地址映射
        uint8_t spiraled[QK_K];
        for (int e = 0; e < QK_K; e++) {
            int xi = (int)(roundf(x[e] * id)) + 1;  // {-1,0,1} → {0,1,2}
            if (xi < 0) xi = 0;
            if (xi > 2) xi = 2;
            spiraled[(e * 5) % QK_K] = xi;  // Krystal Spiral
        }

        // 5 trit/byte 打包 qs[48]
        for (int j = 0; j < 48; j++) {
            uint8_t q = 0;
            for (int p = 0; p < 5; p++) {
                q = q * 3 + spiraled[j * 5 + p];
            }
            y[i].qs[j] = ((uint16_t)q * 256 + 242) / 243;
        }

        // 4 trit/byte 打包 qh[4]
        for (int j = 0; j < 4; j++) {
            uint8_t q = 0;
            for (int p = 0; p < 4; p++) {
                q = q * 3 + spiraled[240 + j * 4 + p];
            }
            q = q * 3;
            y[i].qh[j] = ((uint16_t)q * 256 + 242) / 243;
        }

        // padding 清零
        memset(y[i].padding, 0, 10);
    }
}

// ══════════════════════════════════════════════════════════════
// TQT_0 解量化
// ══════════════════════════════════════════════════════════════
void dequantize_row_tqt_0(const block_tqt_0* x, float* y, int64_t k) {
    assert(k % QK_K == 0);
    int64_t nb = k / QK_K;

    for (int64_t i = 0; i < nb; i++) {
        float d;
        memcpy(&d, &x[i].d, sizeof(uint16_t));

        // 解包 qs + qh → raw
        uint8_t raw[QK_K] = {0};
        for (int j = 0; j < 48; j++) {
            uint8_t byte = x[i].qs[j];
            // 逆映射
            byte = (uint8_t)(((uint16_t)byte * 3) >> 8);
            for (int p = 4; p >= 0; p--) {
                raw[j * 5 + p] = byte % 3;
                byte /= 3;
            }
        }
        for (int j = 0; j < 4; j++) {
            uint8_t byte = x[i].qh[j];
            byte = (uint8_t)(((uint16_t)byte * 3) >> 8);
            byte /= 3;
            for (int p = 3; p >= 0; p--) {
                raw[240 + j * 4 + p] = byte % 3;
                byte /= 3;
            }
        }

        // Krystal Spiral 逆映射
        for (int e = 0; e < QK_K; e++) {
            int xi = (int)raw[(e * 5) % QK_K] - 1;  // {0,1,2} → {-1,0,1}
            *y++ = (float)xi * d;
        }
    }
}

// ══════════════════════════════════════════════════════════════
// GF(3) 矩阵乘法: y = (W @ x) % 3
// ══════════════════════════════════════════════════════════════
void gf3_matmul(const uint8_t* x, const uint8_t* w,
                int N, int out_dim, int in_dim, uint8_t* y) {
    for (int n = 0; n < N; n++) {
        for (int i = 0; i < out_dim; i++) {
            int total = 0;
            for (int j = 0; j < in_dim; j++) {
                total += GF3_MUL[x[n * in_dim + j]][w[i * in_dim + j]];
            }
            y[n * out_dim + i] = total % 3;  // GF(3) 归约
        }
    }
}

// ══════════════════════════════════════════════════════════════
// LCM 桥: acc = (acc × 177147) >> 16
// ══════════════════════════════════════════════════════════════
int32_t lcm_bridge(int32_t acc) {
    return (int32_t)(((int64_t)acc * HUANGZHONG) >> ZHONGLV_SHIFT);
}

// ══════════════════════════════════════════════════════════════
// 批量量化入口
// ══════════════════════════════════════════════════════════════
size_t quantize_tq1_0(const float* src, void* dst, int64_t nrows, int64_t n_per_row) {
    size_t row_size = sizeof(block_tq1_0) * (n_per_row / QK_K);
    for (int64_t i = 0; i < nrows; i++) {
        quantize_row_tq1_0_ref(src + i * n_per_row,
                               (block_tq1_0*)((char*)dst + i * row_size),
                               n_per_row);
    }
    return nrows * row_size;
}
