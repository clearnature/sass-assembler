#!/bin/bash
# 批量提取所有架构的 SASS opcode 映射
# 用法: bash tools/extract_all_archs.sh

ARCHS="sm_70 sm_75 sm_80 sm_86 sm_89 sm_90 sm_100"
PROBE="benchmarks/probe_full_isa.cu"

mkdir -p benchmarks/arch

for arch in $ARCHS; do
    cubin="benchmarks/arch/probe_${arch}.cubin"
    sass="benchmarks/arch/probe_${arch}.sass"
    
    echo -n "[$arch] compiling... "
    nvcc -arch=$arch -cubin -o "$cubin" "$PROBE" 2>/dev/null
    if [ $? -eq 0 ]; then
        cuobjdump -sass "$cubin" 2>/dev/null > "$sass"
        lines=$(wc -l < "$sass")
        # 统计唯一 opcode 字节
        opcodes=$(grep -oP '/\* *0x([0-9a-f]{2})[0-9a-f]{14} *\*/' "$sass" | sed 's/.*0x\(..\).*/\1/' | sort -u | wc -l)
        echo "$lines lines SASS, $opcodes unique opcode bytes"
    else
        echo "FAILED"
    fi
done

echo ""
echo "=== 汇总 ==="
for arch in $ARCHS; do
    sass="benchmarks/arch/probe_${arch}.sass"
    if [ -f "$sass" ]; then
        ops=$(grep -oP '/\* *0x([0-9a-f]{2})[0-9a-f]{14} *\*/' "$sass" | sed 's/.*0x\(..\).*/\1/' | sort -u | tr '\n' ' ')
        printf "%-8s  %2d opcodes: %s\n" "$arch" "$(echo $ops | wc -w)" "$ops"
    fi
done
