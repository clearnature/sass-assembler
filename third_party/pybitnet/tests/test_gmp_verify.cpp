// test_gmp_verify.cpp — GMP黄金标准验证 (金融级精确审计)
// 编译: g++ -std=c++23 -O3 -o test_gmp_verify test_gmp_verify.cpp -I../include -lgmp
// 宪法原则:
//   代数数学 = consteval 编译期真理 (LUT, 常数, 边界检查)
//   量子数学 = 运行时呼吸 (step, zhonglv_closure, a4_flip)
//   GMP mpz_t = GF(3)绝对真理 (逐比特验证, 零浮点)
#include "../include/lcm_constants.h"
#include "../include/gf3_types.h"
#include <gmp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

namespace sm = sov::math;
using sm::HUANGZHONG, sm::ZHONGLV_SHIFT, sm::ZHONGLV_BOUNDARY, sm::LCM_TOTAL;
using sm::MICRO_PUMP, sm::GRAND_PUMP;
using sm::POLAR_WINDING, sm::TOROIDAL_WINDING;
using sm::TRIT_MUL_LUT, sm::TRIT_ADD_SUM, sm::TRIT_ADD_CARRY;

// ============================================================================
// 一、GMP 参考实现: LCM桥接 (黄金标准)
// ============================================================================

// GMP精确计算: bridge = (acc * 3^11) / 2^16
static uint64_t gmp_bridge_forward(uint64_t acc_layer1) {
    mpz_t acc, huangzhong, zhv_boundary, result;
    mpz_inits(acc, huangzhong, zhv_boundary, result, NULL);

    mpz_set_ui(acc, acc_layer1);
    mpz_set_ui(huangzhong, HUANGZHONG);        // 177147
    mpz_set_ui(zhv_boundary, ZHONGLV_BOUNDARY); // 65536

    // bridge = (acc * 177147) / 65536  (精确整数除法, 截断)
    mpz_mul(result, acc, huangzhong);
    mpz_fdiv_q(result, result, zhv_boundary);  // floor division = >> 16

    uint64_t val = mpz_get_ui(result);
    mpz_clears(acc, huangzhong, zhv_boundary, result, NULL);
    return val;
}

// GMP精确计算: acc 经过 N 步 LCM 累加后的值
static uint64_t gmp_accumulate_steps(uint64_t start, const uint8_t* inputs, int n_steps) {
    mpz_t acc, hz, lcm, delta, tmp;
    mpz_inits(acc, hz, lcm, delta, tmp, NULL);

    mpz_set_ui(acc, start);
    mpz_set_ui(hz, HUANGZHONG);
    mpz_set_ui(lcm, LCM_TOTAL);

    for (int i = 0; i < n_steps; ++i) {
        // acc = (acc * 177147 + input) % LCM_TOTAL
        mpz_mul(acc, acc, hz);
        mpz_set_ui(delta, inputs[i]);
        mpz_add(acc, acc, delta);
        mpz_mod(acc, acc, lcm);
    }

    uint64_t val = mpz_get_ui(acc);
    mpz_clears(acc, hz, lcm, delta, tmp, NULL);
    return val;
}

// GMP: (acc * 177147) >> 16 的精确GF(3)值
static uint8_t gmp_gf3_forward(uint64_t acc_layer1) {
    uint64_t bridged = gmp_bridge_forward(acc_layer1);
    return (uint8_t)(bridged % 3);
}

// ============================================================================
// 二、GF(3) 乘法表验证 (GMP vs consteval LUT)
// ============================================================================

