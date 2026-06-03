// Sovereign CUDA Extension — ATen bindings, AMP-aware
#include <torch/extension.h>
#include <c10/cuda/CUDAStream.h>
#include <cuda_runtime.h>
#include <vector>

extern "C" {
    void rmsnorm_fwd(const float* x, const float* w, float* y, float* rstd, float* x_norm, int N, int D, float eps, cudaStream_t s);
    void rmsnorm_bwd(const float* dy, const float* x_norm, const float* rstd, const float* w, float* dx, int N, int D, cudaStream_t s);
    void rmsnorm_dw(const float* dy, const float* x_norm, float* dw, int N, int D, cudaStream_t s);
    void cuda_rope_fwd(const float* x, float* y, const float* cos, const float* sin, int n, int T, int D, cudaStream_t s);
    void cuda_layernorm(const float* x, const float* w, const float* b, float* y, int N, int D, float eps, cudaStream_t s);
    void cuda_gate_blend_gate(const float* x_pe, const float* x_rope, const float* gate, float* y, int N, int D, cudaStream_t s);
    void cuda_add(const float* a, const float* b, float* y, int n, cudaStream_t s);
    void cuda_embedding(const int* tokens, const float* table, float* y, int B, int T, int V, int D, cudaStream_t s);
    void cuda_sparse_4k(const float* x, const float* w, const uint8_t* mk, float* y, int N, int M, int K, cudaStream_t s);
    void cuda_sparse_dw_4k(const float* x, const float* dy, const uint8_t* mk, float* dw, int N, int M, int K, cudaStream_t s);
    void cuda_sparse_dx_4k(const float* dy, const float* w, const uint8_t* mk, float* dx, int N, int M, int K, cudaStream_t s);
    void cuda_cross_entropy(const float* logits, const int* targets, float* loss, int N, int C, cudaStream_t s);
    void cuda_ws_gemm_gelu(const float* x, const float* w, const float* bias, const float* mask, float* y, int N, int M, int K, cudaStream_t s);
    void cuda_attn2(const float* q, const float* k, const float* v, float* o, int B, int NH, int T, int D, bool causal, cudaStream_t s);
}
#define CHECK_CUDA(x) TORCH_CHECK(x.device().is_cuda(), #x " must be CUDA tensor")
#define CHECK_F32(x)  TORCH_CHECK(x.scalar_type()==at::kFloat||x.scalar_type()==at::kHalf, #x " must be float32 or float16")
static cudaStream_t get_stream() { return c10::cuda::getCurrentCUDAStream(c10::cuda::current_device()); }
static at::Tensor to_f32(const at::Tensor& t) { return t.scalar_type()==at::kFloat ? t : t.to(at::kFloat); }

// 1. RMSNorm ──────────────────────────────────────────────
class RMSNormFunc : public torch::autograd::Function<RMSNormFunc> {
public:
    static torch::Tensor forward(torch::autograd::AutogradContext* ctx, const torch::Tensor& x, const torch::Tensor& weight, double eps) {
        CHECK_CUDA(x); CHECK_F32(x); auto _x=to_f32(x), _w=to_f32(weight); auto N=_x.size(0),D=_x.size(1);
        auto y=torch::empty_like(_x), xn=torch::empty_like(_x), rs=torch::empty({N,D},_x.options());
        (void)0; // placeholder
        rmsnorm_fwd(_x.data_ptr<float>(),_w.data_ptr<float>(),y.data_ptr<float>(),rs.data_ptr<float>(),xn.data_ptr<float>(),N,D,(float)eps,get_stream());
        ctx->save_for_backward({xn,rs,_w}); return y;
    }
    static torch::autograd::tensor_list backward(torch::autograd::AutogradContext* ctx, torch::autograd::tensor_list grad_outputs) {
        auto s=ctx->get_saved_variables(); auto xn=s[0],rs=s[1],_w=s[2],gy=grad_outputs[0]; auto N=xn.size(0),D=xn.size(1);
        auto dx=torch::empty_like(gy),dw=torch::zeros_like(_w);
        rmsnorm_bwd(gy.data_ptr<float>(),xn.data_ptr<float>(),rs.data_ptr<float>(),_w.data_ptr<float>(),dx.data_ptr<float>(),N,D,get_stream());
        rmsnorm_dw(gy.data_ptr<float>(),xn.data_ptr<float>(),dw.data_ptr<float>(),N,D,get_stream());
        return {dx,dw,torch::Tensor()};
    }
};
torch::Tensor rmsnorm(const torch::Tensor& x, const torch::Tensor& w, double eps) { return RMSNormFunc::apply(x,w,eps); }

