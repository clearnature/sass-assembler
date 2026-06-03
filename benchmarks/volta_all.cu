/* ============================================================================
 * Pascal GP106 全覆盖 CUDA 探针内核
 *
 * 目标：为 55 条指令逐一生成 SASS hex 用于逆向工程。
 *
 * 覆盖:
 *   控制流: CAL, KILL, YIELD
 *   同步:   BAR, DEPBAR, MEMBAR
 *   转换:   CVT (int→float, float→int)
 *   位操作: BFE, BFI, BREV
 *   内存:   LDC (常量加载)
 *   浮点:   FSET, FMNMX, FSEL
 * ============================================================================ */

// merged {

// ─── 控制流 ───
__global__ void probe_control_flow(int* out, int cond) {
    if (cond) {
        // BAR — 线程同步
        __syncthreads();
        out[0] = 1;
    } else {
        // YIELD + KILL (通过死代码路径触发)
        out[0] = 0;
    }
}

// ─── CAL (子程序调用) ───
__device__ int helper_func(int x) {
    return x * x + 1;
}

__global__ void probe_cal(int* out, int n) {
    int tid = threadIdx.x;
    if (tid < n)
        out[tid] = helper_func(tid);
}

// ─── 同步屏障 ───
__global__ void probe_barriers(int* out) {
    __syncthreads();
    out[0] = threadIdx.x;
    __threadfence_block();
    __threadfence();
    __syncthreads();
}

// ─── 类型转换 ───
__global__ void probe_cvt(float* fout, int* iout, int n) {
    int tid = threadIdx.x;
    if (tid < n) {
        float f = (float)tid;        // I2F
        iout[tid] = (int)(f + 0.5f); // F2I + FADD
        fout[tid] = (float)iout[tid]; // I2F
    }
}

// ─── 位操作 ───
__global__ void probe_bitops(unsigned* out, unsigned a, unsigned b, unsigned mask) {
    unsigned r;

    // BFE: bit field extract via shifts
    r = (a >> 8) & 0xFF;

    // BFI: bit field insert
    r = (a & ~(mask << 4)) | ((b & mask) << 4);

    // BREV: bit reverse
    unsigned rev = 0;
    for (int i = 0; i < 32; i++) {
        rev = (rev << 1) | (a & 1);
        a >>= 1;
    }

    out[0] = r ^ rev;
}

// ─── 浮点比较 + 选择 ───
__global__ void probe_float_compare(float* out, float a, float b, float c, float d) {
    // FSET / FMNMX / FSEL
    float mx = (a > b) ? a : b;       // max → FMNMX
    float mn = (c < d) ? c : d;       // min → FMNMX
    float sel = (mx > mn) ? mx : mn;  // FSEL
    out[0] = sel;
    out[1] = (a == b) ? 1.0f : 0.0f;  // FSET (compare set)
}

// ─── 常量内存 ───
__constant__ float const_data[64];

__global__ void probe_ldc(float* out, int idx) {
    if (idx < 64)
        out[0] = const_data[idx];  // LDC
}

// ─── 内存操作全覆盖 ───
__global__ void probe_memory(float* global_out, int* global_in) {
    extern __shared__ float shared_buf[];
    int tid = threadIdx.x;

    shared_buf[tid] = (float)global_in[tid];   // LDG + STS
    __syncthreads();
    global_out[tid] = shared_buf[tid];          // LDS + STG
}

} // // merged
/* ============================================================================
 * Pascal GP106 — 控制流/同步全覆盖 CUDA 探针
 *
 * 目标: 触发 CAL, KILL, YIELD, BAR, DEPBAR 的 SASS 生成
 * ============================================================================ */

// merged {

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

} // // merged
/* ============================================================================
 * Pascal GP106 — 全 ISA 覆盖率探针 #1: 原子操作 + MUFU + 整数变体
 * ============================================================================ */

// merged {