static int verify_mul_table() {
    int errors = 0;
    printf("══════════ GF(3)乘法表验证 (GMP vs consteval LUT) ══════════\n");

    for (uint8_t a = 0; a < 3; ++a) {
        for (uint8_t b = 0; b < 3; ++b) {
            // GMP精确: (a * b) % 3
            mpz_t ma, mb, m3, prod, result;
            mpz_inits(ma, mb, m3, prod, result, NULL);
            mpz_set_ui(ma, a);
            mpz_set_ui(mb, b);
            mpz_set_ui(m3, 3);
            mpz_mul(prod, ma, mb);
            mpz_mod(result, prod, m3);
            uint8_t expected = (uint8_t)mpz_get_ui(result);
            mpz_clears(ma, mb, m3, prod, result, NULL);

            // consteval LUT
            uint8_t actual = TRIT_MUL_LUT[a][b];

            if (expected != actual) {
                printf("  ❌ T%u⊗T%u: LUT=%u GMP=%u\n", a, b, actual, expected);
                errors++;
            }
        }
    }

    printf("  T2⊗T2 = T1: LUT=%u GMP=%u  %s\n",
        TRIT_MUL_LUT[2][2], (uint8_t)((2*2)%3),
        TRIT_MUL_LUT[2][2] == (2*2)%3 ? "✅" : "❌");
    printf("  总错误: %d  %s\n\n", errors, errors == 0 ? "✅ GF(3)乘法表宪法认证" : "❌ 违宪");
    return errors;
}

// ============================================================================
// 三、LCM桥接验证 (GMP 黄金标准 vs uint64_t 实现)
// ============================================================================

static int verify_bridge() {
    int errors = 0;
    printf("══════════ LCM桥接验证 (GMP vs uint64_t) ══════════\n");

    constexpr int TEST_POINTS = 100;
    printf("  测试点: %d (均匀覆盖 0 .. LCM-1)\n", TEST_POINTS);

    for (int i = 0; i < TEST_POINTS; ++i) {
        // 均匀抽样 LCM 空间
        uint64_t acc = (i == 0) ? 0 : (LCM_TOTAL / TEST_POINTS) * i;

        // uint64_t 实现
        uint64_t our_result = (acc * HUANGZHONG) >> ZHONGLV_SHIFT;

        // GMP 黄金标准
        uint64_t gmp_result = gmp_bridge_forward(acc);

        if (our_result != gmp_result) {
            printf("  ❌ acc=%lu: 我们=%lu GMP=%lu (差=%ld)\n",
                acc, our_result, gmp_result,
                (int64_t)(our_result - gmp_result));
            errors++;
        }
    }

    // 边界测试: 0, 1, 2, LCM-2, LCM-1
    uint64_t edge[] = {0, 1, 2, LCM_TOTAL - 2, LCM_TOTAL - 1,
                       ZHONGLV_BOUNDARY - 1, ZHONGLV_BOUNDARY,
                       ZHONGLV_BOUNDARY + 1, HUANGZHONG - 1, HUANGZHONG,
                       HUANGZHONG + 1, LCM_TOTAL / 2};
    for (auto acc : edge) {
        uint64_t our_result = (acc * HUANGZHONG) >> ZHONGLV_SHIFT;
        uint64_t gmp_result = gmp_bridge_forward(acc);
        if (our_result != gmp_result) {
            printf("  ❌ 边界 acc=%lu: 我们=%lu GMP=%lu\n", acc, our_result, gmp_result);
            errors++;
        }
    }
    printf("  边界测试: %zu 点\n", sizeof(edge)/sizeof(edge[0]));

    printf("  总错误: %d  %s\n\n", errors, errors == 0 ? "✅ LCM桥接宪法认证" : "❌ 逐比特违宪");
    return errors;
}

// ============================================================================
// 四、仲吕闭合验证 (12步微泵)
// ============================================================================

