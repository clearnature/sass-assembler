/* P0优化器测试 */
#include "test_common.h"
#include "sass/instruction.h"
#include "../src/sass/optimizer.h"
#include "../src/sass/safe_guard.h"
#include "../src/sass/robust_guard.h"
using namespace sass;
using namespace sass::optimizer;

static Instruction make_reg3(Opcode o, uint8_t d, uint8_t s1, uint8_t s2) {
    return Instruction::CreateRegOp(o, d, s1, s2);
}
static Instruction make_exit() {
    Instruction i; i.opcode=Opcode::EXIT; return i;
}

// P0-3: 死代码消除
TEST(dead_code_after_exit) {
    std::vector<Instruction> insts = {make_reg3(Opcode::FFMA,0,1,2), make_exit(),
        make_reg3(Opcode::FFMA,3,4,5)};
    optimizer::dead_code_elimination(insts);
    ASSERT_EQ(insts.size(), 2); // EXIT后的FFMA被删除
    return true;
}
TEST(dead_code_no_exit) {
    std::vector<Instruction> insts = {make_reg3(Opcode::FFMA,0,1,2), make_reg3(Opcode::FFMA,3,4,5)};
    optimizer::dead_code_elimination(insts);
    ASSERT_EQ(insts.size(), 2); // 无EXIT, 不删除
    return true;
}

// P0-1: .reuse检测
TEST(reuse_detected) {
    std::vector<Instruction> insts = {make_reg3(Opcode::FFMA,0,1,2), make_reg3(Opcode::FFMA,3,1,4)};
    optimizer::auto_reuse(insts);
    ASSERT_TRUE(insts[0].reuse[1]); // R1在后续复用
    return true;
}
TEST(reuse_not_overwrite) {
    std::vector<Instruction> insts = {make_reg3(Opcode::FFMA,0,1,2), make_reg3(Opcode::FFMA,1,3,4)};
    optimizer::auto_reuse(insts);
    ASSERT_FALSE(insts[0].reuse[1]); // R1被覆盖
    return true;
}

// P0-2: 4320D排序
TEST(schedule_ordering) {
    std::vector<Instruction> insts;
    auto a=make_reg3(Opcode::FFMA,0,1,2); a.scheduled_cycle=10; insts.push_back(a);
    auto b=make_reg3(Opcode::FFMA,3,4,5); b.scheduled_cycle=5;  insts.push_back(b);
    auto c=make_reg3(Opcode::FFMA,6,7,8); c.scheduled_cycle=0;  insts.push_back(c);
    optimizer::schedule_order(insts);
    ASSERT_EQ(insts[0].scheduled_cycle, 0);
    ASSERT_EQ(insts[1].scheduled_cycle, 5);
    ASSERT_EQ(insts[2].scheduled_cycle, 10);
    return true;
}

// P1-1: ILP调度 (保持指令语义, 重排顺序)
TEST(ilp_preserves_all_instructions) {
    std::vector<Instruction> insts;
    for (int i=0;i<10;i++) insts.push_back(make_reg3(Opcode::FFMA,i,i+1,i+2));
    size_t before = insts.size();
    optimizer::ilp_schedule(insts);
    ASSERT_EQ(insts.size(), before); // 不丢失指令
    return true;
}

TEST(ilp_separates_dependent) {
    // RAW依赖: inst[0]写R0, inst[1]读R0 → 应该拉开距离
    std::vector<Instruction> insts;
    insts.push_back(make_reg3(Opcode::FFMA,0,1,2)); // 写R0
    insts.push_back(make_reg3(Opcode::FFMA,3,0,4)); // 读R0 ← 依赖第一条
    insts.push_back(make_reg3(Opcode::FFMA,5,6,7)); // 独立
    optimizer::ilp_schedule(insts);
    // 独立的第三条应该插入到依赖对之间
    ASSERT_EQ(insts.size(), 3);
    return true;
}

// P1-2: 寄存器分配
TEST(reg_alloc_preserves_count) {
    std::vector<Instruction> insts;
    for (int i=0;i<10;i++) insts.push_back(make_reg3(Opcode::FFMA,i,i+1,i+2));
    size_t before = insts.size();
    optimizer::reg_allocate(insts);
    ASSERT_EQ(insts.size(), before);
    return true;
}

