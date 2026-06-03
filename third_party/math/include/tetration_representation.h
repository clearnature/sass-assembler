// tetration_representation.h — 12阶指数塔的结构化表示
//
// 宪法声明:
//   范畴: 跨层 (L1 GF(3) × L6 仲吕倍频 × L8 全息极限环)
//   原则: 不存储天文数值，存储塔的几何结构
//         GF(3)经验: 存相位不存值 → 指数塔: 存高度×相位不存值
//
//   表示: TetrationNumber = (height, z12_phase, zhonglv_count)
//         ≡ 12 ↑↑ height × exp(2πi × phase/12)
//
//   组合: TetrationNumber × TetrationNumber = 塔高相加 + 相位模12加
//         TetrationNumber ^ n = 塔高×n + 相位×n mod 12
//
//   Z12 环: 步长=12, 每步=仲吕闭合×8级倍增
//   模3: GF(3) {T0,T1,T2} — 底层相位旋转
//   模12: Z12十二律 — 中层时间晶格
//   塔高H: 超运算层级 — 顶层全息投影
#ifndef SOV_MATH_TETRATION_REPRESENTATION_H
#define SOV_MATH_TETRATION_REPRESENTATION_H

#include "lcm_constants.h"
#include "gf3_tetration.h"
#include <cstdint>
#include <cmath>
#include <string>

namespace sov::math::tetration {

// ═══════════════════════════════════════════════════════
// 一、塔数结构 — 不存值, 存几何
// ═══════════════════════════════════════════════════════

struct TetrationNumber {
    double   height;           // 指数塔高度 H (可含小数, 如2.44)
    uint8_t  z12_phase;        // Z12相位 [0,11] — 十二律位置
    uint64_t zhonglv_count;    // 仲吕闭合累计 — 时间戳

    // 默认: H=1 (即12本身)
    constexpr TetrationNumber() noexcept
        : height(1.0), z12_phase(0), zhonglv_count(0) {}

    // 从仲吕闭合构建
    static TetrationNumber from_zhonglv(uint64_t zc) noexcept {
        TetrationNumber tn{};
        tn.zhonglv_count = zc;
        tn.z12_phase = static_cast<uint8_t>(zc % 12);
        tn.height = sov::math::tetration::tower_height_from_zhonglv(zc);
        return tn;
    }

    // ═══════════════════════════════════════
    // 塔数运算 (几何操作, 非数值运算)
    // ═══════════════════════════════════════

    // 塔数相乘 ≡ 高度相加 (因为 12↑↑H1 × 12↑↑H2 ≈ 12↑↑(H1+H2))
    TetrationNumber operator*(const TetrationNumber& other) const noexcept {
        TetrationNumber r{};
        r.height = this->height + other.height;
        r.z12_phase = (this->z12_phase + other.z12_phase) % 12;
        r.zhonglv_count = this->zhonglv_count + other.zhonglv_count;
        return r;
    }

    // 塔数幂 ≡ 高度相乘 (因为 (12↑↑H)^n ≈ 12↑↑(H×n))
    TetrationNumber pow(uint64_t n) const noexcept {
        TetrationNumber r{};
        r.height = this->height * static_cast<double>(n);
        r.z12_phase = static_cast<uint8_t>((this->z12_phase * n) % 12);
        r.zhonglv_count = this->zhonglv_count * n;
        return r;
    }

    // 仲吕步进: 每12步增加一次仲吕闭合
    TetrationNumber zhonglv_step(uint64_t closures = 1) const noexcept {
        TetrationNumber r = *this;
        r.zhonglv_count += closures;
        r.z12_phase = static_cast<uint8_t>(r.zhonglv_count % 12);
        r.height = sov::math::tetration::tower_height_from_zhonglv(r.zhonglv_count);
        return r;
    }
};

// ═══════════════════════════════════════════════════════
// 二、塔数打印 — 人类可读
// ═══════════════════════════════════════════════════════

inline std::string to_string(const TetrationNumber& tn) {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
        "12↑↑%.2f @ Z12[%u] (仲吕=%lu)",
        tn.height, tn.z12_phase, tn.zhonglv_count);
    return std::string(buf);
}

// ═══════════════════════════════════════════════════════
// 三、12^12^12^...^12 递推生成
// ═══════════════════════════════════════════════════════

// 12的12次指数塔: ^1 12, ^2 12, ^3 12, ...
//   层0: 12
//   层1: 12^12 ≈ 10^13
//   层2: 12^(12^12) ≈ 10^(10^13) ← 不可直接计算
//   层n: 存储 (H=n, phase=0)
//
// 连续塔: H=2.44 表示在层2→3之间, 已完成了0.44的过渡

struct TetrationTower {
    static constexpr int BASE = 12;
    static constexpr int Z12_ORDER = 12;

    // 生成整数层塔数: ^n 12
    static TetrationNumber integer_level(int n) noexcept {
        TetrationNumber tn{};
        tn.height = static_cast<double>(n);
        tn.z12_phase = 0;
        tn.zhonglv_count = 0;  // 整数层不需仲吕计数
        return tn;
    }

    // 从当前状态推进: 每12步(Z12一圈) ≈ 1个微泵周期
    static TetrationNumber advance_12(const TetrationNumber& current) noexcept {
        return current.zhonglv_step(1);  // 1次仲吕闭合
    }

    // Z12环上的当前位置: 0=黄钟, 5=蕤宾, 11=仲吕
    static constexpr const char* Z12_NAME[12] = {
        "黄钟","林钟","太簇","南吕","姑洗","应钟",
        "蕤宾","大吕","夷则","夹钟","无射","仲吕"
    };

    static const char* phase_name(const TetrationNumber& tn) noexcept {
        return Z12_NAME[tn.z12_phase % 12];
    }
};

// ═══════════════════════════════════════════════════════
// 四、编译期验证
// ═══════════════════════════════════════════════════════

static_assert(TetrationTower::BASE == 12, "塔基=12律");
static_assert(TetrationTower::Z12_ORDER == 12, "Z12阶=12");

} // namespace sov::math::tetration

#endif // SOV_MATH_TETRATION_REPRESENTATION_H