static int verify_zhonglv_closure() {
    int errors = 0;
    printf("══════════ 仲吕闭合验证 (12步微泵, GMP vs 实现) ══════════\n");

    constexpr int CYCLES = 100;
    uint64_t rng = 0xDEADBEEF;

    for (int cycle = 0; cycle < CYCLES; ++cycle) {
        uint64_t acc = (rng = rng * 1103515245 + 12345) % LCM_TOTAL;

        // 我们: 12步累积后执行仲吕闭合
        uint64_t our_acc = acc;
        for (int s = 0; s < MICRO_PUMP; ++s) {
            our_acc = (our_acc * HUANGZHONG + (s % 3)) % LCM_TOTAL;
        }
        our_acc = (our_acc * HUANGZHONG) >> ZHONGLV_SHIFT;  // 仲吕闭合

        // GMP: 精确12步累积后 >> 16
        mpz_t gmp_acc, hz, lcm, delta, tmp;
        mpz_inits(gmp_acc, hz, lcm, delta, tmp, NULL);
        mpz_set_ui(gmp_acc, acc);
        mpz_set_ui(hz, HUANGZHONG);
        mpz_set_ui(lcm, LCM_TOTAL);

        for (int s = 0; s < MICRO_PUMP; ++s) {
            mpz_mul(gmp_acc, gmp_acc, hz);
            mpz_set_ui(delta, s % 3);
            mpz_add(gmp_acc, gmp_acc, delta);
            mpz_mod(gmp_acc, gmp_acc, lcm);
        }
        mpz_mul(gmp_acc, gmp_acc, hz);
        mpz_set_ui(tmp, ZHONGLV_BOUNDARY);
        mpz_fdiv_q(gmp_acc, gmp_acc, tmp);
        uint64_t gmp_result = mpz_get_ui(gmp_acc);
        mpz_clears(gmp_acc, hz, lcm, delta, tmp, NULL);

        if (our_acc != gmp_result) {
            if (errors < 3)  // 只打印前3个
                printf("  ❌ 周期%d: 我们=%lu GMP=%lu\n", cycle, our_acc, gmp_result);
            errors++;
        }
    }

    printf("  测试周期: %d\n", CYCLES);
    printf("  总错误: %d  %s\n\n", errors, errors == 0 ? "✅ 仲吕闭合宪法认证" : "❌ 违宪");
    return errors;
}

// ============================================================================
// 五、GF(3) 加法进位验证 (逢三进一 vs 逢二进一)
// ============================================================================

static int verify_carry() {
    int errors = 0;
    printf("══════════ GF(3)加法进位验证 (逢三进一) ══════════\n");

    // 关键测试: T2+T1 = 3 → sum=T0, carry=1 (逢三进一, 不是逢二进一!)
    struct { uint8_t a, b, expected_sum, expected_carry; } tests[] = {
        {0, 0, 0, 0},  // T0+T0 = T0
        {0, 1, 1, 0},  // T0+T1 = T1
        {1, 1, 2, 0},  // T1+T1 = T2
        {2, 1, 0, 1},  // T2+T1 = T0 进位1 ← 逢三进一!
        {2, 2, 1, 1},  // T2+T2 = T1 进位1 ← 2+2=4=1×3+1
        {2, 2, 1, 1},  // 冗余验证
    };

    for (auto& t : tests) {
        uint8_t sum   = TRIT_ADD_SUM[t.a][t.b];
        uint8_t carry = TRIT_ADD_CARRY[t.a][t.b];

        bool sum_ok   = (sum   == t.expected_sum);
        bool carry_ok = (carry == t.expected_carry);

        if (!sum_ok || !carry_ok) {
            printf("  ❌ T%u+T%u: sum=%u(exp=%u) carry=%u(exp=%u)\n",
                t.a, t.b, sum, t.expected_sum, carry, t.expected_carry);
            errors++;
        }

        // GMP 验证
        mpz_t ma, mb, m3, total, msum, mcarry;
        mpz_inits(ma, mb, m3, total, msum, mcarry, NULL);
        mpz_set_ui(ma, t.a);
        mpz_set_ui(mb, t.b);
        mpz_set_ui(m3, 3);
        mpz_add(total, ma, mb);           // total = a + b
        mpz_mod(msum, total, m3);         // sum = total % 3
        mpz_fdiv_q(mcarry, total, m3);    // carry = total / 3
        uint8_t gmp_sum   = (uint8_t)mpz_get_ui(msum);
        uint8_t gmp_carry = (uint8_t)mpz_get_ui(mcarry);
        mpz_clears(ma, mb, m3, total, msum, mcarry, NULL);

        if (sum != gmp_sum || carry != gmp_carry) {
            printf("  ❌ GMP不匹配 T%u+T%u: sum LUT=%u GMP=%u carry LUT=%u GMP=%u\n",
                t.a, t.b, sum, gmp_sum, carry, gmp_carry);
            errors++;
        }
    }

    printf("  逢三进一: T2+T1=3→sum=0 carry=1  %s\n",
        TRIT_ADD_SUM[2][1] == 0 && TRIT_ADD_CARRY[2][1] == 1 ? "✅" : "❌");
    printf("  逢三进一: T2+T2=4→sum=1 carry=1  %s\n",
        TRIT_ADD_SUM[2][2] == 1 && TRIT_ADD_CARRY[2][2] == 1 ? "✅" : "❌");
    printf("  总错误: %d  %s\n\n", errors,
        errors == 0 ? "✅ GF(3)进位宪法认证 (逢三进一, 非逢二进一)" : "❌ 违宪");
    return errors;
}

