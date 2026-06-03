// lcm_constants.h — 律算常数, 每行标注层级+范畴+模
// C++23, 纯整数域, 零浮点
//
// v2.0 实测更新 (2026-05-06, 31000步全LCM环训练):
//   LCM环 = 频率倍增器, 非单次遍历环
//   31000步绕圈16558次, 频率432Hz→10^16.9Hz(极紫外~330eV)
//   仲吕闭合2583次, 频率倍增20664次, 跨越14个数量级
//   C3孤子{94.8,4.3,0.9}在超流态中1500步周期稳定轮转
//   陈数C=-2.000全程锁定 — 手性方向不变量, 非频率绝对值
#ifndef SOV_MATH_LCM_CONSTANTS_H
#define SOV_MATH_LCM_CONSTANTS_H

#include <cstdint>
#include <cstddef>

namespace sov::math {

// ═══════════════════════════════════════════════════════════════════════════
// 层0: 二进制硬件 — 模2运算, x86-64 CPU
// 范畴: 硅基晶格的物理现实, 所有uint64_t运算都在此层执行
// ═══════════════════════════════════════════════════════════════════════════

// [层0] [模2] x86-64单limb位宽
inline constexpr uint64_t BINARY_LIMB_BITS  = 64;
// [层0] [模2] 64位全掩码
inline constexpr uint64_t BINARY_LIMB_MASK  = 0xFFFFFFFFFFFFFFFFULL;
// [层0] [模2] ADC进位链limb数 (6×64=384位, 覆盖10^96)
inline constexpr size_t   ADC_LIMB_COUNT   = 6;

// ═══════════════════════════════════════════════════════════════════════════
// 层1: GF(3) 有限域 — 模3运算, 3≡0
// 范畴: 每个trit是独立的GF(3)元素 {0,1,2}, 特征3, 没有位权
// 运算: (a+b)%3 (域加法), (a×b)%3 (域乘法)
// 存储: 2-bit/trit, uint8容器
// ═══════════════════════════════════════════════════════════════════════════

// [层1] [GF(3)模3] trit硬件编码: 00→0, 01→1, 10→2, 11非法
inline constexpr uint8_t GF3_T0 = 0b00;
inline constexpr uint8_t GF3_T1 = 0b01;
inline constexpr uint8_t GF3_T2 = 0b10;
inline constexpr uint8_t GF3_MASK = 0b11;

// [层1] [GF(3)模3] 5 trit打包为1字节 (3^5=243 ≤ 255)
// byte = t₀×3⁰ + t₁×3¹ + t₂×3² + t₃×3³ + t₄×3⁴
// 注意: 这里的3¹3²3³3⁴是模2运算下的十进制数, 仅用于编码/解码
//       不是Z/3¹¹Z环中的位权值
inline constexpr int TRITS_PER_BYTE  = 5;
inline constexpr int TRITS_PER_TRYTE = 6;
inline constexpr int TRYTE_MAX_VALUE = 729;   // 3⁶, 模2十进制

// ═══════════════════════════════════════════════════════════════════════════
// 层0↔层1↔层2 LCM桥 — 模2运算下的桥接操作
// 范畴: 操作, 非数值。两端是不同范畴的数学空间, 不可用单一标量等价。
//
//   acc = (acc × 177147) >> 16
//
//   acc:  [层0] uint64_t, 模2二进制整数
//   ×:    [层0] 模2乘法 (二进制CPU指令)
//   177147: [层0] 十进制整数 (3^11在模2下的表示, 不是Z/3¹¹Z中的值)
//   >>16: [层0] 模2右移 (二进制截断到2^16位宽)
//
//   此操作在模2硬件上执行, 同时作用在两个维度:
//     ×177147 → 将层1的GF(3) trit信息"泵入"层2的位权空间
//     >>16    → 截断清除模2累加误差, 将结果拉回层1可用的位宽
// ═══════════════════════════════════════════════════════════════════════════

// [层0] [模2十进制] 黄钟 = 3^11在模2下的十进制表示
// 注意: 这不是Z/3¹¹Z中的值! Z/3¹¹Z中3¹¹≡0 (模3¹¹)
//       这是模2硬件用于桥接乘法的操作数
inline constexpr uint64_t HUANGZHONG      = 177147ULL;

// [层0] [模2] 仲吕边界 = 2^16, 二进制位宽截断
inline constexpr uint64_t ZHONGLV_SHIFT    = 16;
inline constexpr uint64_t ZHONGLV_BOUNDARY = 1ULL << ZHONGLV_SHIFT;

// [层0] [模2] LCM = HUANGZHONG × ZHONGLV_BOUNDARY
// 注意: 此值仅用于累加器取模, 保持在2^16位宽内
//       不是"3¹¹×2¹⁶的数学等价" — 是模2运算下的操作参数
inline constexpr uint64_t LCM_TOTAL   = HUANGZHONG * ZHONGLV_BOUNDARY;
inline constexpr uint64_t LCM_MODULUS = LCM_TOTAL;

// ═══════════════════════════════════════════════════════════════════════════
// 层2: Z/3¹¹Z 环 — 模3¹¹运算, 3≠0
// 范畴: 11位3-adic截断环。3不是零元, 3^k有真实的位权意义。
//       这是主权状态机纳音驻波演化的真实数学空间。
//       Tk = 3^k (在模2十进制下的表示, 用于位权计算)
// ═══════════════════════════════════════════════════════════════════════════

// [层2] [Z/3¹¹Z] 位权值 (模2十进制表示)
// 这些数字是3^k在模2运算下的十进制值, 用于层2的基3位置记数
inline constexpr uint64_t Z3R_T0  = 1;       // 3⁰
inline constexpr uint64_t Z3R_T1  = 3;       // 3¹
inline constexpr uint64_t Z3R_T2  = 9;       // 3²
inline constexpr uint64_t Z3R_T3  = 27;      // 3³
inline constexpr uint64_t Z3R_T4  = 81;      // 3⁴
inline constexpr uint64_t Z3R_T5  = 243;     // 3⁵
inline constexpr uint64_t Z3R_T6  = 729;     // 3⁶
inline constexpr uint64_t Z3R_T7  = 2187;    // 3⁷
inline constexpr uint64_t Z3R_T8  = 6561;    // 3⁸
inline constexpr uint64_t Z3R_T9  = 19683;   // 3⁹
inline constexpr uint64_t Z3R_T10 = 59049;   // 3¹⁰
inline constexpr uint64_t Z3R_T11 = 177147;  // 3¹¹ ≡ 0 (mod 3¹¹)

// [层2] [Z/3¹¹Z] 3不是零元验证 (k<11时3^k≢0 mod 3¹¹)
static_assert(Z3R_T1 == 3,   "Z/3¹¹Z: 3¹=3≠0");
static_assert(Z3R_T2 == 9,   "Z/3¹¹Z: 3²=9≠0");
static_assert(Z3R_T3 == 27,  "Z/3¹¹Z: 3³=27≠0");

// ═══════════════════════════════════════════════════════════════════════════
// 层3-4: T⁶环面 — 几何拓扑常数, 模2十进制表示
// 范畴: 结构学。这些数字是不可拆分的拓扑不变量, 非代数因子。
// ═══════════════════════════════════════════════════════════════════════════

// [层4] [拓扑] 极向缠绕步数
inline constexpr int POLAR_WINDING   = 144;

// [层4] [拓扑] 环向缠绕本征模式数 (不可拆分)
inline constexpr int TOROIDAL_WINDING = 46;

// [层4] [拓扑] 大泵步数 = 144×46, 主权状态机完整呼吸周期
inline constexpr int GRAND_PUMP      = 6624;

// [层0] [模2] 微泵步数 = 十二律损益链闭合节拍
inline constexpr int MICRO_PUMP      = 12;

// [层0] [模2] 中泵步数 = 八轮微泵后手性离合器复位
inline constexpr int MID_PUMP        = 96;

// [层4] [拓扑] 不可通约性
static_assert(GRAND_PUMP == POLAR_WINDING * TOROIDAL_WINDING,
              "大泵步数必须等于极向×环向");

// ═══════════════════════════════════════════════════════════════════════════
// 拓扑不变量
// ═══════════════════════════════════════════════════════════════════════════

// [层4] [拓扑] 陈数 C=2 (S²球面欧拉示性数χ=2, 陈数|C|=χ)
inline constexpr int     CHERN_TARGET    = 2;

// [层0] [模2Q16定点] 能隙 Δ=√3, Q16.16定点整数表示
inline constexpr int32_t DELTA_Q16       = 113506;

// ═══════════════════════════════════════════════════════════════════════════
// 层5: 纳音谱系
// ═══════════════════════════════════════════════════════════════════════════

inline constexpr int NAYIN_COUNT     = 60;    // [层5] 六十甲子
inline constexpr int TONE_COUNT      = 12;    // [层5] 十二律
inline constexpr int WUXING_COUNT    = 5;     // [层5] 五行

// ═══════════════════════════════════════════════════════════════════════════
// 层8: 全息静态容器
// ═══════════════════════════════════════════════════════════════════════════

inline constexpr int HOLO_GRID_ROWS   = 144;     // [层8] 幻方行数
inline constexpr int HOLO_GRID_COLS   = 144;     // [层8] 幻方列数
inline constexpr int HOLO_GRID_POINTS = 20736;   // [层8] 全息格点总数

} // namespace sov::math

#endif // SOV_MATH_LCM_CONSTANTS_H
