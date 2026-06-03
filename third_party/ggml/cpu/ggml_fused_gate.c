// phase3/ggml/cpu/ggml_fused_gate.c — N14 门控融合 (完整 S¹ 相干积 + EMA + 自适应混合)
// 编译: gcc -O3 -mavx2 -mfma -fPIC -shared ggml_fused_gate.c -o libggml_fused_gate.so -lm
#include "ggml_fused_gate.h"
#include <math.h>
#include <string.h>
#include <stdint.h>

#if defined(__AVX2__) && defined(__FMA__)
#include <immintrin.h>
#endif

// ═══════════════════════════════════════════════════════════════
// N14 时钟核心
// ═══════════════════════════════════════════════════════════════

void n14_gate_init(N14GateState* state) {
    state->phase = 0;
    state->step = 0;
    state->trit = 0;       // 内禀 trit=0 → n14_trit=1
    state->n14_phase_error = 0.0f;
}

void n14_gate_tick(N14GateState* state) {
    // phase = (phase + ω₀) % LCM
    state->phase = (state->phase + N14_OMEGA_0) % N14_LCM_TOTAL;
    state->step++;

    // GF(3) trit: 映射 phase_6624 到三个相位区
    // phase_6624 = phase × 6624 / LCM
    // 用除法判断: phase / LCM_THIRD 的商决定 trit
    if (state->phase < N14_LCM_THIRD)
        state->trit = 1;       // C3 顺时针, 激发态
    else if (state->phase >= N14_LCM_TWO_THIRD)
        state->trit = -1;      // C3 逆时针, 抑制态
    else
        state->trit = 0;       // 基态
}

// ── phase_6624 规约到 [0, 6624) ──
static inline int phase_6624(int64_t phase) {
    return (int)((phase * N14_GRAND_PUMP) / N14_LCM_TOTAL);
}

void n14_gate_update_phase_error(N14GateState* state, float gate_val) {
    // θ_gate: gate 值 [0.35, 0.65] → [0, 2π]
    float gate_clipped = fminf(fmaxf(gate_val, 0.35f), 0.65f);
    float theta_gate = ((gate_clipped - 0.35f) / 0.30f) * 2.0f * (float)M_PI;

    // θ_n14: N14 相位 [0, 6624) → [0, 2π]
    int p6624 = phase_6624(state->phase);
    float theta_n14 = (p6624 / 6624.0f) * 2.0f * (float)M_PI;

    // S¹ 复数相干积: coherence = |cos(差角)|, error = 1 - coherence
    float diff = theta_gate - theta_n14;
    float coherence = fabsf(cosf(diff));
    float error = 1.0f - coherence;

    // EMA: e = e × 0.95 + error × 0.05
    state->n14_phase_error = state->n14_phase_error * 0.95f + error * 0.05f;
}

float n14_gate_adaptive_min(const N14GateState* state) {
    // n14_factor: trit = {1→0.3, 0→0.5, 2→0.7}
    // 注意: state->trit 是内禀值 {-1,0,+1}, 需要 +1 映射到 {0,1,2}
    int n14_trit = state->trit + 1;  // {-1,0,+1} → {0,1,2}
    float n14_factor;
    switch (n14_trit) {
        case 1:  n14_factor = 0.3f; break;  // C3 顺时针, 激发态
        case 0:  n14_factor = 0.5f; break;  // 基态
        case 2:  n14_factor = 0.7f; break;  // C3 逆时针, 抑制态
        default: n14_factor = 0.5f; break;
    }
    return 0.35f + state->n14_phase_error * n14_factor;
}

