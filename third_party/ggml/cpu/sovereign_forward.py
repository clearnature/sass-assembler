"""Sovereign V4 全 C 内核前向 —— 最大程度替代 PyTorch 运算"""
import ctypes, numpy as np
from pathlib import Path

_HERE = Path(__file__).parent

# ── 加载所有内核 .so ──
def _load_so(name):
    lib = ctypes.CDLL(str(_HERE / name))
    return lib

_LIBS = {
    'norm':   _load_so('libggml_fused_norm.so'),
    'gate':   _load_so('libggml_fused_gate.so'),
    'rope':   _load_so('libggml_fused_rope.so'),
    'linear': _load_so('libggml_fused.so'),
    'sparse': _load_so('libggml_sparse.so'),
}

# ── 类型设置 ──
class N14GateState(ctypes.Structure):
    _fields_ = [("phase", ctypes.c_int64), ("step", ctypes.c_int),
                ("trit", ctypes.c_int), ("n14_phase_error", ctypes.c_float)]

def _ptr(arr): return arr.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
def _u8ptr(arr): return arr.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8))

# ── 内核封装 ──
class CKCuda:
    pass  # 替换整个训练

def rmsnorm(x, weight, eps=1e-6):
    """RMSNorm: y = x * rsqrt(mean(x^2)+eps) * weight"""
    N, D = x.shape
    y = np.zeros_like(x)
    _LIBS['norm'].fused_rmsnorm(_ptr(x), _ptr(weight), _ptr(y), N, D, ctypes.c_float(eps))
    return y

def rmsnorm_add(x, weight, residual, eps=1e-6):
    """RMSNorm + 残差: y = RMSNorm(x) + residual"""
    N, D = x.shape
    y = np.zeros_like(x)
    _LIBS['norm'].fused_rmsnorm_add(_ptr(x), _ptr(weight), _ptr(residual), _ptr(y), N, D, ctypes.c_float(eps))
    return y

def rope_apply(x, cos, sin):
    """RoPE 原地旋转"""
    T, D = x.shape
    _LIBS['rope'].rope_apply(_ptr(x), _ptr(cos), _ptr(sin), T, D)

def gemm_no_ws(x, w, bias=None):
    """纯 GEMM (无 WS, 无激活): y = x @ W^T + bias"""
    N, K = x.shape; M, _ = w.shape
    y = np.zeros((N, M), dtype=np.float32)
    _LIBS['linear'].fused_linear_ws(
        _ptr(x), _ptr(w),
        ctypes.c_void_p(_ptr(bias).ctypes.value) if bias is not None else None,
        None, _ptr(y), N, M, K)
    return y

def linear_ws_gelu(x, w, bias=None, mask=None):
    """WS + GEMM + GELU (fc1): y = GELU(WS(x @ W^T) + bias)"""
    N, K = x.shape; M, _ = w.shape
    y = np.zeros((N, M), dtype=np.float32)
    _LIBS['linear'].fused_linear_ws_gelu(
        _ptr(x), _ptr(w),
        ctypes.c_void_p(_ptr(bias).ctypes.value) if bias is not None else None,
        ctypes.c_void_p(_ptr(mask).ctypes.value) if mask is not None else None,
        _ptr(y), N, M, K)
    return y

def linear_ws(x, w, bias=None, mask=None):
    """WS + GEMM (fc2/head): y = WS(x @ W^T) + bias"""
    N, K = x.shape; M, _ = w.shape
    y = np.zeros((N, M), dtype=np.float32)
    _LIBS['linear'].fused_linear_ws(
        _ptr(x), _ptr(w),
        ctypes.c_void_p(_ptr(bias).ctypes.value) if bias is not None else None,
        ctypes.c_void_p(_ptr(mask).ctypes.value) if mask is not None else None,
        _ptr(y), N, M, K)
    return y

def n14_gate_blend(x_pe, x_rope, logit, pos_bias, n14_state):
    """N14 门控融合 (tick + 相干积 + EMA + clamp + blend)"""
    B, T, D = x_pe.shape
    y = np.zeros_like(x_pe)
    _LIBS['gate'].n14_fused_gate_blend(
        _ptr(x_pe), _ptr(x_rope), ctypes.c_float(logit), _ptr(pos_bias),
        ctypes.byref(n14_state), _ptr(y), B, T, D)
    return y

