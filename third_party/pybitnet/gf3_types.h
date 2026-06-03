// gf3_types.h — 律算核心类型, 每行标注层级+范畴+模
//
// 宪法声明:
//   层1 GF(3) 有限域: 模3, 3≡0, 每trit独立, enum class Trit{T0,T1,T2}
//   层2 Z/3¹¹Z 环: 模3¹¹, 3≠0, 位权重3^k, TryteValue
//   层0 桥接存储: SovBlock128 (模2硬件上的字节容器)
//   C++23, 纯整数域, 零浮点
#ifndef SOV_MATH_GF3_TYPES_H
#define SOV_MATH_GF3_TYPES_H

#include "lcm_constants.h"
#include "sov_format.h"  // [v2.6] LayerTag, SovTQT0Block128, SovBlockDescriptor
#include <cstdint>
#include <array>
#include <utility>
#include <string>
#include <vector>
#include <bit>
#include <cassert>
#include <span>
#include <cmath>

namespace sov::math {

// ═══════════════════════════════════════════════════════════════════════════
// 层1: GF(3) 有限域 — 模3, 3≡0
// ═══════════════════════════════════════════════════════════════════════════

// [层1] [GF(3)模3] 三值枚举, uint8底层存储
enum class Trit : uint8_t {
    T0 = 0,  // [层1] [GF(3)] 加法单位元, 乘法吸收元
    T1 = 1,  // [层1] [GF(3)] 乘法单位元, 恒等元
    T2 = 2,  // [层1] [GF(3)] T2⊗T2=T1 (2×2≡1 mod 3)
};

// [层1] [GF(3)] 安全转换: 枚举→uint8
[[nodiscard]] constexpr uint8_t trit_val(Trit t) noexcept {
    return std::to_underlying(t);
}

// [层1] [GF(3)] 安全转换: uint8→枚举
[[nodiscard]] constexpr Trit trit_from_val(uint8_t v) noexcept {
    return static_cast<Trit>(v);
}

// [层1] [GF(3)] 编译期合法性检查
consteval bool is_valid_trit(uint8_t v) { return v <= 2; }
static_assert(is_valid_trit(0) && is_valid_trit(1) && is_valid_trit(2));

// ═══════════════════════════════════════════════════════════════════════════
// 层1 GF(3) 预编译LUT — consteval, 编译期生成, 零运行时开销
// 所有LUT在模3下计算 (层1 GF(3)有限域)
// ═══════════════════════════════════════════════════════════════════════════

// [层1] [GF(3)模3] 加法表: sum=(a+b)%3, carry=(a+b)/3 (逢三进一)
consteval auto generate_trit_add_lut() {
    std::array<std::array<uint8_t, 3>, 3> sum{};
    std::array<std::array<uint8_t, 3>, 3> carry{};
    for (uint8_t a = 0; a < 3; ++a) {
        for (uint8_t b = 0; b < 3; ++b) {
            int total = (int)a + (int)b;      // [层0] 模2整数加法
            sum[a][b]   = (uint8_t)(total % 3);   // [层1] 模3归约→本位
            carry[a][b] = (uint8_t)(total / 3);   // [层1] 逢三进一→进位
        }
    }
    return std::pair{sum, carry};
}
inline constexpr auto TRIT_ADD_LUT = generate_trit_add_lut();
inline constexpr auto& TRIT_ADD_SUM   = TRIT_ADD_LUT.first;   // [层1] [模3] 本位
inline constexpr auto& TRIT_ADD_CARRY = TRIT_ADD_LUT.second;  // [层1] [模3] 进位

// [层1] [GF(3)模3] 乘法表: (a×b)%3, 2×2=4≡1 mod 3
consteval auto generate_trit_mul_lut() {
    std::array<std::array<uint8_t, 3>, 3> lut{};
    for (uint8_t a = 0; a < 3; ++a)
        for (uint8_t b = 0; b < 3; ++b)
            lut[a][b] = (uint8_t)((a * b) % 3);  // [层1] 模3乘法
    return lut;
}
inline constexpr auto TRIT_MUL_LUT = generate_trit_mul_lut();

// [层1] [GF(3)模3] 范数表: |0|=0, |1|=1, |2|=1 (2²=4≡1 mod 3)
consteval auto generate_trit_norm_lut() {
    std::array<uint8_t, 3> lut{};
    for (uint8_t a = 0; a < 3; ++a)
        lut[a] = (a == 0) ? 0 : 1;
    return lut;
}
inline constexpr auto TRIT_NORM_LUT = generate_trit_norm_lut();

// ═══════════════════════════════════════════════════════════════════════════
// 层1→层0 编码LUT — 5 trit/byte 打包 (模2运算下的编码)
// 这些LUT在模2下完成 GF(3)→uint8 的编码/解码
// ═══════════════════════════════════════════════════════════════════════════

// [层0] [模2编码] 打包: 3^5=243种组合→1字节
consteval auto generate_pack_5_lut() {
    std::array<uint8_t, 243> lut{};
    for (uint16_t i = 0; i < 243; ++i) {
        uint16_t v = i;
        uint8_t t0 = v % 3; v /= 3;  // [层0] 模2除法
        uint8_t t1 = v % 3; v /= 3;
        uint8_t t2 = v % 3; v /= 3;
        uint8_t t3 = v % 3; v /= 3;
        uint8_t t4 = v % 3;
        lut[i] = t0 + t1*3 + t2*9 + t3*27 + t4*81;  // [层0] 模2乘法+加法
    }
    return lut;
}
inline constexpr auto PACK_5_LUT = generate_pack_5_lut();

// [层0] [模2解码] 解包: 1字节→5 trit
consteval auto generate_unpack_5_lut() {
    std::array<std::array<uint8_t, 5>, 256> lut{};
    for (uint16_t b = 0; b < 256; ++b) {
        uint8_t v = (uint8_t)b;
        lut[b][0] = v / 81; v %= 81;  // [层0] 模2除法 (高位优先)
        lut[b][1] = v / 27; v %= 27;
        lut[b][2] = v / 9;  v %= 9;
        lut[b][3] = v / 3;  v %= 3;
        lut[b][4] = v;
    }
    return lut;
}
inline constexpr auto UNPACK_5_LUT = generate_unpack_5_lut();

// [层0] [模2Q16] rsqrt LUT — Q16定点, 编译期预计算
template<int DIM>
consteval auto generate_rsqrt_q16_lut(double eps = 1e-5) {
    std::array<int32_t, DIM + 1> lut{};
    for (int m = 0; m <= DIM; ++m) {
        double mean = (double)m / (double)DIM;
        double val = 1.0 / std::sqrt(mean + eps);       // [层0] 编译期double (不进入运行时)
        lut[m] = (int32_t)(val * (double)ZHONGLV_BOUNDARY + 0.5);  // [层0] Q16定点截断
    }
    return lut;
}

// [层1] [GF(3)Q16] 能隙检查LUT: |H|² > √3 → 预判所有范数值
consteval auto generate_delta_check_lut() {
    std::array<bool, 4096> lut{};
    for (int nz = 0; nz < 4096; ++nz) {
        lut[nz] = ((int64_t)nz * ZHONGLV_BOUNDARY) > DELTA_Q16;  // [层0] 模2比较
    }
    return lut;
}
inline constexpr auto DELTA_CHECK_LUT = generate_delta_check_lut();

// ═══════════════════════════════════════════════════════════════════════════
// 层0 多维视图 — 模2内存上的多维格点映射
// ═══════════════════════════════════════════════════════════════════════════

// [层0] [模2] 二维视图 (将一维内存视为 R×C 格点)
template<typename T>
class mdspan_2d {
    T* data_;
    size_t rows_, cols_;
public:
    constexpr mdspan_2d(T* data, size_t rows, size_t cols) noexcept
        : data_(data), rows_(rows), cols_(cols) {}