// 2. LayerNorm ────────────────────────────────────────────
class LayerNormFunc : public torch::autograd::Function<LayerNormFunc> {
public:
    static torch::Tensor forward(torch::autograd::AutogradContext* ctx, const torch::Tensor& x, const torch::Tensor& weight, const torch::Tensor& bias, double eps) {
        CHECK_CUDA(x); CHECK_F32(x); auto _x=to_f32(x); int64_t N=_x.size(0),D=_x.size(1); auto y=torch::empty_like(_x);
        (void)0; // placeholder
        cuda_layernorm(_x.data_ptr<float>(),weight.data_ptr<float>(),bias.data_ptr<float>(),y.data_ptr<float>(),N,D,(float)eps,get_stream());
        ctx->save_for_backward({_x,weight,bias}); ctx->saved_data["eps"]=eps; return y;
    }
    static torch::autograd::tensor_list backward(torch::autograd::AutogradContext* ctx, torch::autograd::tensor_list grad_outputs) {
        auto s=ctx->get_saved_variables(); auto _x=s[0],_w=s[1],_b=s[2],gy=grad_outputs[0]; double eps=ctx->saved_data["eps"].toDouble();
        auto x2=_x.detach().requires_grad_(true),w2=_w.detach().requires_grad_(true),b2=_b.detach().requires_grad_(true);
        torch::layer_norm(x2,{x2.size(-1)},w2,b2,eps).backward(gy); return {x2.grad(),w2.grad(),b2.grad(),torch::Tensor()};
    }
};
torch::Tensor layernorm(const torch::Tensor& x, const torch::Tensor& w, const torch::Tensor& b, double eps) { return LayerNormFunc::apply(x,w,b,eps); }

// 3. RoPE ─────────────────────────────────────────────────
class RoPEFunc : public torch::autograd::Function<RoPEFunc> {
public:
    static torch::Tensor forward(torch::autograd::AutogradContext* ctx, const torch::Tensor& x, const torch::Tensor& cos, const torch::Tensor& sin) {
        CHECK_CUDA(x); CHECK_F32(x); auto _x=to_f32(x); int64_t D=_x.size(-1),n=_x.numel()/D,T=cos.size(0); auto y=torch::empty_like(_x);
        (void)0; // placeholder
        cuda_rope_fwd(_x.data_ptr<float>(),y.data_ptr<float>(),cos.data_ptr<float>(),sin.data_ptr<float>(),n,T,D,get_stream());
        ctx->save_for_backward({to_f32(cos),to_f32(sin)}); return y;
    }
    static torch::autograd::tensor_list backward(torch::autograd::AutogradContext* ctx, torch::autograd::tensor_list grad_outputs) {
        auto s=ctx->get_saved_variables(); auto cos=s[0],sin=s[1],gy=grad_outputs[0];
        auto D=gy.size(-1),n=gy.numel()/D,T=cos.size(0); auto gx=torch::empty_like(gy),ns=-sin;
        cuda_rope_fwd(gy.data_ptr<float>(),gx.data_ptr<float>(),cos.data_ptr<float>(),ns.data_ptr<float>(),n,T,D,get_stream());
        return {gx,torch::Tensor(),torch::Tensor()};
    }
};
torch::Tensor rope(const torch::Tensor& x, const torch::Tensor& cos, const torch::Tensor& sin) { return RoPEFunc::apply(x,cos,sin); }

// 4. Gate Blend ───────────────────────────────────────────
torch::Tensor gate_blend(const torch::Tensor& x_pe, const torch::Tensor& x_rope, const torch::Tensor& gate) {
    CHECK_CUDA(x_pe); CHECK_F32(x_pe); auto _pe=to_f32(x_pe),_ro=to_f32(x_rope); int64_t N=_pe.size(0),D=_pe.size(1); auto y=torch::empty_like(_pe);
        (void)0; // placeholder
    auto gf=gate.reshape(-1).contiguous();
    cuda_gate_blend_gate(_pe.data_ptr<float>(),_ro.data_ptr<float>(),gf.data_ptr<float>(),y.data_ptr<float>(),N,D,get_stream()); return y;
}

