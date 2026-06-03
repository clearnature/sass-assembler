/* ============================================================================
 * Pascal GP106 — 控制流/同步全覆盖 CUDA 探针
 *
 * 目标: 触发 CAL, KILL, YIELD, BAR, DEPBAR 的 SASS 生成
 * ============================================================================ */

extern "C" {

// ─── CAL: 强制子程序调用 (非内联) ───
__device__ __noinline__ int cal_target(int a, int b, int c) {
    return a * b + c;
}

__global__ void probe_cal(int* out, int n) {
    int tid = threadIdx.x + blockIdx.x * blockDim.x;
    if (tid < n)
        out[tid] = cal_target(tid, tid + 1, tid + 2);  // → CAL
}

// ─── KILL: 线程提前终止 ───
__global__ void probe_kill(int* out, int* cond, int n) {
    int tid = threadIdx.x + blockIdx.x * blockDim.x;
    if (tid >= n) return;  // 超出范围 → 可能触发 KILL
    if (cond[tid] == 0) {
        out[tid] = -1;
        return;  // 提前退出 → KILL
    }
    out[tid] = tid;
    for (volatile int i = 0; i < 100; i++) {
        out[tid] += cond[tid];
    }
}

// ─── YIELD: 忙等待 / 主动让出 ───
__global__ void probe_yield(volatile int* flag, int* out) {
    int tid = threadIdx.x;
    // 忙等触发 YIELD: 等待 flag 被其他线程设置
    while (flag[0] == 0) {
        // volatile 循环 → ptxas 可能插入 YIELD
    }
    out[tid] = flag[0];

    if (tid == 0) {
        // 写后立即读 → DEPBAR
        flag[0] = 1;
        int val = flag[0];
        out[tid] = val;
    }
}

// ─── BAR + DEPBAR: warp/CTA 级别同步 ───
__global__ void probe_bar_depbar(int* out, int* scratch, int n) {
    extern __shared__ int smem[];
    int tid = threadIdx.x;

    // BAR: CTA 同步
    smem[tid] = tid;
    __syncthreads();           // → BAR.SYNC

    // 写后读 → ptxas 插入 DEPBAR
    smem[tid] = smem[(tid + 1) % blockDim.x];
    __syncthreads();           // → BAR.SYNC

    // 多次同步触发更多 BAR 变体
    if (tid < n) {
        scratch[tid] = smem[tid];
    }
    __syncthreads();           // → BAR.SYNC

    if (tid < n) {
        out[tid] = scratch[tid] + smem[tid];
    }

    // 线程分歧 + 同步 → DEPBAR 变体
    if (tid % 2 == 0) {
        smem[tid] *= 2;
    }
    __syncthreads();           // → BAR.SYNC (divergent path)
}

// ─── 组合: 所有剩余指令在同一个 kernel ───
__device__ __noinline__ int combined_callee(int x) {
    return x * x - x + 3;  // → CAL + IMAD + IADD
}

__global__ void probe_all_remaining(
    int* out, volatile int* flag, int* cond, int* scratch, int n)
{
    extern __shared__ int smem[];
    int tid = threadIdx.x;

    // CAL — 非内联调用
    int val = combined_callee(tid);

    // BAR — 同步
    smem[tid] = val;
    __syncthreads();

    // DEPBAR — 写后读 (ptxas 自动插入)
    smem[tid] = smem[(tid + 31) % blockDim.x] + val;
    __syncthreads();

    // YIELD — 忙等
    if (tid == 0) {
        while (flag[0] == 0) { /* busy */ }
    }
    __syncthreads();

    // KILL — 条件退出
    if (tid >= n) return;
    if (cond[tid] <= 0) {
        out[tid] = -1;
        return;
    }

    // BAR 在 KILL 之后 (部分线程已退出)
    __syncthreads();

    out[tid] = smem[tid] + scratch[tid];
}

} // extern "C"