// ═══════════════════════════════════════════════════════════════
// AVX2 sigmoid (tanh 有理近似)
// ═══════════════════════════════════════════════════════════════
#if defined(__AVX2__) && defined(__FMA__)
static inline __m256 sigmoid_tanh_avx(__m256 x) {
    __m256 half = _mm256_set1_ps(0.5f);
    __m256 z = _mm256_mul_ps(x, _mm256_set1_ps(0.5f));
    __m256 z2 = _mm256_mul_ps(z, z);
    __m256 num = _mm256_mul_ps(z, _mm256_add_ps(_mm256_set1_ps(27.0f), z2));
    __m256 den = _mm256_add_ps(_mm256_set1_ps(27.0f), _mm256_mul_ps(_mm256_set1_ps(9.0f), z2));
    __m256 tanh_z = _mm256_div_ps(num, den);
    return _mm256_add_ps(half, _mm256_mul_ps(half, tanh_z));
}
#endif

// ═══════════════════════════════════════════════════════════════
// 公开 API: N14 完整门控融合 (单步调用)
// ═══════════════════════════════════════════════════════════════

void n14_fused_gate_blend(const float* x_pe, const float* x_rope,
                          float logit, const float* pos_bias,
                          N14GateState* state,
                          float* y, int B, int T, int D) {
    // 1. N14 tick
    n14_gate_tick(state);

    // 2. 计算 gate 均值 → 更新 N14 相位误差
    //    直接从 logit + pos_bias 推断 gate 均值
    float gate_mean = 0.0f;
    for (int t = 0; t < T; t++) {
        float raw = logit + pos_bias[t];
        // sigmoid 近似
        float gate_t = 1.0f / (1.0f + expf(-raw));
        gate_mean += gate_t;
    }
    gate_mean /= (float)T;
    n14_gate_update_phase_error(state, gate_mean);

    // 3. 获取自适应门控下限
    float adaptive_min = n14_gate_adaptive_min(state);
    float adaptive_max = 0.99f;

    // 4. 融合混合: y = clamp(sigmoid(logit + bias)) * x_pe + (1-gate) * x_rope
    //    按 (batch, pos) 逐 D 维执行

#if defined(__AVX2__) && defined(__FMA__)
    __m256 min_v = _mm256_set1_ps(adaptive_min);
    __m256 max_v = _mm256_set1_ps(adaptive_max);

    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            // gate = clamp(sigmoid(logit + pos_bias[t]))
            float raw = logit + pos_bias[t];
            __m256 raw_v = _mm256_set1_ps(raw);
            __m256 gate_v = sigmoid_tanh_avx(raw_v);
            gate_v = _mm256_min_ps(_mm256_max_ps(gate_v, min_v), max_v);

            // FMA 变形: y = rope + gate * (pe - rope)
            const float* pe_row = x_pe + (b * T + t) * D;
            const float* ro_row = x_rope + (b * T + t) * D;
            float* y_row = y + (b * T + t) * D;

            int d;
            for (d = 0; d <= D - 8; d += 8) {
                __m256 pe_v = _mm256_loadu_ps(pe_row + d);
                __m256 ro_v = _mm256_loadu_ps(ro_row + d);
                __m256 delta = _mm256_sub_ps(pe_v, ro_v);
                __m256 yv = _mm256_fmadd_ps(gate_v, delta, ro_v);
                _mm256_storeu_ps(y_row + d, yv);
            }
            for (; d < D; d++)
                y_row[d] = gate_v[0] * pe_row[d] + (1.0f - gate_v[0]) * ro_row[d];
        }
    }
#else
    float gate_cache;
    for (int b = 0; b < B; b++) {
        for (int t = 0; t < T; t++) {
            float gate = 1.0f / (1.0f + expf(-(logit + pos_bias[t])));
            if (gate < adaptive_min) gate = adaptive_min;
            if (gate > adaptive_max) gate = adaptive_max;

            const float* pe_row = x_pe + (b * T + t) * D;
            const float* ro_row = x_rope + (b * T + t) * D;
            float* y_row = y + (b * T + t) * D;

            for (int d = 0; d < D; d++)
                y_row[d] = gate * pe_row[d] + (1.0f - gate) * ro_row[d];
        }
    }
#endif
}
