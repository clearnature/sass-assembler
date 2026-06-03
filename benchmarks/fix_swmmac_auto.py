#!/usr/bin/env python3
"""修复 rocblas_gemm_ex_kernels.cpp: gfx1200 自动启用 SWMMAC"""
import re

path = "/data/ROCm/rocBLAS/library/src/blas_ex/rocblas_gemm_ex_kernels.cpp"
with open(path) as f:
    content = f.read()

old = '''        static int swmmac_enabled = [](){
            const char* env = getenv("ROCBLAS_SWMMAC_INT4");
            return env ? atoi(env) : 0;
        }();'''

new = '''        static int swmmac_enabled = [](){
            // Auto-enable SWMMAC for gfx1200+ (RDNA4), bypassing Tensile
            const char* env = getenv("ROCBLAS_SWMMAC_INT4");
            if (env) return atoi(env);  // manual override
            hipDeviceProp_t prop;
            hipGetDeviceProperties(&prop, 0);
            return strstr(prop.gcnArchName, "gfx12") ? 1 : 0;  // auto-detect
        }();'''

if old in content:
    content = content.replace(old, new)
    with open(path, 'w') as f:
        f.write(content)
    print("✅ gfx1200 auto-detect SWMMAC added")
else:
    print("❌ pattern not found — checking actual content:")
    for line in content.split('\n'):
        if 'swmmac_enabled' in line or 'SWMMAC_INT4' in line or 'atoi(env)' in line:
            print(f"  {line}")