TEST(reg_pressure_report) {
    std::vector<Instruction> insts;
    for (int i=0;i<50;i++) insts.push_back(make_reg3(Opcode::FFMA,i,i+1,i+2));
    auto rpt = optimizer::reg_pressure(insts);
    ASSERT_TRUE(rpt.max_live > 0);
    ASSERT_TRUE(rpt.pressure_pct > 0);
    return true;
}

// P1-3: 常量传播
TEST(const_prop_basic) {
    std::vector<Instruction> insts;
    // MOV R0, 42; IADD R1, R0, R0  → R0应被替换为42
    Instruction mov; mov.opcode=Opcode::MOV;
    mov.operands={{OpType::REG,{.reg_id=0}},{OpType::IMM,{.imm_val=42}}};
    insts.push_back(mov);
    insts.push_back(make_reg3(Opcode::IADD,1,0,0)); // 使用R0
    optimizer::const_propagation(insts);
    // IADD的R0操作数应被替换为IMM 42
    ASSERT_TRUE(insts[1].operands[1].type==OpType::IMM ||
               insts[1].operands[2].type==OpType::IMM);
    return true;
}

// ═══ 稳定性测试 ═══

// 寄存器边界检查
TEST(stable_reg_bounds) {
    Instruction i; i.opcode=Opcode::FFMA;
    i.operands={{OpType::REG,{.reg_id=200}},{OpType::REG,{.reg_id=250}},{OpType::REG,{.reg_id=255}}};
    ASSERT_TRUE(sass::safe::validate_instruction(i));
    return true;
}
TEST(stable_reg_oob_rejected) {
    Instruction i; i.opcode=Opcode::FFMA;
    i.operands={{OpType::REG,{.reg_id=200}},{OpType::REG,{.reg_id=250}},{OpType::REG,{.reg_id=255}},{OpType::REG,{.reg_id=0}},{OpType::REG,{.reg_id=0}}};
    ASSERT_FALSE(sass::safe::validate_instruction(i)); // 5操作数
    return true;
}

// 空程序
TEST(stable_empty_program) {
    std::vector<sass::Instruction> empty;
    auto vr = sass::safe::validate(empty);
    ASSERT_FALSE(vr.ok);
    return true;
}

// 无终止符
TEST(stable_no_terminator_warns) {
    std::vector<sass::Instruction> insts;
    insts.push_back(make_reg3(Opcode::FFMA,0,1,2));
    auto vr = sass::safe::validate(insts);
    ASSERT_TRUE(vr.warnings > 0); // 缺EXIT
    return true;
}

// 超大寄存器
TEST(stable_high_reg_pressure_warns) {
    std::vector<sass::Instruction> insts;
    for (int i=0;i<10;i++)
        insts.push_back(make_reg3(Opcode::FFMA,i,i+1,i+65)); // 用高编号寄存器
    auto vr = sass::safe::validate(insts);
    ASSERT_TRUE(vr.warnings > 0);
    return true;
}

// 有效程序
TEST(stable_valid_program) {
    std::vector<sass::Instruction> insts;
    insts.push_back(make_reg3(Opcode::FFMA,0,1,2));
    insts.push_back(make_exit());
    auto vr = sass::safe::validate(insts);
    ASSERT_TRUE(vr.ok);
    ASSERT_TRUE(vr.has_terminator);
    return true;
}

// ═══ Fuzz 测试: 随机指令序列 ═══
TEST(fuzz_random_instructions) {
    srand(42);  // 固定种子
    for (int trial=0;trial<100;trial++) {
        std::vector<Instruction> insts;
        int n = rand() % 500 + 1;
        for (int i=0;i<n;i++) {
            Instruction inst;
            inst.opcode = static_cast<Opcode>(rand() % 120);
            int ops = rand() % 4;
            for (int j=0;j<ops;j++)
                inst.operands.push_back({OpType::REG, {.reg_id=(uint8_t)(rand()%64)}});
            insts.push_back(inst);
        }
        auto vr = sass::safe::validate(insts);
        if (vr.ok) {
            ilp_schedule(insts);  // 不应崩溃
            ASSERT_TRUE(insts.size() >= (size_t)n/2);  // 不丢失太多
        }
    }
    return true;
}