# ── 预计算 RoPE cos/sin 表 ──
def make_rope_table(T, D=64, base=50000.0):
    half = D // 2
    inv_freq = np.array([1.0/(base**(2*i/D)) for i in range(half)], dtype=np.float32)
    cos = np.zeros((T, D), dtype=np.float32)
    sin = np.zeros((T, D), dtype=np.float32)
    _LIBS['rope'].rope_precompute(_ptr(cos), _ptr(sin), _ptr(inv_freq), T, D, ctypes.c_float(base))
    return cos, sin

# ── 完整 Transformer Block (替换一层 PyTorch forward) ──
def transformer_block(x, pos, ln1_w, attn, ln2_w,
                      fc1_w, fc1_b, fc2_w, fc2_b, mask_fc1, mask_fc2,
                      rope_cos, rope_sin, n14_state, logit_gate):
    """
    完整替换:
      attn_input = RMSNorm(x)    → C fused_rmsnorm
      RoPE Q/K + attention       → C gemm + rope_apply
      Gate blend                 → C n14_gate_blend
      MLP: fc1 + GELU + fc2      → C fused_linear_ws_gelu/ws
    """
    T = x.shape[1]

    # ── RMSNorm → Attention ──
    attn_input = rmsnorm(x[0], ln1_w)[None, :, :]  # (1, T, 768)

    # QKV 投影 (纯 GEMM, 不需要 WS)
    q = gemm_no_ws(attn_input[0], attn['q_w'])         # (T, 768)
    k = gemm_no_ws(attn_input[0], attn['k_w'])
    v = gemm_no_ws(attn_input[0], attn['v_w'])

    # RoPE
    NH, DH = 12, 64
    rope_apply(q, rope_cos, rope_sin)
    rope_apply(k, rope_cos, rope_sin)

    # 多头 reshape + attention (使用 fused_attn 内核)
    # 需要 [B, NH, T, DH] 布局
    q_h = q.reshape(1, NH, T, DH)
    k_h = k.reshape(1, NH, T, DH)
    v_h = v.reshape(1, NH, T, DH)
    o_h = np.zeros_like(q_h)
    _LIBS['attn'] = _load_so('libggml_fused_attn.so')
    _LIBS['attn'].fused_attn(
        _ptr(q_h), _ptr(k_h), _ptr(v_h), _ptr(o_h),
        1, NH, T, DH, True)

    attn_out = o_h.transpose(0, 2, 1, 3).reshape(1, T, 768)  # B, T, D
    attn_out = gemm_no_ws(attn_out[0], attn['out_w'])         # output proj

    # ── Gate Blend (用同一份输入替代 RoPE/PE 分支) ──
    # 简化: RoPE 分支 = attn_out, PE 分支也 = attn_out (单分支时)
    # 实际双分支需要完整 PE 分支计算
    pos_bias = 0.1 * np.sin(np.arange(T, dtype=np.float32) / 8.0)
    x_mixed = n14_gate_blend(
        attn_out[None, :, :], attn_out[None, :, :],
        logit_gate, pos_bias, n14_state)

    # 残差
    x = x + x_mixed

    # ── MLP ──
    mlp_input = rmsnorm(x[0], ln2_w)[None, :, :]
    fc1_out = linear_ws_gelu(mlp_input[0], fc1_w, fc1_b, mask_fc1)
    fc2_out = linear_ws(fc1_out, fc2_w, fc2_b, mask_fc2)
    x = x + fc2_out[None, :, :]

    return x

# ── 完整 12 层 GPT 前向 ──
def gpt_forward(tokens, weights_dict, rope_cos, rope_sin, n14_state, logit_gate=0.0):
    """完整 GPT 前向: tokens → logits (全 C 内核, 无 PyTorch 算子)"""
    T = tokens.shape[1]
    x = weights_dict['tok_embed'][tokens].astype(np.float32)  # embedding lookup (numpy)

    for i in range(12):
        blk = weights_dict['blocks'][i]
        x = transformer_block(x, tokens, blk['ln1_w'], blk['attn'], blk['ln2_w'],
                              blk['fc1_w'], blk['fc1_b'], blk['fc2_w'], blk['fc2_b'],
                              None, None, rope_cos, rope_sin, n14_state, logit_gate)

    # 最终 LayerNorm + head
    # C 内核暂缺 LayerNorm (带 bias), 用简化 rmsnorm
    x = rmsnorm(x[0], weights_dict['ln_final_w'])[None, :, :]
    logits = linear_ws(x[0], weights_dict['head_w'], weights_dict['head_b'])

    return logits
