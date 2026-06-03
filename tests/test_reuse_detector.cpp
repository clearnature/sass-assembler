/* ============================================================================
 * HunTian SASS Assembler — ReuseDetector 单元测试 (红灯)
 *
 * 验证 auto_insert_reuse() 的寄存器复用检测逻辑：
 *   如果源寄存器在后续 4 条指令内被复用，设置 .reuse 标志。
 * ============================================================================ */

#include "test_common.h"
#include "sass/reuse_detector.h"
#include "sass/instruction.h"

using namespace sass;

// ─── 辅助：创建简单的三操作数寄存器指令 ───
static Instruction make_inst(Opcode op, uint8_t dst, uint8_t src1, uint8_t src2) {
    return Instruction::CreateRegOp(op, dst, src1, src2);
}

// ═══════════════════════════════════════════════════════════════
// 1. 基本复用模式
// ═══════════════════════════════════════════════════════════════

TEST(reuse_detected_immediate_next) {
    // R1 在指令 [i] 和 [i+1] 中都是源 → 应标记 .reuse
    std::vector<Instruction> seq;
    seq.push_back(make_inst(Opcode::FFMA, 0, 1, 2));  // dst=R0, src1=R1, src2=R2
    seq.push_back(make_inst(Opcode::FFMA, 3, 1, 4));  // dst=R3, src1=R1, src2=R4

    auto_insert_reuse(seq);

    // R1 在 seq[0] 的 operand[1] 位置被标记为 .reuse
    ASSERT_TRUE(seq[0].reuse[1]);
    return true;
}

TEST(reuse_not_detected_no_usage) {
    // R1 在后续指令中未使用 → 不应标记
    std::vector<Instruction> seq;
    seq.push_back(make_inst(Opcode::FFMA, 0, 1, 2));
    seq.push_back(make_inst(Opcode::FFMA, 3, 5, 6));

    auto_insert_reuse(seq);

    ASSERT_FALSE(seq[0].reuse[1]);
    ASSERT_FALSE(seq[0].reuse[2]);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 2. 窗口边界
// ═══════════════════════════════════════════════════════════════

TEST(reuse_within_lookahead_4) {
    // R1 在 seq[0] 中作为源，seq[3] 中复用（LOOKAHEAD=4，包含 1,2,3,4）
    std::vector<Instruction> seq;
    seq.push_back(make_inst(Opcode::FFMA, 0, 1, 2));
    seq.push_back(make_inst(Opcode::FFMA, 3, 7, 8));
    seq.push_back(make_inst(Opcode::FFMA, 4, 7, 9));
    seq.push_back(make_inst(Opcode::FFMA, 5, 1, 10));  // R1 复用，在第 4 条内

    auto_insert_reuse(seq);

    ASSERT_TRUE(seq[0].reuse[1]);  // 应在窗口内被检测到
    return true;
}

TEST(reuse_beyond_lookahead) {
    // R1 在 seq[5] 中复用，超出 LOOKAHEAD=4 → 不应标记
    std::vector<Instruction> seq;
    seq.push_back(make_inst(Opcode::FFMA, 0, 1, 2));
    seq.push_back(make_inst(Opcode::FFMA, 3, 7, 8));
    seq.push_back(make_inst(Opcode::FFMA, 4, 7, 9));
    seq.push_back(make_inst(Opcode::FFMA, 5, 8, 10));
    seq.push_back(make_inst(Opcode::FFMA, 6, 9, 11));
    seq.push_back(make_inst(Opcode::FFMA, 7, 1, 12));  // 在窗口外

    auto_insert_reuse(seq);

    ASSERT_FALSE(seq[0].reuse[1]);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 3. 寄存器被写入阻断
// ═══════════════════════════════════════════════════════════════

TEST(reuse_blocked_by_overwrite) {
    // R1 在 seq[1] 中被写入 → 覆盖后不应再标记 .reuse
    std::vector<Instruction> seq;
    seq.push_back(make_inst(Opcode::FFMA, 0, 1, 2));    // src=R1, src=R2
    seq.push_back(make_inst(Opcode::FFMA, 1, 3, 4));    // dst=R1, 覆盖了 R1!
    seq.push_back(make_inst(Opcode::FFMA, 5, 1, 6));    // 用新的 R1

    auto_insert_reuse(seq);

    // seq[0] 的 R1 被 seq[1] 覆盖了 → 不应标记 reuse
    ASSERT_FALSE(seq[0].reuse[1]);
    return true;
}

TEST(reuse_after_overwrite) {
    // R1 在 seq[1] 中被写入，但 seq[1] 中的 R3 在 seq[2] 中被复用
    std::vector<Instruction> seq;
    seq.push_back(make_inst(Opcode::FFMA, 0, 1, 2));
    seq.push_back(make_inst(Opcode::FFMA, 1, 3, 4));    // dst=R1, src=R3
    seq.push_back(make_inst(Opcode::FFMA, 5, 3, 6));    // 复用 R3

    auto_insert_reuse(seq);

    // seq[1] 的 R3 在 seq[2] 中复用 → 标记 reuse
    ASSERT_TRUE(seq[1].reuse[1]);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 4. 边界情况
// ═══════════════════════════════════════════════════════════════

TEST(reuse_empty_sequence) {
    std::vector<Instruction> seq;
    auto_insert_reuse(seq);  // 不应崩溃
    ASSERT_EQ(seq.size(), 0);
    return true;
}

TEST(reuse_single_instruction) {
    std::vector<Instruction> seq;
    seq.push_back(make_inst(Opcode::FFMA, 0, 1, 2));

    auto_insert_reuse(seq);

    // 单条指令，无后续可复用 → 全部 false
    ASSERT_FALSE(seq[0].reuse[0]);
    ASSERT_FALSE(seq[0].reuse[1]);
    ASSERT_FALSE(seq[0].reuse[2]);
    return true;
}

TEST(reuse_dst_not_marked) {
    // 目标寄存器 (operand 0) 不应被标记 .reuse
    std::vector<Instruction> seq;
    seq.push_back(make_inst(Opcode::FFMA, 5, 1, 2));     // dst=R5
    seq.push_back(make_inst(Opcode::FFMA, 6, 5, 7));     // 复用 R5

    auto_insert_reuse(seq);

    // operand 0 是 dst，不会被标记（代码跳过 op==0）
    ASSERT_FALSE(seq[0].reuse[0]);
    return true;
}

TEST(reuse_multiple_sources) {
    // 两个源寄存器都在后续复用
    std::vector<Instruction> seq;
    seq.push_back(make_inst(Opcode::FFMA, 0, 1, 2));
    seq.push_back(make_inst(Opcode::FFMA, 3, 1, 2));     // 两个都复用

    auto_insert_reuse(seq);

    ASSERT_TRUE(seq[0].reuse[1]);   // R1 复用
    ASSERT_TRUE(seq[0].reuse[2]);   // R2 复用
    return true;
}

int main() {
    return run_all_tests();
}