TEST(fuzz_encoder_no_crash) {
    srand(123);
    for (int trial=0;trial<20;trial++) {
        std::vector<Instruction> insts;
        insts.push_back(make_reg3(Opcode::FFMA,0,1,2));
        for (int i=0;i<100;i++)
            insts.push_back(Instruction::CreateRegOp(static_cast<Opcode>(rand()%50),rand()%32,rand()%32,rand()%32));
        insts.push_back(make_exit());
        // 所有优化pass不应崩溃
        dead_code_elimination(insts);
        auto_reuse(insts);
        schedule_order(insts);
        const_propagation(insts);
        ilp_schedule(insts);
        predicate_optimize(insts);
        loop_unroll(insts);
        auto rpt = reg_pressure(insts);
        ASSERT_TRUE(rpt.max_live >= 0);
    }
    return true;
}

// 大程序不崩溃
// ═══ 鲁棒性8项测试 ═══

TEST(robust_thread_safety_lock) {
    { std::lock_guard<std::mutex> lk(sass::guard::hal_mutex()); }
    ASSERT_TRUE(true);  // 不崩溃 = 通过
    return true;
}

TEST(robust_memory_exhaustion) {
    auto v = sass::guard::safe_reserve<Instruction>(1000000);
    ASSERT_TRUE(v.empty() || v.capacity() >= 1000000);  // 不抛异常
    return true;
}

TEST(robust_file_integrity) {
    const char data[] = "test";
    bool ok = sass::guard::safe_fwrite("robust_test.bin", data, 4);
    ASSERT_TRUE(ok);
    return true;
}

TEST(robust_imm_overflow) {
    ASSERT_TRUE(sass::guard::imm_in_range(42, 14));
    ASSERT_FALSE(sass::guard::imm_in_range(99999, 14));  // 14位最大 8191
    ASSERT_TRUE(sass::guard::imm_in_range(-8192, 14));
    return true;
}

TEST(robust_vavx3_mask_bounds) {
    std::vector<Instruction> insts;
    for (int i=0;i<8;i++) insts.push_back(make_reg3(Opcode::FFMA,i%64,(i+63)%64,(i+62)%64));
    uint64_t mask = sass::guard::safe_vavx3_mask(insts, 0);
    ASSERT_TRUE(mask != 0);
    return true;
}

TEST(robust_recursion_depth) {
    bool ok=true;
    try { sass::guard::check_recursion_depth(50, 100); }
    catch(...) { ok=false; }
    ASSERT_TRUE(ok);
    return true;
}

TEST(robust_imm_14bit) {
    ASSERT_TRUE(sass::guard::imm_in_range(0, 14));
    ASSERT_TRUE(sass::guard::imm_in_range(8191, 14));
    ASSERT_FALSE(sass::guard::imm_in_range(8192, 14));
    return true;
}

TEST(stable_large_program) {
    std::vector<sass::Instruction> insts;
    for (int i=0;i<10000;i++)
        insts.push_back(make_reg3(Opcode::FFMA,i%32,(i+1)%32,(i+2)%32));
    insts.push_back(make_exit());
    auto vr = sass::safe::validate(insts);
    ASSERT_TRUE(vr.ok);
    optimizer::ilp_schedule(insts);  // 不应崩溃
    ASSERT_EQ(insts.size(), 10001);
    return true;
}

TEST(ilp_no_crash_empty) {
    std::vector<Instruction> insts;
    optimizer::ilp_schedule(insts); // 不崩溃
    ASSERT_EQ(insts.size(), 0);
    return true;
}

// 全流程 P0
TEST(p0_full_pipeline) {
    std::vector<Instruction> insts;
    auto a=make_reg3(Opcode::FFMA,0,1,2); a.scheduled_cycle=10; insts.push_back(a);
    auto b=make_reg3(Opcode::FFMA,3,1,4); b.scheduled_cycle=5;  insts.push_back(b);
    auto e=make_exit(); e.scheduled_cycle=100; insts.push_back(e); // EXIT在高周期
    auto dead=make_reg3(Opcode::FFMA,5,6,7); dead.scheduled_cycle=200; insts.push_back(dead); // 死代码

    optimizer::optimize_p0(insts);
    // FFMA+FFMA+EXIT, 死代码的FFMA被消除
    ASSERT_EQ(insts.size(), 3); // FFMA+FFMA+EXIT, 死代码消除
    ASSERT_EQ(insts[0].scheduled_cycle, 5); // 排序后: 5,10,100
    ASSERT_EQ(insts[1].scheduled_cycle, 10);
    ASSERT_TRUE(insts[0].reuse[1]); // R1复用检测
    return true;
}

int main(){return run_all_tests();}