    [[nodiscard]] constexpr T* operator[](size_t r) noexcept { return data_ + r * cols_; }
    [[nodiscard]] constexpr const T* operator[](size_t r) const noexcept { return data_ + r * cols_; }
    [[nodiscard]] constexpr size_t extent(size_t dim) const noexcept {
        return dim == 0 ? rows_ : cols_;
    }
    [[nodiscard]] constexpr T* data() noexcept { return data_; }
    [[nodiscard]] constexpr size_t size() const noexcept { return rows_ * cols_; }
};

// [层0] [模2] 三维视图
template<typename T>
class mdspan_3d {
    T* data_;
    size_t d0_, d1_, d2_;
    size_t stride1_, stride2_;
public:
    constexpr mdspan_3d(T* data, size_t d0, size_t d1, size_t d2) noexcept
        : data_(data), d0_(d0), d1_(d1), d2_(d2)
        , stride1_(d1 * d2), stride2_(d2) {}

    [[nodiscard]] constexpr T* offset(size_t i, size_t j) noexcept {
        return data_ + i * stride1_ + j * stride2_;
    }
    [[nodiscard]] constexpr size_t extent(size_t dim) const noexcept {
        return dim == 0 ? d0_ : (dim == 1 ? d1_ : d2_);
    }
    [[nodiscard]] constexpr T* data() noexcept { return data_; }
    [[nodiscard]] constexpr size_t size() const noexcept { return d0_ * d1_ * d2_; }
};

// ═══════════════════════════════════════════════════════════════════════════
// 层2: Z/3¹¹Z 环 — 模3¹¹, 3≠0
// ═══════════════════════════════════════════════════════════════════════════

// [层2] [Z/3¹¹Z] Tryte = 6位基3数 (3⁶=729态), 前向声明
struct TryteValue;

// ═══════════════════════════════════════════════════════════════════════════
// 层0 桥接存储: SovBlock128 — 模2硬件上的16字节主权块
// ═══════════════════════════════════════════════════════════════════════════

// [层0] [模2] 128位对齐主权块 (AVX2加载/存储)
struct alignas(16) SovBlock128 {
    uint8_t qs[6];           // [层0] [模2] 6字节存储30 trit (5 trit/byte ×6)
    uint8_t scale_ue8m0;     // [层0] [模2] UE8M0尺度指数
    uint8_t phase_bias;      // [层4] [拓扑] 高4位=十二律相位, 低4位=C3内部相位
    uint8_t chern_guard;     // [层4] [拓扑] 高3位=七阶段阶位, 低5位=局部陈数
    uint8_t wuxing_mask;     // [层3] [五行] 高5位=球谐方向, 低3位=A4生成元
    uint8_t reserved[6];     // [层0] [模2] 对齐填充

