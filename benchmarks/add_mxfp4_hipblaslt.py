#!/usr/bin/env python3
"""为 hipBLASLt gfx1200 添加 MXFP4 配置 (从 INT4 复制)"""
import os

SRC = "/data/ROCm/hipBLASLt/library/src/amd_detail/rocblaslt/src/Tensile/Logic/asm_full/gfx1200"
ADDED = 0

for d in ["Equality", "GridBased"]:
    dp = os.path.join(SRC, d)
    for f in sorted(os.listdir(dp)):
        if 'I4' not in f or not f.endswith('.yaml'): continue
        
        src = os.path.join(dp, f)
        with open(src) as sfh:
            content = sfh.read()
        
        # 重命名: I4 → MX (MXFP4)
        new_name = f.replace('I4II', 'MXFP4').replace('I4BH', 'MXFP4')
        
        # 更新内容
        content = content.replace('I4II', 'MXFP4')
        content = content.replace('I4BH', 'MXFP4')
        content = content.replace('StaggeredWorkClaim: true',
            'StaggeredWorkClaim: true\n    Mxfp4Scale: true')
        
        dst = os.path.join(dp, new_name)
        with open(dst, 'w') as dfh:
            dfh.write(content)
        ADDED += 1

print(f"✅ hipBLASLt gfx1200 MXFP4: {ADDED} configs")
print(f"   Total: 164 + {ADDED} = {164+ADDED}")