// ─── 原子操作 (ATOM, ATOMS, RED) ───
__global__ void probe_atomics(int* gmem, float* fmem, unsigned* umem) {
    int tid = threadIdx.x;
    atomicAdd(&gmem[tid], tid);                        // ATOM.ADD
    atomicExch(&gmem[tid + 32], tid);                  // ATOM.EXCH
    atomicCAS(&gmem[tid + 64], 0, tid);                // ATOM.CAS
    atomicMax(&gmem[tid + 96], tid);                   // ATOM.MAX
    atomicMin(&gmem[tid + 128], tid);                  // ATOM.MIN
    atomicAnd(&umem[tid], 0xFF);                       // ATOM.AND
    atomicOr(&umem[tid + 32], 0xFF00);                 // ATOM.OR
    atomicXor(&umem[tid + 64], tid);                   // ATOM.XOR
    atomicAdd(&fmem[tid], (float)tid);                 // ATOMS.ADD (shared mem atomics)
}

// ─── MUFU 特殊函数 (RCP, RSQ, SIN, COS, EX2, LG2) ───
__global__ void probe_mufu(float* out, float* in, int n) {
    int tid = threadIdx.x;
    if (tid < n) {
        float x = in[tid] + 1.0f;
        out[0] = __frcp_rn(x);       // MUFU.RCP
        out[1] = __frsqrt_rn(x);     // MUFU.RSQ
        out[2] = __sinf(x);          // MUFU.SIN
        out[3] = __cosf(x);          // MUFU.COS
        out[4] = __expf(x);          // MUFU.EX2
        out[5] = __logf(x);          // MUFU.LG2
    }
}

// ─── 整数变体 (POPC, FLO, LOP.AND/OR/XOR) ───
__global__ void probe_int_variants(unsigned* out, unsigned a, unsigned b) {
    out[0] = __popc(a);              // POPC
    out[1] = __popc(b);
    out[2] = __ffs(a);               // FLO (find first set)
    out[3] = __ffs(b);
    out[4] = a & b;                  // LOP.AND
    out[5] = a | b;                  // LOP.OR
    out[6] = a ^ b;                  // LOP.XOR
    out[7] = ~a;                     // LOP.NOT (via XOR)
}

// ─── LEA 地址计算 ───
__global__ void probe_lea(int* out, int* in, int stride) {
    int tid = threadIdx.x;
    int* addr = &in[tid * stride];
    out[tid] = *addr;                // LEA (load effective address)
}

// ─── 数据移动 (PRMT, SHFL, VOTE) ───
__global__ void probe_data_move(unsigned* out) {
    int tid = threadIdx.x;
    // SHFL: warp shuffle
    out[tid] = __shfl_sync(0xFFFFFFFF, tid, 1);     // SHFL.IDX
    out[tid+32] = __shfl_xor_sync(0xFFFFFFFF, tid, 1); // SHFL.XOR

    // VOTE: warp vote
    int pred = (tid < 16);
    out[64] = __any_sync(0xFFFFFFFF, pred);          // VOTE.ANY
    out[65] = __all_sync(0xFFFFFFFF, pred);          // VOTE.ALL
    out[66] = __ballot_sync(0xFFFFFFFF, pred);       // VOTE.BALLOT
}

// ─── 控制流同步 (SSY, SYNC, PBK, BRK, CONT) ───
__global__ void probe_cf_sync(int* out, int n) {
    int tid = threadIdx.x;
    int sum = 0;
    for (int i = 0; i < n; i++) {    // loop → SSY, SYNC, PBK, BRK, CONT
        sum += i;
        if (sum > 1000) break;       // BRK
    }
    out[tid] = sum;
}

// ─── 浮点变体 (FADD32I, FFMA32I, DFMA) ───
__global__ void probe_float_variants(float* out, float a, int b) {
    out[0] = a + (float)b;           // FADD32I
    out[1] = out[0] * a + 1.0f;     // FFMA32I
    double da = (double)a;
    double db = (double)b;
    double dc = da * db + 1.0;       // DFMA (double precision FMA)
    out[2] = (float)dc;
}

} // // merged
/* ============================================================================
 * Pascal GP106 — ISA 覆盖率探针 #2: 谓词/双精度/统一数据路径/转换变体
 * ============================================================================ */

// merged {

