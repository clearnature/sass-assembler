#!/bin/bash
# 全架构 SASS 批量提取管线
# 编译全套探针 → cuobjdump 提取 → 构建 opcode 频率表
# 覆盖: sm_61, sm_70, sm_75, sm_80, sm_86, sm_89, sm_90, sm_100

set -e
ARCHS="sm_61 sm_70 sm_75 sm_80 sm_86 sm_89 sm_90 sm_100"
PROBES="benchmarks/probe_full_isa.cu benchmarks/probe_control_sync.cu benchmarks/probe_isa_ext1.cu benchmarks/probe_isa_ext2.cu benchmarks/probe_precision.cu"
OUTDIR="benchmarks/arch"

mkdir -p "$OUTDIR"

for arch in $ARCHS; do
    echo "=== $arch ==="
    combined="benchmarks/combined_${arch}.cu"
    # 合并探针
    echo 'extern "C" {' > "$combined"
    for p in $PROBES; do
        grep -v 'extern "C"' "$p" | grep -v '^//' >> "$combined" 2>/dev/null || true
    done
    echo '}' >> "$combined"

    # 编译
    cubin="$OUTDIR/full_${arch}.cubin"
    if nvcc -arch=$arch -cubin -o "$cubin" "$combined" 2>/dev/null; then
        # 提取 SASS
        sass="$OUTDIR/full_${arch}.sass"
        cuobjdump -sass "$cubin" > "$sass" 2>/dev/null

        # 统计
        lines=$(wc -l < "$sass")
        unique_instr=$(grep -c '/\* 0x' "$sass" || echo 0)
        unique_ops=$(grep '/\* 0x' "$sass" | grep -v '0x000fe\|0x000fc\|0x000e\|0x001f\|0x004f' | \
            sed 's/.*0x[0-9a-f]\{12\}\([0-9a-f]\{4\}\).*/\1/' | sort -u | wc -l)

        echo "  Lines: $lines  Instructions: $unique_instr  Unique opcodes: $unique_ops"
    else
        echo "  COMPILE FAILED"
    fi
    rm -f "$combined"
done

echo ""
echo "=== Opcode 频率汇总 ==="
for arch in $ARCHS; do
    sass="$OUTDIR/full_${arch}.sass"
    if [ -f "$sass" ]; then
        ops=$(grep '/\* 0x' "$sass" | grep -v '0x000fe\|0x000fc\|0x000e\|0x001f\|0x004f' | \
            sed 's/.*0x[0-9a-f]\{12\}\([0-9a-f]\{4\}\).*/\1/' | sort -u | wc -l)
        printf "%-8s %3d opcodes\n" "$arch" "$ops"
    fi
done
