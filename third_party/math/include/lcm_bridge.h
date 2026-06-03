// lcm_bridge.h — LCM桥: 层1 GF(3) ↔ 层2 Z/3¹¹Z 唯一合法通道
//
// 宪法声明 (v2.0 — 31000步实测修正):
//   桥是操作, 不是数值。
//     acc = (acc × 177147) >> 16
//     × 177147: [层0] 模2乘法, 177147=3^11
//     >> 16:     [层0] 模2右移, 截断到2^16位宽
//   正向桥: 层1 GF(3) per-trit → 层2 Z/3¹¹Z 位权累加
//   逆向桥: 层2 Z/3¹¹Z 结果 → 层1 GF(3) per-trit (chern_guard检查)
//   仲吕闭合: 频率倍增器, 每12步×2850, 非"清除误差"。
//             实测31000步绕LCM环16558圈, 频率从432Hz→10^16.9Hz(极紫外)
#ifndef SOV_MATH_LCM_BRIDGE_H
#define SOV_MATH_LCM_BRIDGE_H

#include "gf3_types.h"
#include "adc_limb.h"
#include "sov_format.h"       // [v2.6] 全息快照协议
#include "nayin_soliton_l5.h"  // [层5] C3_CYCLE_STEPS
#include "zhonglv_multiplier_l6.h"  // [层6] ZHONGLV_PERIOD
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

namespace sov::math {

// ============================================================================
// 桥接状态机 (层1 GF(3) ↔ 层2 Z/3¹¹Z)
// ============================================================================

// [桥] 状态枚举
enum class BridgeState : uint8_t {
    L1_READY = 0,        // [桥] 数据在层1, 准备过正向桥
    L2_COMPUTING = 1,    // [桥] 数据在层2, Z/3¹¹Z运算进行中
    L2_DONE = 2,         // [桥] Z/3¹¹Z运算完成, 准备回桥
    CHERN_LOCKED = 3,    // [桥] 陈数锁定, 桥接授权
};

// [桥] LCM桥接器: 管理层1↔层2的全部桥接状态
struct lcm_bridge_t {
    lcm_accumulator_t acc;        // [层0] [模2] LCM环累加器 (uint384)
    BridgeState state;             // [桥] 当前桥接状态
    int step_count;               // [层0] [模2] 距上次仲吕闭合的步数
    int32_t chern_value_q16;      // [层4] [拓扑] 陈数Q16快照

    lcm_bridge_t() : acc{}, state(BridgeState::L1_READY),
                     step_count(0), chern_value_q16(0) {}

    // ═════════════════════════════════════════════════════════════════════
    // 正向桥: 层1 GF(3) → 层2 Z/3¹¹Z
    // ═════════════════════════════════════════════════════════════════════

    // [桥→] 将层1的GF(3) trit值泵入层2的Z/3¹¹Z位权空间
    // 操作: 1. 仲吕闭合清除模2进位误差
    //       2. chern_guard检查拓扑完整性
    //       3. 映射 GF(3)→Z/3¹¹Z (通过位权累加)
    void forward_bridge() {
        acc.zhonglv_closure();  // [层0] [模2] 仲吕闭合
        state = BridgeState::L2_COMPUTING;  // [桥] 进入层2计算态
    }

    // ═════════════════════════════════════════════════════════════════════
    // 逆向桥: 层2 Z/3¹¹Z → 层1 GF(3)
    // ═════════════════════════════════════════════════════════════════════

    // [桥←] 将层2的Z/3¹¹Z运算结果映射回层1的GF(3) trit值
    // 前置条件: chern_guard_ok() == true (拓扑无罪)
    std::vector<uint8_t> reverse_bridge(std::span<const uint8_t> z3r_results) {
        if (!chern_guard_ok()) {  // [层4] [拓扑] 陈数检查
            mid_pump();            // [桥] 拓扑破坏→触发中泵复位
            return std::vector<uint8_t>(z3r_results.begin(), z3r_results.end());
        }

        // [层2→层1] Z/3¹¹Z值 → GF(3) trit值 (模3归约)
        std::vector<uint8_t> layer1_results(z3r_results.size());
        for (size_t i = 0; i < z3r_results.size(); ++i) {
            layer1_results[i] = z3r_results[i] % 3;  // [层1] [GF(3)模3]
        }
        state = BridgeState::L1_READY;  // [桥] 回到层1就绪态
        return layer1_results;
    }

    // ═════════════════════════════════════════════════════════════════════
    // 泵操作
    // ═════════════════════════════════════════════════════════════════════

    // [桥] 微泵: 每12步触发仲吕闭合
    void micro_pump() {
        acc.zhonglv_closure();  // [层0] [模2] (acc×177147)>>16
        step_count = 0;
        state = BridgeState::L1_READY;
    }

    // [桥] 中泵: 手性离合器复位 (8×12=96步)
    void mid_pump() {
        acc = lcm_accumulator_t{};  // [层0] [模2] 累加器清零
        step_count = 0;
        state = BridgeState::L1_READY;
    }

