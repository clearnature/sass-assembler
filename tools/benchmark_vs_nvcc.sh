#!/bin/bash
# =============================================================================
# HunTian SASS vs nvcc 基准测试
#
# 对比 ht-as 与 nvcc+ptxas 在 GEMM 内核上的输出。
# 用法: bash tools/benchmark_vs_nvcc.sh
# =============================================================================

set -e

HT_AS="./build/ht-as"
NVCC=$(which nvcc 2>/dev/null || echo "")
BENCH_DIR="benchmarks"
RESULTS="$BENCH_DIR/results.txt"

mkdir -p "$BENCH_DIR"

echo "=== HunTian SASS vs nvcc Benchmark ===" | tee "$RESULTS"
echo "Date: $(date)" | tee -a "$RESULTS"
echo "" | tee -a "$RESULTS"

# ─── 1. ht-as 基准 ───
echo "--- ht-as (HunTian SASS Assembler) ---" | tee -a "$RESULTS"

for sass_file in examples/pascal/*.sass; do
    name=$(basename "$sass_file" .sass)
    start=$(date +%s%N)
    $HT_AS "$sass_file" -o "$BENCH_DIR/${name}.sabin" 2>/dev/null
    end=$(date +%s%N)
    elapsed_ms=$(( (end - start) / 1000000 ))
    size=$(stat -c%s "$BENCH_DIR/${name}.sabin" 2>/dev/null || echo 0)
    printf "  %-20s  %4d ms  %6d bytes\n" "$name" "$elapsed_ms" "$size" | tee -a "$RESULTS"
done

# ─── 2. nvcc+ptxas 基准 (如果可用) ───
if [ -n "$NVCC" ]; then
    echo "" | tee -a "$RESULTS"
    echo "--- nvcc + ptxas (NVIDIA) ---" | tee -a "$RESULTS"

    cat > "$BENCH_DIR/gemm_16x16.cu" << 'CUDA'
extern "C" __global__ void gemm_16x16(float* C, const float* A, const float* B, int N) {
    int row = blockIdx.y * 16 + threadIdx.y;
    int col = blockIdx.x * 16 + threadIdx.x;
    float sum = 0.0f;
    for (int k = 0; k < N; ++k)
        sum += A[row * N + k] * B[k * N + col];
    C[row * N + col] = sum;
}
CUDA

    start=$(date +%s%N)
    $NVCC -arch=sm_61 -cubin -o "$BENCH_DIR/gemm_nvcc.cubin" "$BENCH_DIR/gemm_16x16.cu" 2>/dev/null
    end=$(date +%s%N)
    elapsed_ms=$(( (end - start) / 1000000 ))
    size=$(stat -c%s "$BENCH_DIR/gemm_nvcc.cubin" 2>/dev/null || echo 0)
    printf "  %-20s  %4d ms  %6d bytes\n" "gemm_16x16 (nvcc)" "$elapsed_ms" "$size" | tee -a "$RESULTS"

    # cuobjdump 反汇编
    if command -v cuobjdump &>/dev/null; then
        cuobjdump -sass "$BENCH_DIR/gemm_nvcc.cubin" > "$BENCH_DIR/gemm_nvcc.sass" 2>/dev/null || true
        sass_lines=$(wc -l < "$BENCH_DIR/gemm_nvcc.sass" 2>/dev/null || echo 0)
        echo "         nvcc SASS lines: $sass_lines" | tee -a "$RESULTS"
    fi
else
    echo "" | tee -a "$RESULTS"
    echo "(nvcc not available — skipping NVIDIA comparison)" | tee -a "$RESULTS"
fi

echo "" | tee -a "$RESULTS"
echo "Results saved to $RESULTS"
