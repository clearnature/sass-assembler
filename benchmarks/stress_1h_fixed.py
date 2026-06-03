#!/usr/bin/env python3
"""1小时 HIP 内存压力测试 — 严格运行 3600 秒, 每5分钟输出"""

import torch, time, gc, os, numpy as np
from datetime import datetime

start = time.time()
total_allocs = alloc_fails = 0
last_report = -1
frag_log = []
sc = torch.cuda.current_stream().cuda_stream

print(f"=== HIP 1小时压力测试 ===")
print(f"GPU: {torch.cuda.get_device_name(0)}")
print(f"开始: {datetime.now().strftime('%H:%M:%S')}")
end_time = datetime.fromtimestamp(datetime.now().timestamp() + 3600)
print(f"结束: 约 {end_time.strftime('%H:%M:%S')}")
print()

while (time.time() - start) < 3600:
    elapsed = (time.time() - start) / 60

    # 模拟训练: 分配→使用→释放
    try:
        N, D = 4096, 4096
        x = torch.empty(N, D, device='cuda')
        w = torch.ones(D, device='cuda')
        y = torch.empty(N, D, device='cuda')
        total_allocs += 3
        del x, w, y
    except: alloc_fails += 1

    try:
        a = torch.empty(1024, 1024, device='cuda')
        b = torch.empty(1024, 1024, device='cuda')
        c = torch.empty(1024, 1024, device='cuda')
        total_allocs += 3
        del a, b, c
    except: alloc_fails += 1

    try:
        s = int(np.random.exponential(100000)) + 256
        t = torch.empty(min(s, 10*1024*1024), device='cuda')
        total_allocs += 1
        del t
    except: alloc_fails += 1

    # 每 5 分钟报告一次
    minutes = int(elapsed)
    if minutes >= 5 and minutes % 5 == 0 and minutes != last_report:
        last_report = minutes
        free, total = torch.cuda.mem_get_info()
        frag = (1 - free/total) * 100
        
        max_alloc = 0
        for test_mb in [512, 256, 128, 64, 32, 16, 8]:
            try:
                n = test_mb * 1024 * 1024 // 4
                t = torch.empty(n, device='cuda'); del t
                max_alloc = test_mb; break
            except: pass

        torch.cuda.empty_cache(); gc.collect()
        after_free, _ = torch.cuda.mem_get_info()
        
        h = int(minutes / 60)
        m = minutes % 60
        print(f"[{h:02d}:{m:02d}] 分配={total_allocs//1000}k "
              f"失败={alloc_fails} 碎片={frag:.1f}% "
              f"max={max_alloc}MB "
              f"GC释放={(after_free-free)/1e9:.2f}GB")

# 最终
free, total = torch.cuda.mem_get_info()
elapsed_min = (time.time() - start) / 60
print(f"\n=== {elapsed_min/60:.1f}小时测试完成 ===")
print(f"总分配: {total_allocs:,} 次")
print(f"失败: {alloc_fails}")
print(f"最终碎片率: {(1-free/total)*100:.1f}%")
print(f"是否稳定: {'✅ 适合训练' if alloc_fails==0 else '❌ 不稳定'}")
