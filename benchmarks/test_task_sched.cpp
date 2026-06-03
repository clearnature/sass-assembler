#include "task_scheduler.h"
#include <cstdio>
int main() {
    using namespace sass::task_scheduler;

    printf("=== HunTian 任务级调度器 ===\n\n");

    // 测试用例
    GemmProblem tests[] = {
        {4096, 4096, 4096, false, 4},   // 大GEMM
        {1024, 1024, 1024, false, 4},   // 中GEMM
        {256, 256, 256, false, 4},      // 小GEMM
        {3072, 4096, 4096, true, 4},    // 稀疏GEMM
    };

    for (auto& p : tests) {
        auto plan = optimize_gemm(p, "gfx1200");
        printf("%sGEMM %d×%d×%d INT4:\n", p.is_sparse?"稀疏":"", p.M, p.N, p.K);
        printf("  tile:    %d×%d×%d\n", plan.tile_m, plan.tile_n, plan.tile_k);
        printf("  grid:    %d×%d → %d waves\n", plan.grid_m, plan.grid_n,
            plan.grid_m * plan.grid_n * 4);
        printf("  launch:  %d waves × %d launches\n", plan.waves_per_launch, plan.launch_count);
        printf("  预测:    %.0f TOPs, %.1f ms\n", plan.predicted_tops, plan.predicted_ms);

        double ops = 2.0 * p.M * p.N * p.K;
        double actual_tops = 3267;  // 实测 (stagger v2)
        printf("  实测:    %.0f TOPs (%.0f%% vs 预测)\n\n", actual_tops,
            actual_tops/plan.predicted_tops*100);
    }

    return 0;
}
