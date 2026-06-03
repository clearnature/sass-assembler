/* ============================================================================
 * HunTian SASS Assembler — ManifoldScheduler 单元测试 (红灯)
 *
 * 测试 4320D 流形调度的确定性行为和输出特性。
 * 使用 void_spin_4320 是可确定性计算的，因此输出可验证。
 * ============================================================================ */

#include "test_common.h"
#include "sass/sass_types.h"
#include "sass/instruction.h"
#include <span>
#include <cmath>

using namespace sass;

static Instruction make_ffma(uint8_t dst, uint8_t src1, uint8_t src2) {
    return Instruction::CreateRegOp(Opcode::FFMA, dst, src1, src2);
}

// ═══════════════════════════════════════════════════════════════
// 1. 基本属性
// ═══════════════════════════════════════════════════════════════

TEST(scheduler_empty_block) {
    ManifoldScheduler sched;
    std::vector<Instruction> empty;
    auto result = sched.optimize(empty);

    ASSERT_EQ(result.optimized_sequence.size(), 0);
    ASSERT_TRUE(std::isnan(result.bubble_score) || result.bubble_score >= 0.0);
    return true;
}

TEST(scheduler_single_instruction) {
    ManifoldScheduler sched;
    std::vector<Instruction> insts = { make_ffma(0, 1, 2) };
    auto result = sched.optimize(insts);

    ASSERT_EQ(result.optimized_sequence.size(), 1);
    // 单条指令应该原样返回
    ASSERT_EQ(result.optimized_sequence[0].opcode, Opcode::FFMA);
    return true;
}

TEST(scheduler_preserves_all_instructions) {
    ManifoldScheduler sched;
    std::vector<Instruction> insts;
    for (int i = 0; i < 10; ++i)
        insts.push_back(make_ffma(i, i+1, i+2));

    auto result = sched.optimize(insts);

    ASSERT_EQ(result.optimized_sequence.size(), 10);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 2. 确定性
// ═══════════════════════════════════════════════════════════════

TEST(scheduler_deterministic) {
    // 两次相同输入 → 相同输出
    std::vector<Instruction> insts;
    insts.push_back(make_ffma(0, 1, 2));
    insts.push_back(make_ffma(3, 4, 5));
    insts.push_back(make_ffma(6, 7, 8));

    ManifoldScheduler sched1;
    auto r1 = sched1.optimize(insts);

    ManifoldScheduler sched2;
    auto r2 = sched2.optimize(insts);

    ASSERT_EQ(r1.optimized_sequence.size(), r2.optimized_sequence.size());
    for (size_t i = 0; i < r1.optimized_sequence.size(); ++i) {
        ASSERT_EQ(r1.optimized_sequence[i].opcode, r2.optimized_sequence[i].opcode);
        ASSERT_EQ(r1.optimized_sequence[i].scheduled_cycle,
                  r2.optimized_sequence[i].scheduled_cycle);
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 3. 调度周期分配
// ═══════════════════════════════════════════════════════════════

TEST(scheduler_cycles_assigned) {
    ManifoldScheduler sched;
    std::vector<Instruction> insts;
    for (int i = 0; i < 20; ++i)
        insts.push_back(make_ffma(i, i+1, i+2));

    auto result = sched.optimize(insts);

    for (auto& inst : result.optimized_sequence) {
        // scheduled_cycle 应该被设置 (slot / 64)
        ASSERT_TRUE(inst.scheduled_cycle < 68);  // 4320/64 ≈ 67.5
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 4. bubble_score
// ═══════════════════════════════════════════════════════════════

TEST(scheduler_bubble_score_range) {
    ManifoldScheduler sched;
    std::vector<Instruction> insts;
    for (int i = 0; i < 8; ++i)
        insts.push_back(make_ffma(i, i+1, i+2));

    auto result = sched.optimize(insts);

    // bubble_score 应 ≥ 0
    ASSERT_TRUE(result.bubble_score >= 0.0);
    return true;
}

TEST(scheduler_block_id_increments) {
    ManifoldScheduler sched;
    std::vector<Instruction> insts = { make_ffma(0, 1, 2) };

    auto r1 = sched.optimize(insts);
    auto r2 = sched.optimize(insts);

    ASSERT_EQ(r2.block_id, r1.block_id + 1);
    return true;
}

int main() {
    return run_all_tests();
}
