/* 精准位域探针: FP16 + Float比较 + 谓词 + 控制字 */
#include <cuda_fp16.h>

extern "C" {

// ─── FP16 全覆盖 (hadd2, hmul2, hfma2, hset2) ───
__global__ void probe_fp16(half* out, half a, half b, half c, int n) {
    int tid = threadIdx.x;
    if (tid < n) {
        half h0 = __hadd(a, b);           // HADD2
        half h1 = __hmul(a, b);           // HMUL2
        half h2 = __hfma(a, b, c);        // HFMA2
        half2 v0 = __halves2half2(a, b);
        half2 v1 = __halves2half2(c, __float2half(1.0f));
        half2 vr = __hadd2(v0, v1);       // HADD2 (vector)
        out[0] = h0; out[1] = h1; out[2] = h2;
        out[3] = vr.x; out[4] = vr.y;
    }
}

// ─── Float 比较全覆盖 ───
__global__ void probe_fcmp(float* out, float a, float b, float c, float d) {
    // FSETP 全部变体
    bool gt  = (a > b);    // FSETP.GT
    bool ge  = (a >= b);   // FSETP.GEU
    bool eq  = (a == b);   // FSETP.EQ
    bool ne  = (a != b);   // FSETP.NEU
    bool lt  = (c < d);    // FSETP.LT
    bool le  = (c <= d);   // FSETP.LEU

    // FSET 变体
    float mx = (a > b) ? a : b;   // FMNMX.MAX
    float mn = (c < d) ? c : d;   // FMNMX.MIN
    out[0] = gt ? 1.0f : 0.0f;
    out[1] = ge ? 1.0f : 0.0f;
    out[2] = eq ? 1.0f : 0.0f;
    out[3] = ne ? 1.0f : 0.0f;
    out[4] = lt ? 1.0f : 0.0f;
    out[5] = le ? 1.0f : 0.0f;
    out[6] = mx;
    out[7] = mn;
}

// ─── 谓词全覆盖 ───
__global__ void probe_pred(int* out, int* in, int n) {
    int tid = threadIdx.x;
    if (tid < n) {
        int x = in[tid];
        int y = 0;
        // @P0 路径
        if (x > 0) { y = x + 1; }        // @P0 IADD
        else if (x < 0) { y = x - 1; }   // @!P0 IADD
        else { y = 0; }
        // @P1 路径
        if (x != 100) { y += 1; }        // @P1 IADD
        out[tid] = y;
    }
}

// ─── 控制字分析: 不同调度模式 ───
__global__ void probe_ctrl_word(int* out, const int* in, int n) {
    extern __shared__ int smem[];
    int tid = threadIdx.x;

    // .reuse 标志 → 控制字 bit 变化
    int a = in[tid];
    int b = in[tid + 32];
    int c = a + b;                       // .reuse on a or b
    int d = a * b;                       // .reuse again
    out[tid] = c + d;

    // BAR.SYNC → 控制字 0x...ea... (sync)
    smem[tid] = out[tid];
    __syncthreads();
    out[tid] = smem[(tid + 1) % blockDim.x];
}

// ─── 多种寄存器组合 (分析位域) ───
__global__ void probe_reg_layout(int* out, int a, int b, int c, int d, int e) {
    int tid = threadIdx.x;
    // R0-R31 全覆盖: 用不同寄存器组合
    int v0  = a + b;                     // IADD R?, R?, R?
    int v1  = c * d;                     // IMUL R?, R?
    int v2  = v0 + v1;                   // IADD R?, R?, R?
    int v3  = v2 + e;                    // IADD R?, R?, R?
    int v4  = v3 * 2;                    // SHL R?, R?, 1
    int v5  = v4 / 4;                    // SHR R?, R?, 2
    int v6  = v5 & 0xFF;                 // LOP32I.AND
    int v7  = v6 | 0xFF00;               // LOP32I.OR
    int v8  = v7 ^ 0xAAAA;               // LOP32I.XOR
    int v9  = __popc(v8);                // POPC
    out[tid] = v9;
}

} // extern "C"