// ============================================================================
// 六、大泵周期验证 (144×46=6624, 环向46不可拆分根数学常数)
// ============================================================================

static int verify_grand_pump() {
    printf("══════════ 大泵周期验证 144×46=6624 ══════════\n");

    // GMP 精确计算乘积
    mpz_t m144, m46, product;
    mpz_inits(m144, m46, product, NULL);
    mpz_set_ui(m144, POLAR_WINDING);
    mpz_set_ui(m46, TOROIDAL_WINDING);
    mpz_mul(product, m144, m46);
    unsigned long prod_val = mpz_get_ui(product);
    mpz_clears(m144, m46, product, NULL);

    printf("  极向缠绕 = 144 (损益链完整展开)\n");
    printf("  环向缠绕 = 46  (不可拆分根数学常数, 非代数因子2×23)\n");
    printf("  144 × 46  = %lu (期望: 6624)\n", prod_val);

    bool ok = (prod_val == 6624) && (GRAND_PUMP == 6624);
    printf("  大泵=6624:  %s\n", ok ? "✅" : "❌");
    printf("  不可通约性: 144与46的不可通约正是仲吕闭合介入的拓扑根源\n\n");

    return ok ? 0 : 1;
}

// ============================================================================
// 七、量化精度验证 (729 tryte态)
// ============================================================================

static int verify_tryte_precision() {
    printf("══════════ Tryte量化精度验证 (3^6=729态) ══════════\n");

    mpz_t m3, m729, result;
    mpz_inits(m3, m729, result, NULL);
    mpz_set_ui(m3, 3);
    mpz_pow_ui(m729, m3, 6);  // 3^6

    unsigned long tryte_states = mpz_get_ui(m729);
    mpz_clears(m3, m729, result, NULL);

    printf("  3^6 = %lu (期望: 729)\n", tryte_states);
    printf("  729态 → 60纳音: 压缩比 %.2f×\n", 729.0 / 60.0);
    printf("  精度: log2(729/60) = %.2f bit\n", log2(729.0/60.0));
    printf("  %s\n\n", tryte_states == 729 ? "✅ 宪法认证" : "❌");

    return tryte_states == 729 ? 0 : 1;
}

// ============================================================================
// 主入口
// ============================================================================

int main() {
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  律算宪法: GMP黄金标准验证 v2.5                     ║\n");
    printf("║  GMP mpz_t = GF(3)绝对真理, 逐比特审计              ║\n");
    printf("║  代数数学(consteval LUT) vs GMP(黄金标准)            ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    int errors = 0;

    errors += verify_mul_table();        // 一
    errors += verify_carry();            // 二
    errors += verify_bridge();           // 三
    errors += verify_zhonglv_closure();  // 四
    errors += verify_grand_pump();       // 五
    errors += verify_tryte_precision();  // 六

    printf("══════════════════════════════════════════════════════\n");
    if (errors == 0) {
        printf("  最终裁定: ✅ GMP黄金标准验证全部通过\n");
        printf("  consteval LUT ≡ GMP mpz_t 逐比特一致\n");
        printf("  主权状态机 GF(3) 数学库获金融级精确认证\n");
    } else {
        printf("  最终裁定: ❌ 发现 %d 处违宪错误\n", errors);
        printf("  consteval LUT 与 GMP 黄金标准不一致, 必须修正\n");
    }
    printf("══════════════════════════════════════════════════════\n");

    return errors;
}
