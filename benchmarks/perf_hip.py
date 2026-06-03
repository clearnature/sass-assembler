"""RX 9060 XT 性能基准 — HIP 算子 vs PyTorch 原生"""
import ctypes, torch, time
import torch.utils.benchmark as bench

lib = ctypes.CDLL('/data/模型训练精度验证/phase3/ggml/hip/libsovereign_hip.so')

# 设置 argtypes
lib.cuda_rmsnorm.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_void_p]
lib.cuda_attn.argtypes = [ctypes.c_void_p]*4 + [ctypes.c_int]*4 + [ctypes.c_bool, ctypes.c_void_p]
lib.cuda_add.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]

stream = torch.cuda.current_stream()
WARMUP = 5
ITERS = 100

def bench_fn(name, fn, *args):
    for _ in range(WARMUP): fn(*args)
    torch.cuda.synchronize()
    t0 = time.perf_counter()
    for _ in range(ITERS): fn(*args)
    torch.cuda.synchronize()
    t1 = time.perf_counter()
    return (t1-t0)/ITERS*1000

print("=== RX 9060 XT HIP 算子性能 ===")
print(f"GPU: {torch.cuda.get_device_name(0)}")
print(f"PyTorch: {torch.__version__}  HIP: {torch.version.hip}")
print()

# 1. RMSNorm
N, D = 4096, 4096
x = torch.randn(N, D, device='cuda')
w = torch.ones(D, device='cuda')
y_hip = torch.zeros(N, D, device='cuda')

ms_hip = bench_fn('rmsnorm_hip', lib.cuda_rmsnorm, x.data_ptr(), w.data_ptr(), y_hip.data_ptr(), N, D, 1e-5, stream.cuda_stream)
ms_torch = bench_fn('rmsnorm_torch', lambda: torch.nn.functional.rms_norm(x, [D]))
print(f"RMSNorm {N}×{D}: HIP={ms_hip:.3f}ms  PyTorch={ms_torch:.3f}ms  ratio={ms_torch/ms_hip:.2f}x")

# 2. add
N = 4096*4096
a = torch.randn(N, device='cuda')
b = torch.randn(N, device='cuda')
y_add = torch.zeros(N, device='cuda')

ms_hip_add = bench_fn('add_hip', lib.cuda_add, a.data_ptr(), b.data_ptr(), y_add.data_ptr(), N, stream.cuda_stream)
ms_torch_add = bench_fn('add_torch', lambda: a.add_(b))
print(f"add {N/1e6:.0f}M: HIP={ms_hip_add:.3f}ms  PyTorch={ms_torch_add:.3f}ms  ratio={ms_torch_add/ms_hip_add:.2f}x")

# 3. 内存带宽
bytes_rw = N * 4 * 3  # a读+b读+y写
bw = bytes_rw / (ms_hip_add/1000) / 1e9
print(f"内存带宽: {bw:.1f} GB/s")

print()
print("完成")
