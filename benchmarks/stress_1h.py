#!/usr/bin/env python3
"""1小时 HIP 内存压力测试 — 碎片率追踪"""

import ctypes, torch, time, gc, os
import numpy as np
from datetime import datetime

# 加载算子
lib = ctypes.CDLL('/data/模型训练精度验证/phase3/ggml/hip/libsovereign_ilp.so')
lib.cuda_add.argtypes = [ctypes.c_void_p]*3 + [ctypes.c_int, ctypes.c_void_p]
lib.cuda_rmsnorm.argtypes = [ctypes.c_void_p]*3 + [ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_void_p]

sc = torch.cuda.current_stream().cuda_stream
frag_history = []
start = time.time()
errors = 0
alloc_fails = 0
total_allocs = 0

print(f"=== 1小时 HIP 压力测试 ===")
print(f"GPU: {torch.cuda.get_device_name(0)}")
print(f"开始: {datetime.now().strftime('%H:%M:%S')}")
print()

while (time.time() - start) < 3600:
    elapsed = (time.time() - start) / 60  # minutes
    
    # 模拟训练: 分配→计算→释放循环
    # batch 1: RMSNorm
    try:
        N, D = 4096, 4096
        x = torch.empty(N, D, device='cuda')
        w = torch.ones(D, device='cuda')
        y = torch.empty(N, D, device='cuda')
        total_allocs += 3
        lib.cuda_rmsnorm(x.data_ptr(), w.data_ptr(), y.data_ptr(), N, D, 1e-5, sc)
        del x, w, y
    except Exception as e:
        alloc_fails += 1
    
    # batch 2: matmul tensors  
    try:
        a = torch.empty(1024, 1024, device='cuda')
        b = torch.empty(1024, 1024, device='cuda')
        c = torch.empty(1024, 1024, device='cuda')
        total_allocs += 3
        del a, b, c
    except:
        alloc_fails += 1
    
    # batch 3: 随机大小分配
    try:
        size = int(np.random.exponential(100000)) + 256
        size = min(size, 10*1024*1024)
        t = torch.empty(size, device='cuda')
        total_allocs += 1
        del t
    except:
        alloc_fails += 1
    
    # 每 5 分钟测量碎片率
    if elapsed > 0 and int(elapsed) % 5 == 0 and int(elapsed) != (len(frag_history)*5 if frag_history else -1):
        free, total = torch.cuda.mem_get_info()
        frag = (1 - free/total) * 100
        
        # 测试最大可分配块
        max_alloc = 0
        for test_mb in [512, 256, 128, 64, 32, 16, 8, 4, 1]:
            try:
                n = test_mb * 1024 * 1024 // 4
                t = torch.empty(n, device='cuda')
                del t
                max_alloc = test_mb
                break
            except:
                pass
        
        frag_history.append({
            'minute': int(elapsed),
            'frag_pct': frag,
            'max_alloc_mb': max_alloc,
            'alloc_fails': alloc_fails,
            'total_allocs': total_allocs,
        })
        
        torch.cuda.empty_cache()
        gc.collect()
        
        h = frag_history[-1]
        print(f"[{int(elapsed//60):02d}:{int(elapsed%60):02d}] "
              f"碎片={h['frag_pct']:.1f}% max_alloc={h['max_alloc_mb']}MB "
              f"失败={h['alloc_fails']}/{h['total_allocs']}")

# 最终报告
free, total = torch.cuda.mem_get_info()
print(f"\n=== 1小时测试完成 ===")
print(f"总分配: {total_allocs} 次")
print(f"失败: {alloc_fails} 次 ({alloc_fails/max(1,total_allocs)*100:.4f}%)")
print(f"最终碎片率: {(1-free/total)*100:.1f}%")
print(f"VRAM: {free/1e9:.1f}/{total/1e9:.1f} GB")

if frag_history:
    frags = [f['frag_pct'] for f in frag_history]
    print(f"\n碎片率趋势:")
    print(f"  初始: {frags[0]:.1f}%")
    print(f"  最终: {frags[-1]:.1f}%")
    print(f"  变化: {frags[-1]-frags[0]:+.1f}%")
    print(f"  是否增长: {'⚠️ 持续增长!' if frags[-1] > frags[0] * 2 else '✅ 稳定'}")

# 写入日志
with open('benchmarks/stress_log.txt', 'w') as f:
    f.write(f"total_allocs={total_allocs}\n")
    f.write(f"alloc_fails={alloc_fails}\n")
    for h in frag_history:
        f.write(f"minute={h['minute']} frag={h['frag_pct']:.2f}% max={h['max_alloc_mb']}MB\n")
print("日志: benchmarks/stress_log.txt")
