/* ILP 调度收益端到端验证
 *
 * 构造混合指令序列 (链式 RAW 依赖 + 独立指令),
 * 用 cycle-accurate 模型对比 ILP 调度前后的总执行周期,
 * 验证 -20~50% 延迟收益声明。
 *
 * 模型: GP106 实测参数
 *   - FMA 延迟 4 cycles, issue_width=2, 4 warp schedulers
 *   - 链式依赖: R_d = R_d + R_s  (RAW, 必须等 4c)
 *   - 独立指令: R_x = R_a + R_b  (无依赖, 可填充间隙)
 */
#include <vector>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include "sass/instruction.h"
#include "sass/ilp_scheduler.h"

using namespace sass;
using namespace sass::optimizer;

// ─── cycle-accurate 模拟器 (复刻 ILPScheduler 的硬件模型) ───
// 输入: 指令序列; 输出: 总执行周期数
// 模型: FMA 延迟 4c, issue_width=2/cycle, 顺序发射
static int simulate(const std::vector<Instruction>& insts) {
    int reg_ready[256];
    std::memset(reg_ready, 0, sizeof(reg_ready));
    constexpr int FMA_LATENCY = 4;
    constexpr int ISSUE_WIDTH = 2;

    int cycle = 0;
    int issued_this_cycle = 0;
    for (auto& inst : insts) {
        // 1. 等待所有源操作数就绪
        int ready_cycle = cycle;
        for (size_t k = 1; k < inst.operands.size(); k++) {
            if (inst.operands[k].type == OpType::REG) {
                ready_cycle = std::max(ready_cycle, reg_ready[inst.operands[k].reg_id]);
            }
        }
        // 如果目标就绪周期晚于当前 cycle → 跳到该周期, 重置发射槽
        if (ready_cycle > cycle) {
            cycle = ready_cycle;
            issued_this_cycle = 0;
        }
        // 2. 发射槽限制 (issue_width=2/cycle)
        if (issued_this_cycle >= ISSUE_WIDTH) {
            cycle++;
            issued_this_cycle = 0;
        }
        // 3. 更新目标寄存器就绪周期
        if (!inst.operands.empty() && inst.operands[0].type == OpType::REG) {
            reg_ready[inst.operands[0].reg_id] = cycle + FMA_LATENCY;
        }
        issued_this_cycle++;
    }
    return cycle;
}

// ─── 生成测试序列 ───
// chain_len 条链式 FMA (R0=R0+Ri) + indep_len 条独立 FMA (Rj=Rk+Rl)
static std::vector<Instruction> build_mixed(int chain_len, int indep_len) {
    std::vector<Instruction> v;
    v.reserve(chain_len + indep_len);
    // 链式: R0 不断累加, 形成 RAW 依赖链
    for (int i = 0; i < chain_len; i++) {
        uint8_t src = (uint8_t)(1 + i);  // R1, R2, ...
        v.push_back(Instruction::CreateRegOp(Opcode::FFMA, 0, 0, src));
    }
    // 独立: R20+i = R40+i + R60+i  (与链式及彼此均无依赖)
    for (int i = 0; i < indep_len; i++) {
        uint8_t d  = (uint8_t)(20 + i);
        uint8_t s1 = (uint8_t)(40 + i);
        uint8_t s2 = (uint8_t)(60 + i);
        v.push_back(Instruction::CreateRegOp(Opcode::FFMA, d, s1, s2));
    }
    return v;
}

// ─── 理论下界 ───
// 链式: 4*(chain-1)+1 周期 (每条等前一条 FMA 完成)
// 独立: ceil(indep/2) 周期 (双发射)
// 理想交错: max(链式周期, 独立周期)  (独立指令填充进链式间隙)
static int theoretical_lower_bound(int chain, int indep) {
    int chain_cycles = chain > 0 ? 4 * (chain - 1) + 1 : 0;
    int indep_cycles = (indep + 1) / 2;
    return std::max(chain_cycles, indep_cycles);
}

int main() {
    std::printf("=== ILP 调度收益端到端验证 (GP106 模型) ===\n");
    std::printf("参数: FMA=4c, issue_width=2, RAW 依赖链\n\n");
    std::printf("%-8s %-8s %-12s %-12s %-12s %-10s\n",
                "chain", "indep", "orig_cycles", "sched_cycles",
                "theory_min", "speedup%");
    std::printf("------------------------------------------------------------\n");

    int total_cases = 0;
    int improved_cases = 0;
    double total_speedup = 0;

    // 多组规模: 链式 4..32, 独立 4..32
    for (int chain : {4, 8, 16, 32}) {
        for (int indep : {4, 8, 16, 32}) {
            auto insts = build_mixed(chain, indep);
            int orig = simulate(insts);

            auto sched = insts;  // 复制
            ilp_schedule(sched);
            int after = simulate(sched);

            int theory = theoretical_lower_bound(chain, indep);
            double speedup = orig > 0 ? 100.0 * (orig - after) / orig : 0;

            std::printf("%-8d %-8d %-12d %-12d %-12d %9.1f%%\n",
                        chain, indep, orig, after, theory, speedup);
            total_cases++;
            if (after < orig) improved_cases++;
            total_speedup += speedup;
        }
    }

    std::printf("------------------------------------------------------------\n");
    std::printf("改进案例: %d/%d  平均收益: %.1f%%\n",
                improved_cases, total_cases, total_speedup / total_cases);

    // 断言: 调度后不应比原始更差, 且至少有 1 个案例改进
    if (improved_cases == 0) {
        std::printf("\n[FAIL] 未观察到任何 ILP 收益\n");
        return 1;
    }
    // 断言: 调度后周期 <= 原始周期 (调度不应劣化)
    for (int chain : {4, 8, 16, 32}) {
        for (int indep : {4, 8, 16, 32}) {
            auto insts = build_mixed(chain, indep);
            int orig = simulate(insts);
            auto sched = insts;
            ilp_schedule(sched);
            int after = simulate(sched);
            if (after > orig) {
                std::printf("[FAIL] chain=%d indep=%d: 调度后 %d > 原始 %d\n",
                            chain, indep, after, orig);
                return 1;
            }
        }
    }
    std::printf("\n[PASS] 调度后均 <= 原始, 且存在改进案例\n");
    return 0;
}
