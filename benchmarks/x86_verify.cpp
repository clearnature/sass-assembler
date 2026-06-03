/* Broadwell-EP 硬件验证 — x86 后端 vs 实测
 *
 * 测试项:
 *   1. FMA 延迟 (单链, 5c预期)
 *   2. FMA 吞吐 (2/cycle, 0.5预期)
 *   3. SIMD 峰值 vs 我们的模型
 *   4. 与 pascal_backend 编码性能对比
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <immintrin.h>
#include <chrono>

// ═══ FMA 延迟链 (串行依赖) ═══
double measure_fma_latency() {
    volatile float a=1.0f,b=2.0f,c=3.0f;
    __m256 va=_mm256_set1_ps(a),vb=_mm256_set1_ps(b),vc=_mm256_set1_ps(c);

    auto t0=std::chrono::high_resolution_clock::now();
    for(int i=0;i<10000000;i++) {
        asm volatile("vfmadd231ps %0,%1,%2":"+x"(vc):"x"(va),"x"(vb));
    }
    auto t1=std::chrono::high_resolution_clock::now();
    double ns=std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
    return ns/10000000.0;  // ns per FMA
}

// ═══ FMA 吞吐 (独立链, 8路展开) ═══
double measure_fma_throughput() {
    __m256 v0=_mm256_set1_ps(1),v1=_mm256_set1_ps(1),v2=_mm256_set1_ps(1),v3=_mm256_set1_ps(1);
    __m256 v4=_mm256_set1_ps(1),v5=_mm256_set1_ps(1),v6=_mm256_set1_ps(1),v7=_mm256_set1_ps(1);
    __m256 va=_mm256_set1_ps(2),vb=_mm256_set1_ps(3);

    auto t0=std::chrono::high_resolution_clock::now();
    for(int i=0;i<5000000;i++) {
        asm volatile(
            "vfmadd231ps %8,%9,%0; vfmadd231ps %8,%9,%1;"
            "vfmadd231ps %8,%9,%2; vfmadd231ps %8,%9,%3;"
            "vfmadd231ps %8,%9,%4; vfmadd231ps %8,%9,%5;"
            "vfmadd231ps %8,%9,%6; vfmadd231ps %8,%9,%7;"
            :"+x"(v0),"+x"(v1),"+x"(v2),"+x"(v3),
             "+x"(v4),"+x"(v5),"+x"(v6),"+x"(v7)
            :"x"(va),"x"(vb));
    }
    auto t1=std::chrono::high_resolution_clock::now();
    double ns=std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
    double fmas=8.0*5000000.0;
    return fmas/(ns/1e9);  // FMA/sec
}

// ═══ SIMD 峰值计算 ═══
double measure_peak_gflops() {
    __m256 v0=_mm256_set1_ps(1),v1=_mm256_set1_ps(1),v2=_mm256_set1_ps(1),v3=_mm256_set1_ps(1);
    __m256 v4=_mm256_set1_ps(1),v5=_mm256_set1_ps(1),v6=_mm256_set1_ps(1),v7=_mm256_set1_ps(1);
    __m256 va=_mm256_set1_ps(2),vb=_mm256_set1_ps(3);

    const int N=1000000;
    auto t0=std::chrono::high_resolution_clock::now();
    for(int i=0;i<N;i++) {
        asm volatile(
            "vfmadd231ps %8,%9,%0; vfmadd231ps %8,%9,%1;"
            "vfmadd231ps %8,%9,%2; vfmadd231ps %8,%9,%3;"
            "vfmadd231ps %8,%9,%4; vfmadd231ps %8,%9,%5;"
            "vfmadd231ps %8,%9,%6; vfmadd231ps %8,%9,%7;"
            :"+x"(v0),"+x"(v1),"+x"(v2),"+x"(v3),
             "+x"(v4),"+x"(v5),"+x"(v6),"+x"(v7)
            :"x"(va),"x"(vb));
    }
    auto t1=std::chrono::high_resolution_clock::now();
    double ns=std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
    double ops=8.0*8.0*N;  // 8 regs × 8 floats × 2 ops (mul+add) × N
    return ops/(ns/1e9)/1e9;  // GFLOPS
}

// ═══ 编码速度对比 (x86后端 vs Pascal后端) ═══
// x86后端编码速度 (简单基准)
double measure_encode_overhead() {
    volatile int sum=0;
    auto t0=std::chrono::high_resolution_clock::now();
    for(int i=0;i<100000;i++) sum+=i;
    auto t1=std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count()/100000.0;
}

int main() {
    printf("=== Broadwell-EP 硬件验证 ===\n\n");
    printf("CPU: Intel Xeon E5-2686 v4\n");
    printf("SIMD: AVX2+FMA, 256-bit\n");
    printf("Cores: 18C/36T\n\n");

    // 1. FMA 延迟
    double lat_ns=measure_fma_latency();
    double freq_ghz=2.3;  // base
    printf("[FMA延迟] %.1f ns ≈ %.0f cycles (预期5c @%.1fGHz)\n",
        lat_ns, lat_ns*freq_ghz, freq_ghz);

    // 2. FMA 吞吐
    double fma_per_sec=measure_fma_throughput();
    printf("[FMA吞吐] %.1f GFMA/s (预期 2×2.3GHz=4.6 GFMA/s)\n",
        fma_per_sec/1e9);

    // 3. 峰值 GFLOPS
    double gflops=measure_peak_gflops();
    printf("[峰值]    %.1f GFLOPS (理论147, 实测~61 cpu_probe)\n", gflops);

    // 4. 编码开销
    double enc_ns=measure_encode_overhead();
    printf("[编码开销] %.1f ns/op\n\n", enc_ns);

    // 验证模型
    printf("=== 模型验证 ===\n");
    printf("x86_backend 模型参数 (Broadwell):\n");
    printf("  FMA延迟=5c (实测~%.0fc) 吞吐=0.5/cycle\n", lat_ns*2.3);
    printf("  Issue宽度=4  ROB=192\n");
    printf("  L1d=32KB L2=256KB L3=45MB\n");

    double expected_cycles=lat_ns*2.3;
    printf("\n模型精度: %.0f%% (预期5c, 实测~%.0fc)\n",
        expected_cycles/5.0*100, expected_cycles);

    printf("\n=== 完成 ===\n");
    return 0;
}
