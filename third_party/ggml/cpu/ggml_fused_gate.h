// phase3/ggml/cpu/ggml_fused_gate.h — N14 门控融合 (完整相干积 + EMA + 自适应混合)
// y = gate * x_pe + (1-gate) * x_rope
//   其中 gate = clamp(sigmoid(logit + bias), adaptive_min, 0.99)
//   adaptive_min = 0.35 + n14_phase_error * n14_factor(trit)
//   n14_phase_error由 S¹ 复数相干积控制: e = (1 - |cos(θ_gate - θ_n14)|) EMA
// 编译: gcc -O3 -mavx2 -mfma -fPIC -shared ggml_fused_gate.c -o libggml_fused_gate.so -lm

#ifndef PHASE3_GGML_FUSED_GATE_H
#define PHASE3_GGML_FUSED_GATE_H

#include <stdint.h>

// ── N14 时钟状态 ──
// LCM 整数域相位累加器, 6624 步精确归零, 零累积误差
typedef struct {
    int64_t phase;              // [0, LCM) LCM 域相位
    int     step;               // 当前步数
    int     trit;               // GF(3) {0,1,2} = phase_6624 < LCM_THIRD ? 1 : phase_6624 >= 2*LCM_THIRD ? 2 : 0
    float   n14_phase_error;    // 复数相干积 EMA, 初始 0.0
} N14GateState;

// ── N14 LCM 常量 ──
#define N14_LCM_TOTAL     11609505792LL    // 3^11 × 2^16
#define N14_OMEGA_0       3708592128LL     // LCM / 144 × 46
#define N14_GRAND_PUMP    6624             // 144 × 46
#define N14_LCM_THIRD     3869835264LL     // LCM / 3
#define N14_LCM_TWO_THIRD 7739670528LL     // 2 * LCM / 3

// ─────────────────────────────────────────────────────────────
// 初始化 N14 时钟状态 (清零)
// ─────────────────────────────────────────────────────────────
void n14_gate_init(N14GateState* state);

// ─────────────────────────────────────────────────────────────
// N14 tick: 推进一步相位 + 更新 trit
//   phase = (phase + ω₀) % LCM
// ─────────────────────────────────────────────────────────────
void n14_gate_tick(N14GateState* state);

// ─────────────────────────────────────────────────────────────
// N14 复数相干积: 更新 n14_phase_error (EMA)
//   gate_val: 当前 step 的 gate 均值 (来自路由层)
//   state:    N14 时钟状态, 内含相位信息
//
//   θ_gate = (gate_val - 0.35) / 0.30 × 2π  映射到 [0.35, 0.65] → [0, 2π]
//   θ_n14  = phase_6624 / 6624 × 2π
//   coherence = |cos(θ_gate - θ_n14)|
//   error = 1 - coherence
//   n14_phase_error = n14_phase_error × 0.95 + error × 0.05   (EMA)
// ─────────────────────────────────────────────────────────────
void n14_gate_update_phase_error(N14GateState* state, float gate_val);

// ─────────────────────────────────────────────────────────────
// 获取自适应门控下限
//   adaptive_min = 0.35 + n14_phase_error × factor(trit)
//   factor: trit={0→0.5, 1→0.3, 2→0.7}
// ─────────────────────────────────────────────────────────────
float n14_gate_adaptive_min(const N14GateState* state);

// ─────────────────────────────────────────────────────────────
// N14 完整门控融合 (单步调用)
//   在一次调用中完成: tick → phase_error → clamp → blend
//
//   x_pe    [B, T, D] float32  PE 分支输出
//   x_rope  [B, T, D] float32  RoPE 分支输出
//   logit   [1]       float    logit_gate (标量)
//   pos_bias [T]      float32  逐位置偏置
//   state   N14GateState*     N14 时钟状态 (tick + phase_error 在内部更新)
//   y        [B, T, D] float32  输出
//   B, T, D
// ─────────────────────────────────────────────────────────────
void n14_fused_gate_blend(const float* x_pe, const float* x_rope,
                          float logit, const float* pos_bias,
                          N14GateState* state,
                          float* y, int B, int T, int D);

#endif // PHASE3_GGML_FUSED_GATE_H
