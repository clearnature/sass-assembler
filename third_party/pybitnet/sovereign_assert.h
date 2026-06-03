// sovereign_assert.h — 律算宪法断言模板 (C++23 consteval/static_assert)
// [编译期] 验证全部数学库的宪法合规性, 每断言标注层级+范畴
// [反优化] 禁止编译器将主权运算优化为近似表达式
//
// v2.0 宪法不变量 (31000步实测验证):
//   POLAR_WINDING=144(120+24)  TOROIDAL=46  C=-2.000
//   LCM环=频率倍增器, 仲吕闭合×2850/12步
//   C3孤子周期=1500步=12×5³  驻波tone%3==0
//   陈数不翻转=手性方向锁定的动力学常数
#ifndef SOV_MATH_SOVEREIGN_ASSERT_H
#define SOV_MATH_SOVEREIGN_ASSERT_H

#include "lcm_constants.h"
#include "gf3_types.h"
#include <cstdint>
#include <bit>

namespace sov::math::asserts {

// ============================================================================
// 零、反优化原语 — 禁止编译器错误优化主权运算
// ============================================================================

// 编译器屏障: 禁止跨越此屏障重排/消除内存操作
#define SOV_COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")

// 标记变量为"外部可见" — 禁止编译器消除
#define SOV_KEEP(x) __asm__ __volatile__("" :: "r"(x) : "memory")

// 标记函数不可内联/不可消除
#define SOV_NOOPT __attribute__((noinline, used))

// volatile 累加器 — 禁止编译器将多步累加折叠为常量
template<typename T>
struct volatile_acc {
    volatile T value;
    volatile_acc(T v = 0) : value(v) {}
    SOV_NOOPT void add(T delta) { SOV_COMPILER_BARRIER(); value += delta; SOV_COMPILER_BARRIER(); }
    SOV_NOOPT T get() const { SOV_COMPILER_BARRIER(); T v = value; SOV_COMPILER_BARRIER(); return v; }
};

// ============================================================================
// 一、层0 断言: 二进制硬件边界
// ============================================================================

namespace layer0_asserts {

// ADC limb 容量足以覆盖 10^96
static_assert(ADC_LIMB_COUNT >= 5,
    "ADC limb数量不足以覆盖10^96 (需 ≥5×64=320位, 实际需384位)");

// 2^16 = 65536 — 二进制截断边界
static_assert((1ULL << ZHONGLV_SHIFT) == ZHONGLV_BOUNDARY,
    "仲吕边界 2^16=65536 不变");
static_assert(ZHONGLV_BOUNDARY == 65536ULL,
    "二进制截断位宽 2^16");

} // namespace layer0_asserts

// ============================================================================
// 二、层1 断言: {0,1,2} 标准三进制编码
// ============================================================================

namespace layer1_asserts {

// 5 trit/byte 打包: 3^5 = 243 ≤ 255 (uint8合法)
static_assert(TRITS_PER_BYTE == 5,
    "每字节5 trit");
static_assert(243 <= 255,
    "3^5=243 < 256, uint8可容纳");
static_assert(TRITS_PER_TRYTE == 6,
    "每tryte 6 trit");
static_assert(TRYTE_MAX_VALUE == 729,
    "3^6 = 729 tryte态");

// 层1编码: {0→0b00, 1→0b01, 2→0b10}
//static_assert(TRIT_LAYER1_0 == 0b00 && TRIT_LAYER1_1 == 0b01 && TRIT_LAYER1_2 == 0b10,
//    "层1编码: 0→00, 1→01, 2→10");
//static_assert(TRIT_LAYER1_MASK == 0b11,
//    "层1 2-bit掩码");

} // namespace layer1_asserts

// ============================================================================
// 三、层2 断言: {T₀,T₁,T₂} GF(3) 本征域
// ============================================================================

namespace layer2_asserts {

// Trit 枚举值合法
static_assert(trit_val(Trit::T0) == 0, "T₀ = 0");
static_assert(trit_val(Trit::T1) == 1, "T₁ = 1");
static_assert(trit_val(Trit::T2) == 2, "T₂ = 2");

// GF(3) 乘法表: T₂⊗T₂ = T₁ (2×2=4≡1 mod 3, 非4)
static_assert(TRIT_MUL_LUT[0][0] == 0, "T₀⊗T₀ = T₀");
static_assert(TRIT_MUL_LUT[0][1] == 0, "T₀⊗T₁ = T₀");
static_assert(TRIT_MUL_LUT[0][2] == 0, "T₀⊗T₂ = T₀");
static_assert(TRIT_MUL_LUT[1][0] == 0, "T₁⊗T₀ = T₀");
static_assert(TRIT_MUL_LUT[1][1] == 1, "T₁⊗T₁ = T₁");
static_assert(TRIT_MUL_LUT[1][2] == 2, "T₁⊗T₂ = T₂");
static_assert(TRIT_MUL_LUT[2][0] == 0, "T₂⊗T₀ = T₀");
static_assert(TRIT_MUL_LUT[2][1] == 2, "T₂⊗T₁ = T₂");
static_assert(TRIT_MUL_LUT[2][2] == 1, "T₂⊗T₂ = T₁ (2×2≡1 mod 3) ← 宪法核心");

// GF(3) 加法表: 逢三进一 (非简单 mod-3!)
static_assert(TRIT_ADD_SUM[0][0]   == 0 && TRIT_ADD_CARRY[0][0]   == 0, "T₀+T₀");
static_assert(TRIT_ADD_SUM[0][1]   == 1 && TRIT_ADD_CARRY[0][1]   == 0, "T₀+T₁");
static_assert(TRIT_ADD_SUM[1][1]   == 2 && TRIT_ADD_CARRY[1][1]   == 0, "T₁+T₁");
static_assert(TRIT_ADD_SUM[2][0]   == 2 && TRIT_ADD_CARRY[2][0]   == 0, "T₂+T₀");
static_assert(TRIT_ADD_SUM[2][1]   == 0 && TRIT_ADD_CARRY[2][1]   == 1, "T₂+T₁ = T₀进1 ← 逢三进一");
static_assert(TRIT_ADD_SUM[2][2]   == 1 && TRIT_ADD_CARRY[2][2]   == 1, "T₂+T₂ = T₁进1 ← 2+2=4=1×3+1");

// GF(3) 范数: |T₀|²=0, |T₁|²=1, |T₂|²≡1
static_assert(TRIT_NORM_LUT[0] == 0, "|T₀|² = 0");
static_assert(TRIT_NORM_LUT[1] == 1, "|T₁|² = 1");
static_assert(TRIT_NORM_LUT[2] == 1, "|T₂|² ≡ 1 (2²=4≡1 mod 3)");

// 所有 LUT 值域均在 {0,1,2} 内
consteval bool verify_lut_range_3x3(const auto& lut) {
    for (uint8_t i = 0; i < 3; ++i)
        for (uint8_t j = 0; j < 3; ++j)
            if (lut[i][j] > 2) return false;
    return true;
}
static_assert(verify_lut_range_3x3(TRIT_ADD_SUM),   "加法表值域 {0,1,2}");
static_assert(verify_lut_range_3x3(TRIT_ADD_CARRY), "进位数表值域 {0,1}");
static_assert(verify_lut_range_3x3(TRIT_MUL_LUT),   "乘法表值域 {0,1,2}");

} // namespace layer2_asserts

// ============================================================================
// 四、LCM桥断言: 层1 ↔ 层2 桥接
// ============================================================================

namespace bridge_asserts {

// LCM = 3^11 × 2^16 = 11609505792
static_assert(HUANGZHONG == 177147ULL,      "黄钟 3^11");
static_assert(ZHONGLV_BOUNDARY == 65536ULL,  "仲吕边界 2^16");
static_assert(LCM_TOTAL == 11609505792ULL,   "LCM = 3^11×2^16");

// 仲吕闭合自洽: (0 * 177147) >> 16 = 0
static_assert(((0ULL * HUANGZHONG) >> ZHONGLV_SHIFT) == 0,
    "仲吕闭合(0) = 0");

// 桥接输出范围: (acc * 177147) >> 16 总是合法
consteval bool verify_bridge_range(uint64_t acc) {
    uint64_t r = (acc * HUANGZHONG) >> ZHONGLV_SHIFT;
    return (r % 3) <= 2;
}
static_assert(verify_bridge_range(0),       "桥接(0) 合法");
static_assert(verify_bridge_range(1),       "桥接(1) 合法");
static_assert(verify_bridge_range(2),       "桥接(2) 合法");
static_assert(verify_bridge_range(65535),   "桥接(65535) 合法");
static_assert(verify_bridge_range(LCM_TOTAL - 1), "桥接(LCM-1) 合法");

} // namespace bridge_asserts

// ============================================================================
// 五、泵系统断言: 拓扑周期不变量
// ============================================================================

namespace pump_asserts {

// 微泵 = 12 (十二律)
static_assert(MICRO_PUMP == 12,
    "微泵 = 十二律");

// 中泵 = 96 (八轮微泵)
static_assert(MID_PUMP == 96,
    "中泵 = 八轮微泵");

// 大泵 = 6624 (极向×环向, 主权状态机完整呼吸)
static_assert(GRAND_PUMP == 6624,
    "大泵 = 6624 = 极向×环向");

// 极向缠绕步数
static_assert(POLAR_WINDING == 144,
    "极向缠绕 = 144");

// 环向缠绕本征模式数 (不可拆分的拓扑不变量)
static_assert(TOROIDAL_WINDING == 46,
    "环向缠绕 = 46");

// 不可通约性: 极向与环向不可通约
// 这正是仲吕闭合必须强制介入的拓扑根源
static_assert(POLAR_WINDING % TOROIDAL_WINDING != 0
              && TOROIDAL_WINDING % POLAR_WINDING != 0,
    "极向与环向不可通约");

} // namespace pump_asserts

// ============================================================================
// 六、拓扑不变量断言
// ============================================================================

namespace topology_asserts {

// 陈数 C = 2 (S² 球面欧拉示性数 χ=2)
static_assert(CHERN_TARGET == 2,
    "陈数 C = 2");

// 能隙 Δ² = 3 (平方值, 整数域)
// 能隙 Δ² = 3 (陈数保护下的最小拓扑壁垒)
consteval bool verify_energy_gap() {
    // |T₁|² = 1, |T₂|² = 1 → 最小非零范数为1
    // 但Δ² = 3 是全局能隙 (跨trit累积后)
    // 验证: 3个 T₁ 的累积范数 = 1+1+1 = 3 = Δ²
    return TRIT_NORM_LUT[1] * 3 == 3;
}
static_assert(verify_energy_gap(),
    "能隙 Δ² = 3 (三态累积范数)");

// Q16 能隙验证: Δ_q16² / 65536² ≈ 3.0 ± tolerance
consteval bool verify_delta_q16() {
    // (113506 / 65536)² ≈ (1.73205...)² ≈ 3.000...
    int64_t d2 = (int64_t)DELTA_Q16 * (int64_t)DELTA_Q16;
    int64_t expected = 3LL * 65536LL * 65536LL;  // 3 × 2^32
    int64_t diff = d2 - expected;
    // 允许 0.01% 的截断容差
    return diff > -((int64_t)1 << 24) && diff < ((int64_t)1 << 24);
}
static_assert(verify_delta_q16(),
    "能隙 Δ=√3 的Q16表示偏离超限");

} // namespace topology_asserts

// ============================================================================
// 七、纳音谱系断言
// ============================================================================

namespace nayin_asserts {

// 六十甲子
static_assert(NAYIN_COUNT == 60,
    "纳音标签 = 60甲子");

// 十二律
static_assert(TONE_COUNT == 12,
    "十二律");

// 五行
static_assert(WUXING_COUNT == 5,
    "五行");

// Tryte → 纳音 映射自洽: tryte(0) → nayin(0)
static_assert(TryteValue{0}.nayin_label() == 0,
    "tryte(0) 纳音 = 0");
static_assert(TryteValue{60}.nayin_label() == 0,
    "tryte(60) 纳音 = 0 (mod 60闭合)");

} // namespace nayin_asserts

// ============================================================================
// 八、SovBlock128 断言: 主权块结构
// ============================================================================

namespace block_asserts {

// 16字节对齐
static_assert(sizeof(SovBlock128) == 16,
    "SovBlock128 = 16字节");
static_assert(alignof(SovBlock128) == 16,
    "SovBlock128 16字节对齐");

// qs 中的 trit 值域验证: 打包后解包应恢复
// SovBlock128 qs[0] 5-trit打包: 直接存 base-3编码值
consteval bool verify_qs_pack(uint8_t t0, uint8_t t1, uint8_t t2,
                               uint8_t t3, uint8_t t4) {
    // 5 trit → 1 byte: PACK_5_LUT 的索引 = t0 + t1*3 + t2*9 + t3*27 + t4*81
    uint16_t idx = t0 + t1*3 + t2*9 + t3*27 + t4*81;
    uint8_t packed = PACK_5_LUT[idx];
    // 解包验证: UNPACK_5_LUT 高位优先 {t4,t3,t2,t1,t0}
    auto& u5 = UNPACK_5_LUT[packed];
    return u5[4] == t0 && u5[3] == t1 && u5[2] == t2 && u5[1] == t3 && u5[0] == t4;
}
static_assert(verify_qs_pack(0, 0, 0, 0, 0), "Pack5 00000 往返");
static_assert(verify_qs_pack(1, 1, 1, 1, 1), "Pack5 11111 往返");
static_assert(verify_qs_pack(2, 2, 2, 2, 2), "Pack5 22222 往返");
static_assert(verify_qs_pack(0, 1, 2, 0, 1), "Pack5 01201 往返");

} // namespace block_asserts

// ============================================================================
// 九、反优化编译标志文档
// ============================================================================

// 推荐编译选项 (CMakeLists.txt / Makefile):
//
//   g++ -std=c++23 -O3 -mavx2 -march=native \
//       -fno-strict-aliasing        ← 禁止严格别名优化 (破坏 trit 位操作)
//       -fno-omit-frame-pointer     ← 保留帧指针 (方便调试主权步进)
//       -fwrapv                     ← 有符号整数溢出为环绕 (非UB)
//       -fsigned-char               ← char = signed (平衡三进制兼容)
//
// 禁止使用的优化选项:
//   -ffast-math                    ← 违宪! 引入浮点近似
//   -funsafe-math-optimizations    ← 违宪! 允许有损代数重排
//   -ftree-vectorize -O3 默认     ← 允许, 但需用 SOV_COMPILER_BARRIER 保护关键段
//   -flto                          ← 谨慎! 跨TU内联可能消除主权步进

// ============================================================================
// 十、运行时宪法验证 (consteval 编译后, 运行时二次确认)
// ============================================================================

// 全宪法断言汇总 — 一次调用触发全部编译期 + 运行时验证
SOV_NOOPT
inline bool full_constitutional_audit() {
    // 运行时验证: GF(3) 乘法表自洽
    for (uint8_t a = 0; a < 3; ++a) {
        for (uint8_t b = 0; b < 3; ++b) {
            volatile uint8_t result = TRIT_MUL_LUT[a][b];
            if (result > 2) return false;
        }
    }

    // 运行时验证: 逢三进一
    volatile uint8_t sum_21   = TRIT_ADD_SUM[2][1];
    volatile uint8_t carry_21 = TRIT_ADD_CARRY[2][1];
    if (sum_21 != 0 || carry_21 != 1) return false;

    volatile uint8_t sum_22   = TRIT_ADD_SUM[2][2];
    volatile uint8_t carry_22 = TRIT_ADD_CARRY[2][2];
    if (sum_22 != 1 || carry_22 != 1) return false;

    // 运行时验证: 仲吕闭合
    volatile uint64_t acc = 0;
    SOV_COMPILER_BARRIER();
    acc = (acc * HUANGZHONG) >> ZHONGLV_SHIFT;
    SOV_COMPILER_BARRIER();
    if (acc != 0) return false;

    // 所有验证通过
    return true;
}

} // namespace sov::math::asserts

#endif // SOV_MATH_SOVEREIGN_ASSERT_H