    // [层1→层2] 从qs解包trit (层0模2解码→层1 GF(3)值)
    [[nodiscard]] constexpr std::array<Trit, 6> get_trits() const noexcept {
        auto u5_0 = UNPACK_5_LUT[qs[0]];  // [层0] 模2解码
        auto u5_1 = UNPACK_5_LUT[qs[1]];
        return {
            static_cast<Trit>(u5_0[0]), static_cast<Trit>(u5_0[1]),
            static_cast<Trit>(u5_0[2]), static_cast<Trit>(u5_0[3]),
            static_cast<Trit>(u5_0[4]), static_cast<Trit>(u5_1[0]),
        };
    }

    // [层2] [Z/3¹¹Z] 计算tryte值 (基3位置记数, 模2运算)
    [[nodiscard]] constexpr TryteValue get_tryte_value() const noexcept;
};

// [层0] 编译期结构验证
static_assert(sizeof(SovBlock128) == 16,   "SovBlock128 = 16字节");
static_assert(alignof(SovBlock128) == 16,  "SovBlock128 16字节对齐");

// ═══════════════════════════════════════════════════════════════════════════
// 层2: TryteValue — Z/3¹¹Z 环中的6位基3数
// ═══════════════════════════════════════════════════════════════════════════

struct TryteValue {
    uint16_t value;  // [层0] [模2] uint16存储, 值域[0,728]

    constexpr explicit TryteValue(uint16_t v = 0) noexcept : value(v) {}

    // [层2] [Z/3¹¹Z] 范围验证
    static constexpr bool is_valid(uint16_t v) noexcept { return v < TRYTE_MAX_VALUE; }
    static_assert(TRYTE_MAX_VALUE == 729, "3^6=729");

    // [层5] [纳音] 模60→六十甲子标签
    [[nodiscard]] constexpr uint8_t nayin_label() const noexcept {
        return (uint8_t)(value % NAYIN_COUNT);  // [层0] 模2模60
    }

