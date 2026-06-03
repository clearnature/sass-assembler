// RMSNorm Backward CUDA kernel
// 前向: y = x * rsqrt(mean(x^2) + eps) * weight
// 反向: dx = r * (w*dy - r * x̂ * mean(w*dy*x̂))
//          dw = sum_n dy_n * x̂_n
#include <hip/hip_runtime.h>
#include <math.h>

// 前向: y = x * rstd * w, 保存 rstd 和 x_norm 给 backward
__global__ void rmsnorm_fwd_k(const float* x, const float* w,
    float* y, float* rstd_out, float* x_norm_out, int D, float eps) {
    extern __shared__ float s[];
    int row = blockIdx.x, tid = threadIdx.x;
    const float* xr = x + row * D;
    float* yr = y + row * D;

    float ssq = 0;
    for (int i = tid; i < D; i += blockDim.x) {
        float v = xr[i]; ssq += v * v;
    }
    s[tid] = ssq; __syncthreads();
    for (int s2 = blockDim.x / 2; s2 > 0; s2 >>= 1) {
        if (tid < s2) s[tid] += s[tid + s2]; __syncthreads();
    }
    float rstd = rsqrtf(s[0] / (float)D + eps); __syncthreads();

    for (int i = tid; i < D; i += blockDim.x) {
        float xn = xr[i] * rstd;
        yr[i] = xn * w[i];
        if (rstd_out) rstd_out[row * D + i] = rstd;
        if (x_norm_out) x_norm_out[row * D + i] = xn;
    }
}

// 反向: dx = r * (w*dy - r * x_norm * mean(w*dy*x_norm))
__global__ void rmsnorm_bwd_k(const float* dy, const float* x_norm,
    const float* rstd, const float* w, float* dx, int D) {
    extern __shared__ float s[];
    int row = blockIdx.x, tid = threadIdx.x;
    const float* dy_r = dy + row * D;
    const float* xn_r = x_norm + row * D;
    float* dx_r = dx + row * D;
    float r = rstd ? rstd[row * D] : 1.0f;
    // 如果 rstd 是标量每行, 取第一个元素
    // 实际上 rstd 在存储中是 [N, D] 每行相同, 取 row*D
    // 更高效: 传 scalar r
    // 实际上 rstd_out 在 forward 中存了 [N, D] 每行相同

    // sum_term = mean(w * dy * x_norm)
    float sum_t = 0;
    for (int i = tid; i < D; i += blockDim.x)
        sum_t += w[i] * dy_r[i] * xn_r[i];
    s[tid] = sum_t; __syncthreads();
    for (int s2 = blockDim.x / 2; s2 > 0; s2 >>= 1) {
        if (tid < s2) s[tid] += s[tid + s2]; __syncthreads();
    }
    float mean_t = s[0] / (float)D; __syncthreads();

    for (int i = tid; i < D; i += blockDim.x)
        dx_r[i] = r * (w[i] * dy_r[i] - r * xn_r[i] * mean_t);
}

// dw = sum_n dy[n][d] * x_norm[n][d] (over N rows)
__global__ void rmsnorm_dw_k(const float* dy, const float* x_norm,
    float* dw, int N, int D) {
    extern __shared__ float s[];
    int d = blockIdx.x, tid = threadIdx.x;
    float sum_t = 0;
    for (int n = tid; n < N; n += blockDim.x)
        sum_t += dy[n * D + d] * x_norm[n * D + d];
    s[tid] = sum_t; __syncthreads();
    for (int s2 = blockDim.x / 2; s2 > 0; s2 >>= 1) {
        if (tid < s2) s[tid] += s[tid + s2]; __syncthreads();
    }
    if (tid == 0) dw[d] += s[0];
}

extern "C" void rmsnorm_fwd(const float* x, const float* w,
    float* y, float* rstd_out, float* x_norm_out, int N, int D, float eps, hipStream_t s) {
    rmsnorm_fwd_k<<<N, min(D, 256), min(D, 256) * sizeof(float), s>>>(x, w, y, rstd_out, x_norm_out, D, eps);
}

extern "C" void rmsnorm_bwd(const float* dy, const float* x_norm,
    const float* rstd, const float* w, float* dx, int N, int D, hipStream_t s) {
    rmsnorm_bwd_k<<<N, min(D, 256), min(D, 256) * sizeof(float), s>>>(dy, x_norm, rstd, w, dx, D);
}

extern "C" void rmsnorm_dw(const float* dy, const float* x_norm,
    float* dw, int N, int D, hipStream_t s) {
    rmsnorm_dw_k<<<D, min(N, 256), min(N, 256) * sizeof(float), s>>>(dy, x_norm, dw, N, D);
}
