#!/usr/bin/env python3
"""为 hipBLASLt gfx1200 添加 INT4 (I4II) Tensile 配置"""
import os, shutil

SRC = "/data/ROCm/hipBLASLt/library/src/amd_detail/rocblaslt/src/Tensile/Logic/asm_full/gfx1200"
EQ  = os.path.join(SRC, "Equality")
GRID = os.path.join(SRC, "GridBased")

# 从 INT8 模板复制
added = 0
for d in [EQ, GRID]:
    for f in sorted(os.listdir(d)):
        if not 'I8' in f or not f.endswith('.yaml'): continue
        src = os.path.join(d, f)
        with open(src) as sfh:
            content = sfh.read()
        
        # INT8 → INT4
        content = content.replace('DataType: 8', 'DataType: 9')
        content = content.replace('I8', 'I4')
        content = content.replace('i8', 'i4')
        
        # 添加 SWMMAC + StaggeredWorkClaim
        if 'AggressivePerfMode:' in content:
            content = content.replace(
                '    AggressivePerfMode:',
                '    EnableSwmmac: true\n    SwmmacChainCount: 16\n    StaggeredWorkClaim: true\n    WorkClaimOversubWaves: 8\n    AggressivePerfMode:'
            )
        
        new_name = f.replace('I8', 'I4')
        dst = os.path.join(d, new_name)
        with open(dst, 'w') as dfh:
            dfh.write(content)
        added += 1

print(f"hipBLASLt gfx1200 INT4: {added} configs added")
print(f"Total gfx1200: {154 + added} configs")