    // [层5] [纳音] 模12→十二律偏移
    [[nodiscard]] constexpr uint8_t semitone() const noexcept {
        return (uint8_t)(value % TONE_COUNT);   // [层0] 模2模12
    }
};

// [层2] [Z/3¹¹Z] SovBlock128 → TryteValue (基3位权展开, 模2运算)
inline constexpr TryteValue SovBlock128::get_tryte_value() const noexcept {
    auto t = get_trits();
    return TryteValue{(uint16_t)(
        trit_val(t[0]) * 1      // [层2] 3⁰位
      + trit_val(t[1]) * 3      // [层2] 3¹位
      + trit_val(t[2]) * 9      // [层2] 3²位
      + trit_val(t[3]) * 27     // [层2] 3³位
      + trit_val(t[4]) * 81     // [层2] 3⁴位
      + trit_val(t[5]) * 243    // [层2] 3⁵位
    )};
}

// ═══════════════════════════════════════════════════════════════════════════
// 层4: 环面格点视图 — T⁶: 144×46
// ═══════════════════════════════════════════════════════════════════════════

// [层4] [拓扑] T⁶环面 = 极向144 × 环向46
using TorusGrid = mdspan_2d<uint8_t>;

[[nodiscard]] inline TorusGrid make_torus_grid(
    uint8_t* data, int polar = POLAR_WINDING, int toroidal = TOROIDAL_WINDING
) noexcept {
    return TorusGrid(data, polar, toroidal);
}

// [层4] [拓扑] S²胞腔格点 = 12 × 12
using S2CellGrid = mdspan_2d<uint8_t>;

[[nodiscard]] inline S2CellGrid make_s2_grid(uint8_t* data) noexcept {
    return S2CellGrid(data, 12, 12);
}

// ═══════════════════════════════════════════════════════════════════════════
// [v2.6] SovTensor — 带范畴标签的张量容器
// ═══════════════════════════════════════════════════════════════════════════
// 每个张量必须声明其所属的代数域 (LayerTag)。
// 加载时据此将数据路由到 L1(GF3量子态)、L2(Z/3¹¹Z环) 或 L3(手性离合)。
// 违反范畴隔离的张量操作将被陈数守卫检测并拒绝。

struct SovTensor {
    std::string name;                        // 张量名称 (如 "layers.0.christoffel.weight")
    std::vector<uint32_t> shape;             // 拓扑维度 [N, C, H, W]
    sov::io::LayerTag tag;                   // [v2.6] 范畴标签 — 绝对不可跨域操作
    std::vector<uint8_t> data;               // Payload (TQT_0 128-bit 对齐)

    SovTensor() : tag(sov::io::LayerTag::TAG_L1_GF3_TENSOR) {}

    SovTensor(std::string n, std::vector<uint32_t> s,
              sov::io::LayerTag t = sov::io::LayerTag::TAG_L1_GF3_TENSOR)
        : name(std::move(n)), shape(std::move(s)), tag(t) {}

    // 从 BlockDescriptor 构造
    explicit SovTensor(const sov::io::SovBlockDescriptor& desc)
        : name(desc.name_len, '\0')
        , shape{desc.shape[0], desc.shape[1], desc.shape[2], desc.shape[3]}
        , tag(desc.layer_tag)
        , data(desc.byte_size, 0) {}

    // 生成 BlockDescriptor
    [[nodiscard]] sov::io::SovBlockDescriptor descriptor() const noexcept {
        sov::io::SovBlockDescriptor desc{};
        desc.layer_tag = this->tag;
        desc.name_len  = static_cast<uint32_t>(this->name.size());
        desc.shape[0]  = shape.size() > 0 ? shape[0] : 0;
        desc.shape[1]  = shape.size() > 1 ? shape[1] : 0;
        desc.shape[2]  = shape.size() > 2 ? shape[2] : 0;
        desc.shape[3]  = shape.size() > 3 ? shape[3] : 0;
        desc.byte_size = static_cast<uint64_t>(this->data.size());
        return desc;
    }

    // Payload 须为 16 的倍数 (满足 TQT_0 128-bit 对齐)
    [[nodiscard]] bool is_payload_aligned() const noexcept {
        return (data.size() % 16) == 0;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// 编译期类型验证
// ═══════════════════════════════════════════════════════════════════════════

// [层1] [GF(3)] Trit枚举值
static_assert(trit_val(Trit::T0) == 0);
static_assert(trit_val(Trit::T1) == 1);
static_assert(trit_val(Trit::T2) == 2);

// [层1] [GF(3)模3] 加法进位
static_assert(TRIT_ADD_SUM[2][1] == 0);     // 2+1=3→本位0
static_assert(TRIT_ADD_CARRY[2][1] == 1);   // 2+1=3→进位1
static_assert(TRIT_ADD_SUM[2][2] == 1);     // 2+2=4→本位1
static_assert(TRIT_ADD_CARRY[2][2] == 1);   // 2+2=4→进位1

// [层1] [GF(3)模3] 乘法
static_assert(TRIT_MUL_LUT[2][2] == 1);  // 2×2≡1 mod 3
static_assert(TRIT_MUL_LUT[1][0] == 0);  // 1×0=0
static_assert(TRIT_MUL_LUT[2][1] == 2);  // 2×1=2

// [层2] [Z/3¹¹Z] Tryte范围
static_assert(TryteValue::is_valid(0) && TryteValue::is_valid(728));
static_assert(!TryteValue::is_valid(729));

} // namespace sov::math

#endif // SOV_MATH_GF3_TYPES_H
