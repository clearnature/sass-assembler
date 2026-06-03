# Sovereign V3.2 — CPU 内核 (AVX2+FMA)

> Intel Xeon E5-2686 v4 (Broadwell-EP, 18C/36T, AVX2+FMA)  
> 最后更新: 2026-05-24

## 内核清单

| 库 | 函数 | 说明 |
|------|------|------|
| `libggml_fused_attn.so` | `fused_attn` | FlashAttention, TILE_Q=32, TILE_K=16, online softmax |
| | `fused_gated_attn` | 双轨门控注意力 (RoPE+PE 分支) |
| `libggml_fused_norm.so` | `fused_rmsnorm` | RMSNorm forward (AVX2, 2.2ms @ 8192×768) |
| | `fused_rmsnorm_add` | RMSNorm + 残差 |
| | `fused_rmsnorm_bwd` | RMSNorm backward (dx, dw, AVX2+OpenMP) |
| `libggml_fused_rope.so` | `rope_apply` | RoPE 旋转位置编码 |
| | `rope_precompute` | 预计算 cos/sin 表 |
| `libggml_fused_gate.so` | `n14_fused_gate_blend` | N14 门控混合 (PE/RoPE) |
| | `n14_gate_tick` | N14 时钟步进 |
| `libggml_fused.so` | `fused_embedding` | Token 嵌入查表 |
| | `fused_add_inplace` | 残差连接 |
| | `fused_cross_entropy` | 交叉熵损失 |
| `libggml_sparse.so` | `sparse_gemm` | 32×32 块稀疏 GEMM FW |
| | `sparse_gemm_gelu` | 稀疏 GEMM + GELU |
| | `sparse_gemm_bwd_dw` | 稀疏 GEMM BW: dW = X^T @ dY |
| | `sparse_gemm_bwd_dx` | 稀疏 GEMM BW: dX = dY @ W^T |
| `libsovereign.so` | `fused_layernorm` | LayerNorm (AVX2) |
| | `fused_embedding` | 嵌入查表 |

## 编译

```bash
cd /data/模型训练精度验证/phase3/ggml/cpu

# 各内核编译 (需要 AVX2 + FMA 支持)
gcc -O3 -mavx2 -mfma -fopenmp -fPIC -shared -o libggml_fused_attn.so ggml_fused_attn.c -lm
gcc -O3 -mavx2 -mfma -fopenmp -fPIC -shared -o libggml_fused_norm.so ggml_fused_norm.c -lm
gcc -O3 -mavx2 -mfma -fopenmp -fPIC -shared -o libggml_fused_rope.so ggml_fused_rope.c -lm
gcc -O3 -mavx2 -mfma -fopenmp -fPIC -shared -o libggml_fused_gate.so ggml_fused_gate.c -lm
gcc -O3 -mavx2 -mfma -fopenmp -fPIC -shared -o libggml_fused.so ggml_fused.c -lm
gcc -O3 -mavx2 -mfma -fopenmp -fPIC -shared -o libggml_sparse.so ggml_sparse.c -lm
gcc -O3 -mavx2 -mfma -fopenmp -fPIC -shared -o libsovereign.so ggml_final.c -lm
```

## Python 调用

```python
import ctypes, numpy as np

lib = ctypes.CDLL('./libggml_fused_attn.so')
lib.fused_attn.argtypes = [ctypes.c_void_p]*4 + [ctypes.c_int]*4 + [ctypes.c_bool]

def cpu_attention(q, k, v, causal=True):
    B, NH, T, D = q.shape
    o = np.zeros((B*NH*T*D), dtype=np.float32)
    qc = np.ascontiguousarray(q.reshape(-1, T, D))
    kc = np.ascontiguousarray(k.reshape(-1, T, D))
    vc = np.ascontiguousarray(v.reshape(-1, T, D))
    lib.fused_attn(qc.ctypes.data, kc.ctypes.data, vc.ctypes.data,
                   o.ctypes.data, B, NH, T, D, causal)
    return o.reshape(B, NH, T, D)
```

## 架构探针验证

```
Xeon E5-2686 v4 (Broadwell-EP):
  L1d: 32KB @ 0.7ns       — 18KB 工作集完全适配
  L2:  256KB @ 3.3ns      — 每核独占
  L3:  45MB @ 4.1ns       — 共享 LLC
  FMA: 5 cycle latency, 2/cycle throughput (ports 0,1)
  执行端口: 8 (0,1,5=FP, 2,3=Load, 4=Store, 6,7=INT)
  
探针代码: /data/rtl-sdr/cpu_probe/
结果: /data/rtl-sdr/cpu_probe/results_v2.txt
```

## 性能参考

| 算子 | 尺寸 | 时间 |
|------|------|------|
| Attention | T=4096, B=1 | 301ms |
| Attention | T=512, B=1 | 5ms |
| RMSNorm FW | 8192×768 | 2.2ms |
| RMSNorm BW | 8192×768 | 3.7ms |
| SGEMM | 1024×1024 | 61 GFLOPS (单核) |

## 与 CUDA 版本的对应

| CUDA (GPU) | CPU | 算法一致性 |
|------|------|-----------|
| `attn2.cu` (7.6ms) | `ggml_fused_attn.c` (301ms) | TILE_K=16, online softmax |
| `rmsnorm_bwd.cu` | `ggml_fused_norm.c` | 解析式 backward |
| `sparse_bwd_4k.cu` | `ggml_sparse.c` | 32×32 块稀疏 |
| `cuda_ops.cu` (RoPE) | `ggml_fused_rope.c` | AVX2 SIMD |
| `cuda_ops.cu` (Gate) | `ggml_fused_gate.c` | N14 时钟 |

## 优化记录

| 优化 | 时间 | 说明 |
|------|------|------|
| 基础 AVX2 | 357ms | 标量 expf, TILE_Q=32, TILE_K=16 |
| 双累加器+交错V | 312ms | s0/s1 双流, 2路V并行 |
| vhaddps→vextract+vaddps | 301ms | 归约从 10cyc 降到 6cyc |
| vs oneDNN | 154ms | PyTorch用大GEMM, 无法在分块内核中复制 |

## perf stat 分析 (T=4096, B=1)

```
单次 Forward:
  Instructions:     3.00B
  AVX2 FMA 指令:   420M  (6.8%)
  IPC:             0.97  (4-wide, 25%利用率)
  L1d miss:        0.81% ← 18KB 工作集适配 32KB L1d
  LLC miss:        5.28%
  Branch miss:     0.32%
  
瓶颈: expf延迟 + 控制流 (93.2%), 非计算 (6.8%) 非缓存 (0.81%)
```
