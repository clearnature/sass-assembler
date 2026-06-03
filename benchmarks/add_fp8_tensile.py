#!/usr/bin/env python3
"""为 gfx1200 添加 FP8/BF8 Tensile 配置
   类型代码: FP8=10, BF8=11
   
   同时检查还有哪些 rocBLAS 文件需要 gfx1200 修改
"""

import os, glob

SRC = "/data/ROCm/rocBLAS/library/src/blas3/Tensile/Logic/asm_full/gfx1200"

# ─── FP8/BF8 配置 ───
# 从 HHS (FP16) 模板生成
bases = [
    "Cijk_Ailk_Bjlk_HHS_BH", "Cijk_Ailk_Bljk_HHS_BH",
    "Cijk_Alik_Bjlk_HHS_BH", "Cijk_Alik_Bljk_HHS_BH"
]

# FP8_FP8 (F8F8)
for base in bases:
    src = os.path.join(SRC, f"gfx1200_{base}.yaml")
    if not os.path.exists(src): continue
    with open(src) as f: content = f.read()
    # FP8 input, FP32 compute
    content = content.replace("DataType: 4", "DataType: 10")
    content = content.replace("DestDataType: 4", "DestDataType: 0")  # FP32 output
    # 添加 FP8 特有配置
    content = content.replace("SwmmacChainCount: 14", "SwmmacChainCount: 16")
    new_name = f"gfx1200_{base.replace('HHS','F8F8')}.yaml"
    with open(os.path.join(SRC, new_name), 'w') as f:
        f.write(content)

# BF8_BF8 (B8B8)
for base in bases:
    src = os.path.join(SRC, f"gfx1200_{base}.yaml")
    if not os.path.exists(src): continue
    with open(src) as f: content = f.read()
    content = content.replace("DataType: 4", "DataType: 11")
    content = content.replace("DestDataType: 4", "DestDataType: 0")
    content = content.replace("SwmmacChainCount: 14", "SwmmacChainCount: 16")
    new_name = f"gfx1200_{base.replace('HHS','B8B8')}.yaml"
    with open(os.path.join(SRC, new_name), 'w') as f:
        f.write(content)

# FP8_BF8 mixed (F8B8)
for base in bases:
    src = os.path.join(SRC, f"gfx1200_{base}.yaml")
    if not os.path.exists(src): continue
    with open(src) as f: content = f.read()
    content = content.replace("DataType: 4", "DataType: 10")  # A=FP8
    # B type is stored elsewhere in Tensile, simplified here
    content = content.replace("DestDataType: 4", "DestDataType: 0")
    content = content.replace("SwmmacChainCount: 14", "SwmmacChainCount: 16")
    new_name = f"gfx1200_{base.replace('HHS','F8B8')}.yaml"
    with open(os.path.join(SRC, new_name), 'w') as f:
        f.write(content)

all_files = sorted(os.listdir(SRC))
print(f"=== gfx1200 总计: {len(all_files)} 配置 ===")
for prefix in ['I4II','I8II','F8F8','B8B8','F8B8','HHS','BBS','SS','DD','HB','SB']:
    count = len([f for f in all_files if prefix in f])
    if count > 0:
        print(f"  {prefix:8s}: {count}")

# ─── 检查 rocBLAS 中需要 gfx1200 引用的其他文件 ───
print("\n=== rocBLAS 中其他需要 gfx1200 的文件 ===")
for pattern in ['library/src/blas_ex/*.cpp', 'library/src/blas3/*.cpp',
                'clients/include/*.hpp', 'clients/gtest/*.cpp']:
    files = glob.glob(f"/data/ROCm/rocBLAS/{pattern}")
    for f in files[:3]:
        with open(f) as fh:
            content = fh.read()
            mentions_arch = 'gfx110' in content or 'navi3' in content
            mentions_gfx12 = 'gfx1200' in content
            if mentions_arch and not mentions_gfx12:
                print(f"  ⚠️ {f}: 有其他架构但缺少 gfx1200")
