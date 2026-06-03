/* ============================================================================
 * Pascal GP106 — ISA 覆盖率探针 #2: 谓词/双精度/统一数据路径/转换变体
 * ============================================================================ */

extern "C" {

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

} // extern "C"