// ─── 谓词前缀 (@P0, @!P0) ───
__global__ void probe_predication(int* out, int* in, int n) {
    int tid = threadIdx.x;
    if (tid < n) {
        int x = in[tid];
        if (x > 0)      out[tid] = x + 1;      // @P0 IADD
        else if (x < 0) out[tid] = x - 1;      // @!P0 IADD32I
        else            out[tid] = 0;           // @!P0 MOV

        out[tid + n] = (x != 0) ? x * 2 : x;    // @P0 IMUL : @!P0 MOV
    }
}

// ─── 双精度全覆盖 ───
__global__ void probe_double(double* out, double a, double b) {
    out[0] = a + b;          // DADD
    out[1] = a - b;          // DADD
    out[2] = a * b;          // DMUL
    out[3] = a / b;          // DMUL + MUFU.RCP
    out[4] = (a > b) ? a : b; // DSET + DSEL
}

// ─── 统一数据路径 ───
__global__ void probe_uniform(int* out, int* in, int n, int stride) {
    if (n > 0) {
        int base = in[0];
        #pragma unroll
        for (int i = 0; i < 4; i++)
            out[i] = base + i * stride;   // UADD/UIMAD
    }
}

// ─── 转换变体全模式 ───
__global__ void probe_cvt_variants(int* iout, float* fout, unsigned* uout, int val) {
    iout[0] = __float2int_rd((float)val);  // F2I.RD
    iout[1] = __float2int_ru((float)val);  // F2I.RU
    iout[2] = __float2int_rn((float)val);  // F2I.RN
    iout[3] = __float2int_rz((float)val);  // F2I.RZ
    fout[0] = __int2float_rd(val);          // I2F.RD
    fout[1] = __int2float_ru(val);          // I2F.RU
    fout[2] = __int2float_rn(val);          // I2F.RN
    fout[3] = __int2float_rz(val);          // I2F.RZ
}

// ─── XMAD 全覆盖变体 ───
__global__ void probe_xmad_all(int* out, int a, int b, int c) {
    short sa = (short)a, sb = (short)b;
    out[0] = (int)(sa * sb) + c;              // XMAD.S16.S16
    out[1] = a * b + c;                       // XMAD chain → XMAD+MRG+PSL
    out[2] = ((a >> 16) * (b >> 16)) + c;     // XMAD.CHI
}

// ─── IMNMX 全覆盖 ───
__global__ void probe_imnmx_all(int* out, int a, int b, int n) {
    int tid = threadIdx.x;
    if (tid < n) {
        out[0] = min(a, b);                    // IMNMX.MIN
        out[1] = max(a, b);                    // IMNMX.MAX
        unsigned ua = (unsigned)a, ub = (unsigned)b;
        out[2] = (int)min(ua, ub);             // IMNMX.MIN.U32
        out[3] = (int)max(ua, ub);             // IMNMX.MAX.U32
    }
}

// ─── CONT ───
__global__ void probe_cont(int* out, int n) {
    int tid = threadIdx.x;
    int sum = 0;
    for (int i = 0; i < n; i++) {
        if (i % 3 == 0) continue;
        sum += i;
    }
    out[tid] = sum;
}

// ─── 乘加全覆盖 ───
__global__ void probe_mad_all(int* out, float* fout, int a, int b, int c) {
    out[0] = a * b + c;                    // IMAD (integer mad)
    out[1] = a * b + c + c;                // IMAD with accumulator
    fout[0] = (float)a * (float)b + (float)c;  // FFMA
}

// ─── SEL / 条件移动 ───
__global__ void probe_sel(int* out, int a, int b, int cond) {
    out[0] = cond ? a : b;                 // SEL
}

} // // merged
/* Pascal GP106 — 纹理指令探针 v2 (CUDA 12 texture object API) */
#include <cuda_runtime.h>

// merged {

__global__ void probe_tex(float* out, cudaTextureObject_t tex, float coord) {
    out[0] = tex1Dfetch<float>(tex, (int)coord);    // TLD
    out[1] = tex1D<float>(tex, coord);               // TEX
}

__global__ void probe_surface(int* out, cudaSurfaceObject_t surf, int idx) {
    surf1Dread<int>(out, surf, idx * sizeof(int));   // SULD
    surf1Dwrite(surf, idx, idx * sizeof(int));        // SUST
}

} // // merged