// 5. Add ──────────────────────────────────────────────────
class AddFunc : public torch::autograd::Function<AddFunc> {
public:
    static torch::Tensor forward(torch::autograd::AutogradContext* ctx, const torch::Tensor& a, const torch::Tensor& b) {
        CHECK_CUDA(a); CHECK_F32(a); auto _a=to_f32(a),_b=to_f32(b); auto y=torch::empty_like(_a);
        (void)0; // placeholder
        cuda_add(_a.data_ptr<float>(),_b.data_ptr<float>(),y.data_ptr<float>(),_a.numel(),get_stream()); return y;
    }
    static torch::autograd::tensor_list backward(torch::autograd::AutogradContext*, torch::autograd::tensor_list g) { return {g[0],g[0]}; }
};
torch::Tensor add_tensors(const torch::Tensor& a, const torch::Tensor& b) { return AddFunc::apply(a,b); }

// 6. Embedding ────────────────────────────────────────────
class EmbedFunc : public torch::autograd::Function<EmbedFunc> {
public:
    static torch::Tensor forward(torch::autograd::AutogradContext* ctx, const torch::Tensor& tokens, const torch::Tensor& table) {
        CHECK_CUDA(tokens); CHECK_CUDA(table); auto _t=to_f32(table); int64_t B=tokens.size(0),T=tokens.size(1),V=_t.size(0),D=_t.size(1); auto y=torch::empty({B,T,D},_t.options());
        (void)0; // placeholder
        auto tok_i32=tokens.to(torch::kInt); cuda_embedding(tok_i32.data_ptr<int>(),_t.data_ptr<float>(),y.data_ptr<float>(),B,T,V,D,get_stream());
        ctx->save_for_backward({tokens}); ctx->saved_data["V"]=(int64_t)V; ctx->saved_data["D"]=(int64_t)D; return y;
    }
    static torch::autograd::tensor_list backward(torch::autograd::AutogradContext* ctx, torch::autograd::tensor_list grad_outputs) {
        auto tokens=ctx->get_saved_variables()[0]; int64_t V=ctx->saved_data["V"].toInt(),D=ctx->saved_data["D"].toInt(); auto gy=grad_outputs[0];
        auto gt=torch::zeros({V,D},gy.options()); gt.index_add_(0,tokens.reshape(-1),gy.reshape({-1,D})); return {torch::Tensor(),gt};
    }
};
torch::Tensor embedding(const torch::Tensor& t, const torch::Tensor& tbl) { return EmbedFunc::apply(t,tbl); }

// 7. Sparse GEMM ──────────────────────────────────────────
class SparseGEMMFunc : public torch::autograd::Function<SparseGEMMFunc> {
public:
    static torch::Tensor forward(torch::autograd::AutogradContext* ctx, const torch::Tensor& x, const torch::Tensor& w, const torch::Tensor& bm) {
        CHECK_CUDA(x); CHECK_F32(x); auto _x=to_f32(x),_w=to_f32(w); int64_t N=_x.size(0),M=_w.size(0),K=_x.size(1); auto y=torch::zeros({N,M},_x.options());
        (void)0; // placeholder
        cuda_sparse_4k(_x.data_ptr<float>(),_w.data_ptr<float>(),bm.data_ptr<uint8_t>(),y.data_ptr<float>(),N,M,K,get_stream());
        ctx->save_for_backward({_x,_w,bm}); return y;
    }
    static torch::autograd::tensor_list backward(torch::autograd::AutogradContext* ctx, torch::autograd::tensor_list grad_outputs) {
        auto s=ctx->get_saved_variables(); auto _x=s[0],_w=s[1],bm=s[2],gy=grad_outputs[0]; auto N=_x.size(0),M=_w.size(0),K=_x.size(1);
        auto dw=torch::zeros({M,K},_x.options()),dx=torch::zeros({N,K},_x.options());
        cuda_sparse_dw_4k(_x.data_ptr<float>(),gy.data_ptr<float>(),bm.data_ptr<uint8_t>(),dw.data_ptr<float>(),N,M,K,get_stream());
        cuda_sparse_dx_4k(gy.data_ptr<float>(),_w.data_ptr<float>(),bm.data_ptr<uint8_t>(),dx.data_ptr<float>(),N,M,K,get_stream());
        return {dx,dw,torch::Tensor()};
    }
};
torch::Tensor sparse_gemm(const torch::Tensor& x, const torch::Tensor& w, const torch::Tensor& bm) { return SparseGEMMFunc::apply(x,w,bm); }

