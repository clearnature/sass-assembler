"""验证 libsovereign_hip.so 在 RX 9060 XT 上可用"""
import torch
import ctypes
import os

so_path = "/data/模型训练精度验证/phase3/ggml/hip/libsovereign_hip.so"
print(f"加载: {so_path}")
print(f"文件大小: {os.path.getsize(so_path)/1024:.0f} KB")

# 加载 .so
lib = ctypes.CDLL(so_path)
print("✅ .so 加载成功")

# 获取函数符号
funcs = ['launch_rmsnorm', 'launch_attn', 'launch_rope', 
         'launch_sparse_gemm', 'launch_embedding', 'launch_add_inplace']
for f in funcs:
    try:
        getattr(lib, f)
        print(f"  ✅ {f}")
    except AttributeError:
        print(f"  ⚠️ {f} (not exported)")

print()
print("GPU:", torch.cuda.get_device_name(0))
print("Compute:", torch.cuda.get_device_capability(0))
print("PyTorch:", torch.__version__)
print("HIP:", torch.version.hip)
print("✅ 验证完成")
