/* ============================================================================
 * Pascal GP106 — 全 ISA 覆盖率探针 #1: 原子操作 + MUFU + 整数变体
 * ============================================================================ */

extern "C" {

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

} // extern "C"
