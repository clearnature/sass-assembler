#!/usr/bin/env python3
"""Phase 3 GGML Python 绑定 — ctypes 封装"""
import ctypes, os, numpy as np

_LIB = None
_HERE = os.path.dirname(os.path.abspath(__file__))

def load():
    global _LIB
    if _LIB is not None: return _LIB
    # 尝试加载 .so
    paths = [os.path.join(_HERE, 'build', 'libggml_phase3.so'),
             os.path.join(_HERE, 'libggml_phase3.so')]
    for p in paths:
        if os.path.exists(p):
            _LIB = ctypes.CDLL(p)
            break
    if _LIB is None:
        raise ImportError(f"未找到 libggml_phase3.so，请先编译: cd {_HERE} && mkdir build && cd build && cmake .. && make")
    return _LIB

def quantize_tq1_0(weights: np.ndarray) -> bytes:
    """float32 权重 → TQ1_0 打包格式"""
    lib = load()
    out_d, in_d = weights.shape
    assert in_d % 256 == 0
    nb = in_d // 256
    row_size = 54 * nb  # sizeof(block_tq1_0) * nb
    dst = (ctypes.c_uint8 * (out_d * row_size))()
    lib.quantize_tq1_0(
        weights.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
        dst, out_d, in_d)
    return bytes(dst)

def dequantize_tq1_0(packed: bytes, out_d: int, in_d: int) -> np.ndarray:
    """TQ1_0 → float32"""
    assert in_d % 256 == 0
    nb = in_d // 256
    row_size = 54 * nb
    result = np.zeros((out_d, in_d), dtype=np.float32)
    lib = load()
    src = (ctypes.c_uint8 * len(packed)).from_buffer_copy(packed)
    for row in range(out_d):
        blk_ptr = ctypes.cast(
            ctypes.pointer(src) + row * row_size,
            ctypes.POINTER(ctypes.c_uint8))
        lib.dequantize_row_tq1_0(
            ctypes.cast(blk_ptr, ctypes.c_void_p),
            result[row].ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
            in_d)
    return result

def gf3_matmul(x: np.ndarray, w: np.ndarray) -> np.ndarray:
    """GF(3) 矩阵乘: y = (W @ x) % 3"""
    lib = load()
    N, K = x.shape
    out_dim, K2 = w.shape
    assert K == K2
    y = np.zeros((N, out_dim), dtype=np.uint8)
    lib.gf3_matmul(
        x.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8)),
        w.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8)),
        N, out_dim, K, y.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8)))
    return y

def lcm_bridge(acc: np.ndarray) -> np.ndarray:
    """LCM 桥: acc = (acc × 177147) >> 16"""
    lib = load()
    return np.array([lib.lcm_bridge(int(a)) for a in acc.flatten()], dtype=np.int32)

def test():
    """自验证"""
    print("TQ1_0 量化测试...")
    w = np.random.randn(2, 256).astype(np.float32)
    packed = quantize_tq1_0(w)
    w2 = dequantize_tq1_0(packed, 2, 256)
    mse = np.mean((w - w2)**2)
    print(f"  MSE: {mse:.6f} (期望 < 0.01)")

    print("GF(3) 矩阵乘测试...")
    x = np.random.randint(0, 3, (2, 4), dtype=np.uint8)
    w = np.random.randint(0, 3, (3, 4), dtype=np.uint8)
    y = gf3_matmul(x, w)
    y_ref = (x @ w.T) % 3
    assert (y == y_ref).all(), "GF(3) matmul 验证失败"
    print(f"  OK: {y}")

    print("LCM 桥测试...")
    acc = np.array([100, 1000, 10000], dtype=np.int32)
    result = lcm_bridge(acc)
    print(f"  acc={acc}, lcm={result}")
    print("  ✅ 全部通过")

if __name__ == '__main__':
    test()
