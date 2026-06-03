/* ============================================================================
 * HunTian SASS 汇编器 - 三进制包装层
 *
 * 直接使用 third_party/math/ 的 sov::math 命名空间，
 * 不重新实现任何算法。
 * ============================================================================ */

#ifndef SASS_TERNARY_WRAPPER_H
#define SASS_TERNARY_WRAPPER_H

#include "instruction.h"  // sass::Trit, sass::TRIT_NEG/ZERO/POS
#include <cstdint>

// third_party/math/ 的完整数学库（无宏冲突、scoped enum）
#include "gf3_field.h"
#include "gf3_layer1.h"
#include "gf3_layer2.h"
#include "lcm_bridge.h"

namespace sass {

// ─── GF(3) 运算 ───
inline Trit trit_add(Trit a, Trit b) {
    uint8_t r = (sov::math::trit_val(a) + sov::math::trit_val(b)) % 3;
    return static_cast<Trit>(r);
}

inline Trit trit_mul(Trit a, Trit b) {
    uint8_t r = (sov::math::trit_val(a) * sov::math::trit_val(b)) % 3;
    return static_cast<Trit>(r);
}

inline int gf3_add(int a, int b) { return (a + b) % 3; }
inline int gf3_mul(int a, int b) { return (a * b) % 3; }

} // namespace sass

#endif // SASS_TERNARY_WRAPPER_H