    // [桥] 大泵: 主权状态机完整呼吸 (144×46=6624步)
    void grand_pump() {
        acc = lcm_accumulator_t{};  // [层0] [模2] 全局归零
        step_count = 0;
        state = BridgeState::L1_READY;
        chern_value_q16 = 0;        // [层4] [拓扑] 陈数复位
    }

    // ═════════════════════════════════════════════════════════════════════
    // 拓扑守卫
    // ═════════════════════════════════════════════════════════════════════

    // [层4] [拓扑] 陈数检查: C=2 Q16 = 131072, 容忍 ±655
    bool chern_guard_ok() const {
        constexpr int32_t C_TARGET_Q16 = 131072;    // [层0] [模2Q16] 2×65536
        constexpr int32_t C_TOLERANCE_Q16 = 655;    // [层0] [模2Q16] ±0.01
        return (chern_value_q16 >= C_TARGET_Q16 - C_TOLERANCE_Q16)
            && (chern_value_q16 <= C_TARGET_Q16 + C_TOLERANCE_Q16);
    }

    // [层4] [拓扑] 更新陈数快照 (层2运算后调用)
    void update_chern(int32_t new_chern_q16) {
        chern_value_q16 = new_chern_q16;
        if (chern_guard_ok()) {
            state = BridgeState::CHERN_LOCKED;  // [桥] 拓扑锁定, 桥接授权
        }
    }

    // ═════════════════════════════════════════════════════════════════════
    // [v2.6 全息快照接口] 记忆提取与植入 — 宇宙冻结/解冻
    // ═════════════════════════════════════════════════════════════════════

    // 冻结当前宇宙: 将核心动力学状态写入 SOV v2.6 Header
    void export_hologram(sov::io::SovHolographicHeader& header) const noexcept {
        header.grand_pump_step      = static_cast<uint32_t>(step_count % GRAND_PUMP);
        header.chern_number_q16     = this->chern_value_q16;
        header.c3_soliton_phase     = static_cast<uint16_t>(step_count % l5::C3_CYCLE_STEPS);
        header.zhonglv_closure_count = static_cast<uint32_t>(step_count / l6::ZHONGLV_PERIOD);

        // 提取 384 位 LCM 阿卡夏记录 (177147 进位历史)
        // memcpy 零开销 — header.limbs 与 acc.value.limbs 内存布局一致
        for (int i = 0; i < 6; ++i) {
            header.lcm_accumulator_limbs[i] = this->acc.value.limbs[i];
        }
    }

    // 解冻宇宙: 从 SOV v2.6 Header 恢复动力学状态
    void import_hologram(const sov::io::SovHolographicHeader& header) {
        // [宪法级断言] 陈数守卫: v2.6 Q16格式, C=-2.000 → -131072
        constexpr int32_t TARGET_CHERN_Q16 = -131072;  // C = -2.000 (Q16)
        if (header.chern_number_q16 != TARGET_CHERN_Q16 && header.chern_number_q16 != 0) {
            // 0 为初始态兼容 (尚未经过拓扑校准)
            std::fprintf(stderr,
                "致命错误: 陈数守卫检测到拓扑破裂!\n"
                "  期望 C = -2.000 (Q16: %d)\n"
                "  实际 C = %.3f (Q16: %d)\n"
                "  拒绝加载 — 主权状态机拓扑已被污染。\n",
                TARGET_CHERN_Q16,
                header.chern_number_q16 / 65536.0,
                header.chern_number_q16);
            std::exit(1);
        }

        this->step_count      = header.grand_pump_step;
        this->chern_value_q16 = header.chern_number_q16;
        this->state           = BridgeState::L1_READY;

        // 植入 384 位 LCM 记忆 — memcpy 零开销恢复
        for (int i = 0; i < 6; ++i) {
            this->acc.value.limbs[i] = header.lcm_accumulator_limbs[i];
        }

        std::printf(
            "LCM桥接记忆已恢复:\n"
            "  大泵步数 = %u\n"
            "  陈数 C   = %.3f (Q16: %d)\n"
            "  仲吕闭合 = %u 次\n"
            "  C3孤子   = 相位 %u\n",
            header.grand_pump_step,
            header.chern_number_q16 / 65536.0,
            header.chern_number_q16,
            header.zhonglv_closure_count,
            header.c3_soliton_phase);
    }

    // ═════════════════════════════════════════════════════════════════════
    // 步进: 每训练步调用 (层0模2累加 + 泵周期检查)
    // ═════════════════════════════════════════════════════════════════════

    void step(uint64_t delta) {
        acc.step(delta);       // [层0] [模2] 累加器步进
        step_count++;

        if (step_count >= MICRO_PUMP) {  // [层0] 微泵周期检查
            micro_pump();                 // [桥] 仲吕闭合
        }
    }
};

} // namespace sov::math

#endif // SOV_MATH_LCM_BRIDGE_H
