#!/usr/bin/env python3
"""为 rocBLAS gfx1200 添加缺失的 Tensile 数据类型:
   INT4 (I4), FP64 (DD), FP32 (SS)

Tensile 类型代码:
  0=FP32, 1=FP64, 4=FP16, 5=BF16, 6=INT32, 8=INT8, 9=INT4
"""

import os

SRC = "/data/ROCm/rocBLAS/library/src/blas3/Tensile/Logic/asm_full/gfx1200"
DST = SRC  # in-place

# 基于 INT8 (I8II) 模板生成 INT4 (I4II) 和 FP64 (DD) 和 FP32 (SS)

def gen_configs(base_file, new_type, type_code, compute_code):
    """从 I8II 模板生成新类型配置"""
    src_path = os.path.join(SRC, f"gfx1200_{base_file}")
    if not os.path.exists(src_path):
        return []

    with open(src_path) as f:
        content = f.read()

    # 替换类型名
    new_base = base_file.replace("I8II", new_type)

    # 替换 DataType
    content = content.replace("DataType: 8", f"DataType: {type_code}")
    content = content.replace("ComputeDataType: 6", f"ComputeDataType: {compute_code}")
    content = content.replace("DestDataType: 6", f"DestDataType: {compute_code}")

    # 为 INT4 添加 SWMMAC 特有配置
    if new_type == "I4II":
        content = content.replace("SwmmacChainCount: 14", "SwmmacChainCount: 16")
        # INT4 uses 16-chain

    generated = []
    for variant in [f"Cijk_Ailk_Bjlk_{new_type}_BH", f"Cijk_Ailk_Bjlk_{new_type}_BH_GB",
                     f"Cijk_Ailk_Bljk_{new_type}_BH", f"Cijk_Ailk_Bljk_{new_type}_BH_GB",
                     f"Cijk_Alik_Bjlk_{new_type}_BH", f"Cijk_Alik_Bjlk_{new_type}_BH_GB",
                     f"Cijk_Alik_Bljk_{new_type}_BH", f"Cijk_Alik_Bljk_{new_type}_BH_GB"]:
        new_name = f"gfx1200_{variant}.yaml"
        new_path = os.path.join(DST, new_name)
        if not os.path.exists(new_path):
            with open(new_path, 'w') as f:
                f.write(content)
            generated.append(variant)

    return generated

# ─── 1. INT4 (I4II) — 基于 I8II 模板 ───
print("=== INT4 (I4II) ===")
gen_configs("Cijk_Ailk_Bjlk_I8II_BH.yaml", "I4II", 9, 6)
gen_configs("Cijk_Ailk_Bjlk_I8II_BH_GB.yaml", "I4II", 9, 6)
gen_configs("Cijk_Ailk_Bljk_I8II_BH.yaml", "I4II", 9, 6)
gen_configs("Cijk_Ailk_Bljk_I8II_BH_GB.yaml", "I4II", 9, 6)
gen_configs("Cijk_Alik_Bjlk_I8II_BH.yaml", "I4II", 9, 6)
gen_configs("Cijk_Alik_Bjlk_I8II_BH_GB.yaml", "I4II", 9, 6)
gen_configs("Cijk_Alik_Bljk_I8II_BH.yaml", "I4II", 9, 6)
gen_configs("Cijk_Alik_Bljk_I8II_BH_GB.yaml", "I4II", 9, 6)
i4_count = len([f for f in os.listdir(DST) if "I4II" in f])
print(f"  INT4 configs: {i4_count}")

# ─── 2. FP64 (DD) — 基于 HHS 模板 ───
print("\n=== FP64 (DD) ===")
for base in ["Cijk_Ailk_Bjlk_HHS_BH", "Cijk_Ailk_Bljk_HHS_BH",
             "Cijk_Alik_Bjlk_HHS_BH", "Cijk_Alik_Bljk_HHS_BH"]:
    src = os.path.join(SRC, f"gfx1200_{base}.yaml")
    if os.path.exists(src):
        with open(src) as f:
            content = f.read()
        content = content.replace("DataType: 4", "DataType: 1")
        content = content.replace("ComputeDataType: 0", "ComputeDataType: 1")
        content = content.replace("DestDataType: 4", "DestDataType: 1")
        # 移除 StaggeredWorkClaim (FP64 不需要)
        content = content.replace("\n    StaggeredWorkClaim: true\n", "\n")
        content = content.replace("\n    WorkClaimOversubWaves: 8\n", "\n")
        content = content.replace("\n    EnableSwmmac: true\n", "\n")
        content = content.replace("\n    SwmmacChainCount: 14\n", "\n")
        new_name = f"gfx1200_{base.replace('HHS','DD')}.yaml"
        with open(os.path.join(DST, new_name), 'w') as f:
            f.write(content)
dd_count = len([f for f in os.listdir(DST) if "_DD_" in f])
print(f"  FP64 configs: {dd_count}")

# ─── 3. FP32 (SS) — 基于 HHS 模板 ───
print("\n=== FP32 (SS) ===")
for base in ["Cijk_Ailk_Bjlk_HHS_BH", "Cijk_Ailk_Bljk_HHS_BH",
             "Cijk_Alik_Bjlk_HHS_BH", "Cijk_Alik_Bljk_HHS_BH"]:
    src = os.path.join(SRC, f"gfx1200_{base}.yaml")
    if os.path.exists(src):
        with open(src) as f:
            content = f.read()
        content = content.replace("DataType: 4", "DataType: 0")
        content = content.replace("ComputeDataType: 0", "ComputeDataType: 0")
        content = content.replace("DestDataType: 4", "DestDataType: 0")
        new_name = f"gfx1200_{base.replace('HHS','SS')}.yaml"
        with open(os.path.join(DST, new_name), 'w') as f:
            f.write(content)
ss_count = len([f for f in os.listdir(DST) if "_SS_" in f])
print(f"  FP32 configs: {ss_count}")

# ─── 总结 ───
total = len(os.listdir(DST))
print(f"\n=== gfx1200 总计: {total} Tensile 配置 ===")
print(f"  INT4 (I4II): {i4_count} | FP64 (DD): {dd_count} | FP32 (SS): {ss_count}")
print(f"  INT8 (I8II): {len([f for f in os.listdir(DST) if 'I8II' in f])}")
print(f"  FP16 (HHS):  {len([f for f in os.listdir(DST) if 'HHS' in f])}")
print(f"  BF16 (BBS):  {len([f for f in os.listdir(DST) if 'BBS' in f])}")
