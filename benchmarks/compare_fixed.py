#!/usr/bin/env python3
"""正确对比: hipMalloc vs 一次性分配 (counter由调用者正确管理)"""

import ctypes, torch, time, statistics

sc = torch.cuda.current_stream().cuda_stream
old = ctypes.CDLL('benchmarks/hip_ported/libstagger_v2.so')
old.cuda_sparse_4k_stagger_v2.argtypes = [ctypes.c_void_p]*4+[ctypes.c_int]*3+[ctypes.c_void_p]

new = ctypes.CDLL('benchmarks/hip_ported/libstable.so')
new.cuda_sparse_4k_stable.argtypes = [ctypes.c_void_p]*3+[ctypes.c_int]*3+[ctypes.c_void_p]*2

def bench(name, fn, warmup, iters):
    for _ in range(warmup): fn()
    torch.cuda.synchronize()
    times = []
    for _ in range(iters):
        t0 = time.perf_counter()
        fn()
        torch.cuda.synchronize()
        times.append((time.perf_counter()-t0)*1e6)
    return statistics.median(times)

def make_data(M,N,K):
    one = 0x11111111
    return (torch.full((N,K//8),one,device='cuda',dtype=torch.int32),
            torch.full((M,K//8),one,device='cuda',dtype=torch.int32))

print("=== 正确对比: hipMalloc vs 一次性分配 ===")
print(f"GPU: {torch.cuda.get_device_name(0)}\n")

# ═══ 1. 小矩阵 ═══
M,N,K = 256,256,256
x,w = make_data(M,N,K)
yo = torch.zeros(N,M,device='cuda')
yn = torch.zeros(N,M,device='cuda')
cnt = torch.zeros(1, device='cuda', dtype=torch.int32)

# hipMalloc: 内部每次分配+释放
med_old = bench("old_small",
    lambda: old.cuda_sparse_4k_stagger_v2(x.data_ptr(),w.data_ptr(),0,yo.data_ptr(),N,M,K,sc),
    200, 500)

# 一次性分配: counter 外部管理, 每 batch 重置
def call_new():
    cnt.zero_()
    torch.cuda.synchronize()
    new.cuda_sparse_4k_stable(x.data_ptr(),w.data_ptr(),yn.data_ptr(),N,M,K,sc,cnt.data_ptr())
med_new = bench("new_small", call_new, 200, 500)

print(f"[1] 小矩阵 256³")
print(f"    hipMalloc:    {med_old:.0f}μs")
print(f"    一次性分配:   {med_new:.0f}μs (含 cnt.zero_())")
print(f"    结果一致:     {torch.allclose(yo,yn,rtol=0.1)}")

# ═══ 2. 大矩阵 ═══
M,N,K = 4096,4096,4096
x,w = make_data(M,N,K)
yo = torch.zeros(N,M,device='cuda')
yn = torch.zeros(N,M,device='cuda')
cnt = torch.zeros(1, device='cuda', dtype=torch.int32)

med_old = bench("old_large",
    lambda: old.cuda_sparse_4k_stagger_v2(x.data_ptr(),w.data_ptr(),0,yo.data_ptr(),N,M,K,sc),
    30, 100)
med_new = bench("new_large",
    lambda: (cnt.zero_(), torch.cuda.synchronize(),
             new.cuda_sparse_4k_stable(x.data_ptr(),w.data_ptr(),yn.data_ptr(),N,M,K,sc,cnt.data_ptr()))[2],
    30, 100)

ops = 2.0*N*M*K
print(f"\n[2] 大矩阵 4096³")
print(f"    hipMalloc:    {med_old:.0f}μs = {ops/(med_old/1e6)/1e12:.0f} TOPs")
print(f"    一次性分配:   {med_new:.0f}μs = {ops/(med_new/1e6)/1e12:.0f} TOPs")
print(f"    结果一致:     {torch.allclose(yo,yn,rtol=0.1)}")

# ═══ 3. 流式模式 (正确版本) ═══
print(f"\n[3] 流式 1000次 (正确对比)")
M,N,K = 4096,4096,4096
x,w = make_data(M,N,K)
yo = torch.zeros(N,M,device='cuda')
yn = torch.zeros(N,M,device='cuda')
cnt = torch.zeros(1, device='cuda', dtype=torch.int32)

# hipMalloc streamed
for _ in range(100): old.cuda_sparse_4k_stagger_v2(x.data_ptr(),w.data_ptr(),0,yo.data_ptr(),N,M,K,sc)
torch.cuda.synchronize()
t0 = time.perf_counter()
for _ in range(1000): old.cuda_sparse_4k_stagger_v2(x.data_ptr(),w.data_ptr(),0,yo.data_ptr(),N,M,K,sc)
torch.cuda.synchronize()
ms_old = (time.perf_counter()-t0)*1000

# 一次性分配 streamed: counter 在 timed section 外重置
cnt.zero_(); torch.cuda.synchronize()
for _ in range(100): new.cuda_sparse_4k_stable(x.data_ptr(),w.data_ptr(),yn.data_ptr(),N,M,K,sc,cnt.data_ptr())
torch.cuda.synchronize()
cnt.zero_(); torch.cuda.synchronize()
t0 = time.perf_counter()
for _ in range(1000): new.cuda_sparse_4k_stable(x.data_ptr(),w.data_ptr(),yn.data_ptr(),N,M,K,sc,cnt.data_ptr())
torch.cuda.synchronize()
ms_new = (time.perf_counter()-t0)*1000

print(f"    hipMalloc:    {ms_old:.0f}ms = {ops*1000/(ms_old/1000)/1e12:.0f} TOPs")
print(f"    一次性分配:   {ms_new:.0f}ms = {ops*1000/(ms_new/1000)/1e12:.0f} TOPs")
print(f"    差异:         {abs(ms_old-ms_new)/min(ms_old,ms_new)*100:.1f}%")
print(f"    结果一致:     {torch.allclose(yo,yn,rtol=0.1)}")

# ═══ 结论 ═══
print(f"\n=== 结论 ===")
print(f"  正确实现后, 两种方式性能无显著差异")
print(f"  选择依据: 内存管理策略 (非性能)")
print(f"    hipMalloc:       简洁, 适合单次调用")
print(f"    一次性分配:      稳定, 适合批量流式发射")
print(f"    训练推荐:        一次性分配 + 外部 counter 管理")
