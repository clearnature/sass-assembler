#!/usr/bin/env python3
"""修复 gfx1200 Tensile YAML — 正确格式 + SWMMAC"""
import os, re

src_dir = "/data/ROCm/rocBLAS/library/src/blas3/Tensile/Logic/asm_full/gfx1152"
dst_dir = "/data/ROCm/rocBLAS/library/src/blas3/Tensile/Logic/asm_full/gfx1200"

os.makedirs(dst_dir, exist_ok=True)

for f in sorted(os.listdir(src_dir)):
    if not f.endswith('.yaml'): continue
    src = os.path.join(src_dir, f)
    dst_name = f.replace('gfx1152', 'gfx1200')
    dst = os.path.join(dst_dir, dst_name)

    with open(src) as sf:
        lines = sf.readlines()

    # 替换架构名 (头部)
    new_lines = []
    for line in lines:
        line = line.replace('gfx1152', 'gfx1200')
        new_lines.append(line)

    # 找到第一个 solution block 起始 (- - 开头), 插入 SWMMAC 配置
    out_lines = []
    for line in new_lines:
        out_lines.append(line)
        # 在第一个 solution block 的 AggressivePerfMode 后插入
        if 'AggressivePerfMode:' in line and 'StaggeredWorkClaim' not in ''.join(out_lines[-5:]):
            indent = '    '  # 4-space indent for solution block
            is_int8 = 'I8II' in dst_name or 'I8' in dst_name
            is_fp16 = 'HHS' in dst_name or 'HB' in dst_name

            if is_int8:
                out_lines.append(f'{indent}    EnableSwmmac: true\n')
                out_lines.append(f'{indent}    SwmmacChainCount: 14\n')
            if is_int8 or is_fp16:
                out_lines.append(f'{indent}    StaggeredWorkClaim: true\n')
                out_lines.append(f'{indent}    WorkClaimOversubWaves: 8\n')

    with open(dst, 'w') as df:
        df.writelines(out_lines)

count = len(os.listdir(dst_dir))
print(f"✅ gfx1200: {count} files (StaggeredWorkClaim + SWMMAC inserted into solution blocks)")
