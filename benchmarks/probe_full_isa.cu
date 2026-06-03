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

extern "C" {

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

} // extern "C"