// 8. WS+GEMM+GELU ────────────────────────────────────────
class WSGELUFunc : public torch::autograd::Function<WSGELUFunc> {
public:
    static torch::Tensor forward(torch::autograd::AutogradContext* ctx, const torch::Tensor& x, const torch::Tensor& w, const std::optional<torch::Tensor>& bias, const std::optional<torch::Tensor>& mask) {
        CHECK_CUDA(x); CHECK_F32(x); auto _x=to_f32(x); int64_t N=_x.size(0),M=w.size(0),K=_x.size(1); auto y=torch::empty({N,M},_x.options());
        (void)0; // placeholder
        const float* bp=bias.has_value()?bias->data_ptr<float>():nullptr; const float* mp=mask.has_value()?mask->data_ptr<float>():nullptr;
        cuda_ws_gemm_gelu(_x.data_ptr<float>(),w.data_ptr<float>(),bp,mp,y.data_ptr<float>(),N,M,K,get_stream());
        if(bias.has_value()) ctx->save_for_backward({_x,w,*bias}); else ctx->save_for_backward({_x,w}); return y;
    }
    static torch::autograd::tensor_list backward(torch::autograd::AutogradContext*,torch::autograd::tensor_list) { return {torch::Tensor(),torch::Tensor(),torch::Tensor(),torch::Tensor()}; }
};
torch::Tensor ws_gemm_gelu(const torch::Tensor& x, const torch::Tensor& w, std::optional<torch::Tensor> b, std::optional<torch::Tensor> m) { return WSGELUFunc::apply(x,w,b,m); }
torch::Tensor ws_gemm_gelu_raw(const torch::Tensor& x, const torch::Tensor& w, std::optional<torch::Tensor> b, std::optional<torch::Tensor> m) {
    CHECK_CUDA(x); CHECK_F32(x); auto _x=to_f32(x); int64_t N=_x.size(0),M=w.size(0),K=_x.size(1); auto y=torch::empty({N,M},_x.options());
        (void)0; // placeholder
    const float* bp=b.has_value()?b->data_ptr<float>():nullptr; const float* mp=m.has_value()?m->data_ptr<float>():nullptr;
    cuda_ws_gemm_gelu(_x.data_ptr<float>(),w.data_ptr<float>(),bp,mp,y.data_ptr<float>(),N,M,K,get_stream()); return y;
}

// 9. Attention 4K ─────────────────────────────────────────
class Attn2Func : public torch::autograd::Function<Attn2Func> {
public:
    static torch::Tensor forward(torch::autograd::AutogradContext* ctx, const torch::Tensor& q, const torch::Tensor& k, const torch::Tensor& v, bool causal) {
        CHECK_CUDA(q); CHECK_F32(q);
        auto _q=to_f32(q),_k=to_f32(k),_v=to_f32(v);
        int64_t B=_q.size(0),NH=_q.size(1),T=_q.size(2),D=_q.size(3);
        auto q2=_q.reshape({B*NH,T,D}).contiguous(),k2=_k.reshape({B*NH,T,D}).contiguous(),v2=_v.reshape({B*NH,T,D}).contiguous();
        auto o=torch::empty({B*NH,T,D},_q.options());
        cuda_attn2(q2.data_ptr<float>(),k2.data_ptr<float>(),v2.data_ptr<float>(),o.data_ptr<float>(),B,NH,T,D,causal,get_stream());
        ctx->save_for_backward({_q,_k,_v,o});
        ctx->saved_data["causal"]=causal; ctx->saved_data["T"]=T; ctx->saved_data["D"]=D;
        return o.reshape({B,NH,T,D});
    }
    static torch::autograd::tensor_list backward(torch::autograd::AutogradContext* ctx, torch::autograd::tensor_list grad_outputs) {
        auto s=ctx->get_saved_variables(); auto q=s[0],k=s[1],v=s[2],o_fwd=s[3];
        bool causal=ctx->saved_data["causal"].toBool();
        int64_t T=ctx->saved_data["T"].toInt(),D=ctx->saved_data["D"].toInt();
        auto go=grad_outputs[0];
        auto _go=to_f32(go);
        auto B=q.size(0),NH=q.size(1);
        // Use PyTorch's efficient SDPA for backward (avoids materializing full QK^T matrix)
        at::AutoGradMode gm(true);
        auto qr=q.detach().requires_grad_(true);
        auto kr=k.detach().requires_grad_(true);
        auto vr=v.detach().requires_grad_(true);
        auto o2=torch::scaled_dot_product_attention(qr,kr,vr,{},0.0,causal);
        o2.backward(_go);
        return {qr.grad(),kr.grad(),vr.grad(),torch::Tensor()};
    }
};
torch::Tensor attn2(const torch::Tensor& q, const torch::Tensor& k, const torch::Tensor& v, bool causal) { return Attn2Func::apply(q,k,v,causal); }

