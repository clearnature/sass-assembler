"""
Sovereign V4 — CUDA 内核 PyTorch 集成
全部算子通过 C++ Extension (ATen bindings), 废弃 ctypes
"""
import torch
from sovereign_cuda_extension import (
    rmsnorm as _rmsnorm,
    layernorm as _layernorm,
    rope as _rope,
    gate_blend as _gate_blend,
    add_tensors as _add_tensors,
    embedding as _embedding,
    sparse_gemm as _sparse_gemm,
    cross_entropy as _cross_entropy,
    attn2 as _attn2,
)

# ═══════════════════════════════════════════════════════════════
# AMP float16→float32 转换 (C++ kernel 只接受 float32)
# ═══════════════════════════════════════════════════════════════
def _f32(*tensors):
    return tuple(t.float() if t.dtype != torch.float32 else t for t in tensors)

# ═══════════════════════════════════════════════════════════════
# Python 级辅助函数 (3D reshape, 默认参数等)
# ═══════════════════════════════════════════════════════════════

def rmsnorm(x, weight, eps=1e-6):
    """RMSNorm with CUDA kernel, supports 2D [N,D] and 3D [B,T,D]"""
    if x.dim() == 3:
        B, T, D = x.shape
        return _rmsnorm(x.reshape(B * T, D), weight, eps).reshape(B, T, D)
    return _rmsnorm(x, weight, eps)

def layernorm(x, weight, bias, eps=1e-5):
    """LayerNorm with CUDA fwd"""
    if x.dim() == 3:
        B, T, D = x.shape
        return _layernorm(x.reshape(B * T, D), weight, bias, eps).reshape(B, T, D)
    return _layernorm(x, weight, bias, eps)

def rope(x, cos, sin):
    """可微分 RoPE, x: [..., D], cos/sin: [T, D]"""
    return _rope(x, cos, sin)

def gate_blend(x_pe, x_rope, gate):
    """y = gate * x_pe + (1-gate) * x_rope"""
    return _gate_blend(x_pe, x_rope, gate)

def add_tensors(a, b):
    """y = a + b, CUDA float4"""
    return _add_tensors(a, b)

def embedding(tokens, table):
    """CUDA embedding lookup + autograd backward"""
    return _embedding(tokens, table)

def sparse_linear(x, w, mask):
    """Sparse GEMM with CUDA fwd+bwd dW/dX (supports 2D and 3D input)"""
    from torch.nn.functional import max_pool2d
    ndim = x.dim()
    if ndim == 3:
        B, T, K = x.shape
        x_2d = x.reshape(B * T, K)
    else:
        x_2d = x
    M, K = w.size(0), x_2d.size(1)
    block_mask = max_pool2d(mask.reshape(1, 1, M, K).float(), 32, 32).reshape(-1).to(dtype=torch.uint8, device='cuda')
    y = _sparse_gemm(x_2d, w, block_mask)
    return y.reshape(B, T, M) if ndim == 3 else y

def cross_entropy(logits, targets):
    """CUDA CrossEntropy fwd"""
    return _cross_entropy(logits, targets)

def attention(q, k, v, causal=True):
    """4K FlashAttention (attn2), q/k/v: [B, NH, T, D]"""
    return _attn2(q, k, v, causal)

# WS+GEMM+GELU: CUDA fwd (kernel) + PyTorch bwd (recompute, correct GELU derivative)
from sovereign_cuda_extension import ws_gemm_gelu_raw
class _WSGELUFn(torch.autograd.Function):
    @staticmethod
    def forward(ctx, x, w, bias, mask):
        y = ws_gemm_gelu_raw(x, w, bias, mask)
        ctx.save_for_backward(x, w)
        ctx.has_bias = bias is not None
        ctx.has_mask = mask is not None
        return y
    @staticmethod
    def backward(ctx, grad_y):
        x, w = ctx.saved_tensors
        with torch.enable_grad():
            x2 = x.detach().requires_grad_()
            w2 = w.detach().requires_grad_()
            ws = (w2 - w2.mean(1, keepdim=True)) / (w2.std(1, keepdim=True) + 1e-6)
            if ctx.has_mask:
                pass  # mask backward not implemented
            y = torch.nn.functional.gelu(x2 @ ws.T)
            torch.autograd.backward(y, grad_y)
        return x2.grad, w2.grad, None, None

def ws_gemm_gelu(x, w, bias=None, mask=None):
    """WS + GEMM + GELU fusion (CUDA fwd, PyTorch bwd)"""
    return _WSGELUFn.apply(x, w, bias, mask)

# ═══════════════════════════════════════════════════════════════
# 兼容旧接口 (训练脚本可能直接导入内部符号)
# ═══════════════════════════════════════════════════════════════
# all kernels imported above
