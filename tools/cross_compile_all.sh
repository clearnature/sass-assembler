#!/bin/bash
# 全架构交叉编译 + SASS 提取
# 用法: bash tools/cross_compile_all.sh

ARCHS="61 70 75 80 86 89 90 100"
PROBE="benchmarks/probe_precision.cu"
OUTDIR="benchmarks/arch"

mkdir -p "$OUTDIR"

for sm in $ARCHS; do
    echo -n "[sm_${sm}] "
    cubin="$OUTDIR/x_sm${sm}.cubin"
    sass="$OUTDIR/x_sm${sm}.sass"

    if nvcc -arch=sm_${sm} -cubin -o "$cubin" "$PROBE" 2>/dev/null; then
        cuobjdump -sass "$cubin" > "$sass" 2>/dev/null
        lines=$(wc -l < "$sass" 2>/dev/null || echo 0)
        # 统计唯一 opcode (Pascal: byte 7, Volta+: word0[15:0])
        if [ "$sm" = "61" ]; then
            ops=$(grep '/\* 0x' "$sass" | sed 's/.*0x\(..\).*/\1/' | sort -u | wc -l)
        else
            ops=$(grep '/\* 0x' "$sass" | grep -v '0x000fe\|0x000fc\|0x000e\|0x001f' | \
                  sed 's/.*0x[0-9a-f]\{12\}\([0-9a-f]\{4\}\).*/\1/' | sort -u | wc -l)
        fi
        echo "OK  lines=$lines  opcodes=$ops"
    else
        echo "FAIL"
    fi
done

echo ""
echo "=== Opcode 汇总 ==="
for sm in $ARCHS; do
    sass="$OUTDIR/x_sm${sm}.sass"
    if [ -f "$sass" ]; then
        printf "sm_%-3s " "$sm"
        if [ "$sm" = "61" ]; then
            grep -oP '0x(..)[0-9a-f]{14}' "$sass" | sed 's/0x//' | sort -u | tr '\n' ' '
        else
            grep '/\* 0x' "$sass" | grep -v '0x000fe\|0x000fc\|0x000e\|0x001f' | \
                sed 's/.*0x[0-9a-f]\{12\}\([0-9a-f]\{4\}\).*/\1/' | sort -u | tr '\n' ' '
        fi
        echo ""
    fi
done