// 10. CrossEntropy ─────────────────────────────────────────
class CEFunc : public torch::autograd::Function<CEFunc> {
public:
    static torch::Tensor forward(torch::autograd::AutogradContext* ctx, const torch::Tensor& logits, const torch::Tensor& targets) {
        CHECK_CUDA(logits); CHECK_F32(logits); auto _l=to_f32(logits); int64_t N=_l.size(0),C=_l.size(1); auto loss=torch::empty({N},_l.options());
        (void)0; // placeholder
        auto tgt_i32=targets.to(torch::kInt); cuda_cross_entropy(_l.data_ptr<float>(),tgt_i32.data_ptr<int>(),loss.data_ptr<float>(),N,C,get_stream());
        ctx->save_for_backward({_l,targets}); return loss.mean();
    }
    static torch::autograd::tensor_list backward(torch::autograd::AutogradContext* ctx, torch::autograd::tensor_list grad_outputs) {
        auto s=ctx->get_saved_variables(); auto _l=s[0],tgt=s[1],go=grad_outputs[0]; at::AutoGradMode gm(true);
        auto l2=_l.detach().requires_grad_(true); auto lv=torch::cross_entropy_loss(l2,tgt,{},torch::Reduction::None); lv.backward(torch::ones_like(lv)); return {l2.grad()*go,torch::Tensor()};
    }
};
torch::Tensor cross_entropy_loss(const torch::Tensor& l, const torch::Tensor& t) { return CEFunc::apply(l,t); }

// ── Python module ────────────────────────────────────────
PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
    m.def("rmsnorm",&rmsnorm); m.def("layernorm",&layernorm); m.def("rope",&rope);
    m.def("gate_blend",&gate_blend); m.def("add_tensors",&add_tensors); m.def("embedding",&embedding);
    m.def("sparse_gemm",&sparse_gemm); m.def("ws_gemm_gelu",&ws_gemm_gelu); m.def("ws_gemm_gelu_raw",&ws_gemm_gelu_raw);
    m.def("cross_entropy",&cross_entropy_loss);
    m.def("attn2",&attn2);
}
TORCH_LIBRARY(sovereign, m) {
    m.def("rmsnorm(Tensor x, Tensor weight, float eps) -> Tensor",&rmsnorm);
    m.def("layernorm(Tensor x, Tensor weight, Tensor bias, float eps) -> Tensor",&layernorm);
    m.def("rope(Tensor x, Tensor cos, Tensor sin) -> Tensor",&rope);
    m.def("gate_blend(Tensor x_pe, Tensor x_rope, Tensor gate) -> Tensor",&gate_blend);
    m.def("add_tensors(Tensor a, Tensor b) -> Tensor",&add_tensors);
    m.def("embedding(Tensor tokens, Tensor table) -> Tensor",&embedding);
    m.def("sparse_gemm(Tensor x, Tensor w, Tensor block_mask) -> Tensor",&sparse_gemm);
    m.def("ws_gemm_gelu(Tensor x, Tensor w, Tensor? bias=None, Tensor? mask=None) -> Tensor",&ws_gemm_gelu);
    m.def("cross_entropy(Tensor logits, Tensor targets) -> Tensor",&cross_entropy_loss);
    m.def("attn2(Tensor q, Tensor k, Tensor v, bool causal) -> Tensor",&attn2);
}
