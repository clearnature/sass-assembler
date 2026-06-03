#!/usr/bin/env python3
"""persistent counter vs hipMalloc/Free 性能对比"""

import ctypes, torch, time, statistics

sc = torch.cuda.current_stream().cuda_stream

# v1: hipMalloc/Free 每调用 (old kernel)
old = ctypes.CDLL('benchmarks/hip_ported/libstagger_v2.so')
old.cuda_sparse_4k_stagger_v2.argtypes = [ctypes.c_void_p]*4 + [ctypes.c_int]*3 + [ctypes.c_void_p]

# v2: external persistent counter (stable kernel)
new = ctypes.CDLL('benchmarks/hip_ported/libstable.so')
new.cuda_sparse_4k_stable.argtypes = [ctypes.c_void_p]*3 + [ctypes.c_int]*3 + [ctypes.c_void_p]*2

M,N,K = 4096,4096,4096
one = 0x11111111
x = torch.full((N,K//8), one, device='cuda', dtype=torch.int32)
w = torch.full((M,K//8), one, device='cuda', dtype=torch.int32)

def bench(name, fn, warmup=30, iters=500):
    for _ in range(warmup): fn()
    torch.cuda.synchronize()
    times = []
    for _ in range(iters):
        t0 = time.perf_counter()
        fn()
        torch.cuda.synchronize()
        times.append((time.perf_counter()-t0)*1e6)
    return statistics.median(times), statistics.stdev(times) if len(times)>1 else 0

# Test 1: 大GEMM 单次调用开销
yo = torch.zeros(N,M,device='cuda')

# hipMalloc 版本
def call_old():
    old.cuda_sparse_4k_stagger_v2(x.data_ptr(), w.data_ptr(), 0, yo.data_ptr(), N, M, K, sc)
med_old, std_old = bench("old", call_old)

# persistent counter 版本  
yn = torch.zeros(N,M,device='cuda')
cnt = torch.zeros(1, device='cuda', dtype=torch.int32)
def call_new():
    cnt.zero_()
    new.cuda_sparse_4k_stable(x.data_ptr(), w.data_ptr(), yn.data_ptr(), N, M, K, sc, cnt.data_ptr())
med_new, std_new = bench("new", call_new)

print("=== persistent counter vs hipMalloc ===")
print(f"大GEMM 4096³ (单次调用):")
print(f"  hipMalloc版本:    {med_old:.0f}μs ±{std_old:.0f}μs")
print(f"  persistent ctr:   {med_new:.0f}μs ±{std_new:.0f}μs")
print(f"  差异:             {med_new/med_old:.2f}x ({'+' if med_new>med_old else ''}{med_new-med_old:.0f}μs)")
print(f"  结论:             {'hipMalloc 更快' if med_old < med_new else 'persistent 更快'}")

# Test 2: 小GEMM 多次调用
M,N,K = 256,256,256
x_s = torch.full((N,K//8), one, device='cuda', dtype=torch.int32)
w_s = torch.full((M,K//8), one, device='cuda', dtype=torch.int32)
yo_s = torch.zeros(N,M,device='cuda')
yn_s = torch.zeros(N,M,device='cuda')
cnt_s = torch.zeros(1, device='cuda', dtype=torch.int32)

def call_old_small():
    old.cuda_sparse_4k_stagger_v2(x_s.data_ptr(), w_s.data_ptr(), 0, yo_s.data_ptr(), N, M, K, sc)
med_old_s, _ = bench("old_small", call_old_small, warmup=200, iters=5000)

def call_new_small():
    cnt_s.zero_()
    new.cuda_sparse_4k_stable(x_s.data_ptr(), w_s.data_ptr(), yn_s.data_ptr(), N, M, K, sc, cnt_s.data_ptr())
med_new_s, _ = bench("new_small", call_new_small, warmup=200, iters=5000)

print(f"\n小GEMM 256³ (5000次):")
print(f"  hipMalloc版本:    {med_old_s:.0f}μs")
print(f"  persistent ctr:   {med_new_s:.0f}μs")
print(f"  差异:             {med_new_s/med_old_s:.2f}x")
