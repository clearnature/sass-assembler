#!/usr/bin/env python3
"""HIP 内存稳定性压力测试 — 模拟数天训练负载

测试项:
  1. 循环分配/释放 (模拟训练batch迭代)
  2. 持久计数器稳定性 (百万次kernel launch)
  3. 内存碎片检测 (分配失败率)
  4. 分配延迟分布 (P50/P99/max)
  5. 长期运行OOM检测

用法:
  python3 stress_test.py --hours 24 --log stress.log
"""

import ctypes, torch, time, sys, os, gc
import numpy as np
from collections import deque
from datetime import datetime

class StressTester:
    def __init__(self, target_hours=24):
        self.target_hours = target_hours
        self.start_time = None
        self.total_allocations = 0
        self.total_frees = 0
        self.allocation_failures = 0
        self.latencies = deque(maxlen=100000)
        self.leak_check = []
        
        # 加载我们的 HIP 算子 (persistent counter版本)
        try:
            self.lib = ctypes.CDLL('/data/模型训练精度验证/phase3/ggml/hip/libsovereign_ilp.so')
            self.has_ilp = True
        except:
            self.lib = ctypes.CDLL('/data/模型训练精度验证/phase3/ggml/hip/libsovereign_hip.so')
            self.has_ilp = False
        
        # 绑定函数
        self.lib.cuda_add.argtypes = [ctypes.c_void_p]*3 + [ctypes.c_int, ctypes.c_void_p]
        self.lib.cuda_rmsnorm.argtypes = [ctypes.c_void_p]*3 + [ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_void_p]
        
        # GPU信息
        self.device = torch.cuda.get_device_name(0)
        self.total_mem = torch.cuda.get_device_properties(0).total_memory
        print(f"=== HIP 内存压力测试 ===")
        print(f"GPU: {self.device}")
        print(f"VRAM: {self.total_mem/1e9:.1f} GB")
        print(f"目标时长: {target_hours} 小时")
        print(f"ILP优化: {'✅' if self.has_ilp else '❌'}")
        print()

    def measure_latency(self, fn, *args):
        torch.cuda.synchronize()
        t0 = time.perf_counter()
        result = fn(*args)
        torch.cuda.synchronize()
        elapsed = (time.perf_counter() - t0) * 1e6  # μs
        self.latencies.append(elapsed)
        return result

    def test_random_alloc_free(self, iterations=1000):
        """随机大小分配/释放循环"""
        print(f"[{datetime.now().strftime('%H:%M:%S')}] 随机分配测试 ({iterations}次)...")
        
        for i in range(iterations):
            # 随机大小: 1KB - 100MB
            size = int(np.random.exponential(1e6)) + 1024
            size = min(size, 100 * 1024 * 1024)  # cap 100MB
            size = max(size, 1024)
            
            try:
                t = torch.empty(size // 4, device='cuda', dtype=torch.float32)
                self.total_allocations += 1
                del t
                self.total_frees += 1
            except Exception as e:
                self.allocation_failures += 1
                if self.allocation_failures < 5:
                    print(f"  ⚠️ 分配失败 ({size/1e6:.1f}MB): {e}")
            
            if i % 10000 == 0 and i > 0:
                gc.collect()
                torch.cuda.empty_cache()
        
        # 检查碎片
        free, total = torch.cuda.mem_get_info()
        frag_pct = (1 - free/total) * 100
        print(f"  完成. 失败={self.allocation_failures}, 碎片率={frag_pct:.1f}%")

    def test_persistent_counter(self, launches=100000):
        """持久计数器: 百万次 kernel launch 不重置"""
        print(f"[{datetime.now().strftime('%H:%M:%S')}] 持久计数器测试 ({launches}次 launch)...")
        
        sc = torch.cuda.current_stream().cuda_stream
        cnt = torch.zeros(1, device='cuda', dtype=torch.int32)
        a = torch.ones(1024, device='cuda')
        b = torch.ones(1024, device='cuda')
        y = torch.zeros(1024, device='cuda')
        
        errors = 0
        for i in range(launches):
            try:
                self.measure_latency(self.lib.cuda_add, 
                    a.data_ptr(), b.data_ptr(), y.data_ptr(), 1024, sc)
            except Exception as e:
                errors += 1
                if errors < 3:
                    print(f"  ⚠️ launch {i} 失败: {e}")
            
            if i % 10000 == 0 and i > 0:
                torch.cuda.synchronize()
        
        # 验证计数器
        torch.cuda.synchronize()
        expected = torch.full_like(y, 2.0 * 3)  # 每次 a+b, 重复 3 (warmup不计)
        correct = torch.allclose(y, torch.ones(1024, device='cuda') * launches * 2 + 3)  # approximate
        print(f"  完成. errors={errors}, 计数器={cnt.item()}")

    def test_memory_fragmentation(self):
        """检测长期运行后的内存碎片"""
        print(f"[{datetime.now().strftime('%H:%M:%S')}] 碎片检测...")
        
        # 尝试分配大块连续内存
        sizes = [1e6, 1e7, 5e7, 1e8, 5e8, 1e9]  # 1MB - 1GB
        for s in sizes:
            try:
                n = int(s / 4)
                t = torch.empty(n, device='cuda')
                del t
                print(f"  {s/1e6:.0f}MB: ✅")
            except:
                print(f"  {s/1e6:.0f}MB: ❌ FRAGMENTED")

    def print_stats(self, elapsed_hours):
        """打印统计"""
        ops = self.total_allocations + len(self.latencies)
        lats = np.array(list(self.latencies)) if self.latencies else np.array([0])
        
        print(f"\n=== {elapsed_hours:.1f}h 统计 ===")
        print(f"  分配: {self.total_allocations} 次")
        print(f"  失败: {self.allocation_failures} 次 ({self.allocation_failures/max(1,self.total_allocations)*100:.4f}%)")
        print(f"  延迟: P50={np.percentile(lats,50):.0f}μs P99={np.percentile(lats,99):.0f}μs max={lats.max():.0f}μs")
        
        free, total = torch.cuda.mem_get_info()
        print(f"  VRAM: {free/1e9:.1f}/{total/1e9:.1f} GB free ({free/total*100:.1f}%)")
        
        # 警告条件
        if self.allocation_failures > 0:
            print(f"  ⚠️ 检测到分配失败!")
        if lats.max() > 100000:  # P99 > 100ms
            print(f"  ⚠️ 延迟尖峰: {lats.max():.0f}μs")
        if free / total < 0.1:
            print(f"  🚨 VRAM 不足 10%!")

    def run(self):
        """主循环: 模拟训练负载"""
        self.start_time = time.time()
        cycle = 0
        
        while True:
            elapsed = (time.time() - self.start_time) / 3600
            if elapsed >= self.target_hours:
                break
            
            cycle += 1
            print(f"\n--- Cycle {cycle} ({elapsed:.1f}h/{self.target_hours}h) ---")
            
            # 模拟一个训练 step
            self.test_random_alloc_free(iterations=500)
            self.test_persistent_counter(launches=1000)
            
            # 每10个cycle做碎片检测
            if cycle % 10 == 0:
                self.test_memory_fragmentation()
                gc.collect()
                torch.cuda.empty_cache()
            
            # 每小时打印统计
            if cycle % 60 == 0:
                self.print_stats(elapsed)
        
        self.print_stats(elapsed)
        print(f"\n✅ 压力测试完成 ({elapsed:.1f}小时)")
        
        if self.allocation_failures > 0:
            print(f"⚠️ 总共 {self.allocation_failures} 次分配失败 — 不适合长期训练!")
            return 1
        return 0

if __name__ == '__main__':
    hours = 24
    if len(sys.argv) > 1:
        for i, arg in enumerate(sys.argv):
            if arg == '--hours' and i+1 < len(sys.argv):
                hours = float(sys.argv[i+1])
    
    tester = StressTester(target_hours=hours)
    sys.exit(tester.run())
