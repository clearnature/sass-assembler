//===-- vavx3_blas.h - VAVX3 BLAS Float/Double Primitives ---*- C -*-===//
//
// HunTian Virtual AVX3 BLAS Primitives for OpenBLAS
// 512-bit virtual vector operations for float/double
// Runtime: Broadwell (AVX2) — each 512-bit op = 2× 256-bit AVX2
//
//===----------------------------------------------------------------------===//

#ifndef VAVX3_BLAS_H
#define VAVX3_BLAS_H

#include <immintrin.h>

#define RESIDENT_CORE __attribute__((always_inline, hot, aligned(64)))

// ==================== [ Float 512-bit (16× float) ] ====================

typedef struct { __m256 v0; __m256 v1; } vavx3_512f;

static inline vavx3_512f RESIDENT_CORE vavx3_load_512f(const float* p) {
    vavx3_512f r;
    r.v0 = _mm256_loadu_ps(p);
    r.v1 = _mm256_loadu_ps(p + 8);
    return r;
}

static inline void RESIDENT_CORE vavx3_store_512f(float* p, vavx3_512f v) {
    _mm256_storeu_ps(p, v.v0);
    _mm256_storeu_ps(p + 8, v.v1);
}

static inline vavx3_512f RESIDENT_CORE vavx3_set1_512f(float x) {
    vavx3_512f r;
    r.v0 = _mm256_set1_ps(x);
    r.v1 = _mm256_set1_ps(x);
    return r;
}

static inline vavx3_512f RESIDENT_CORE vavx3_add_512f(vavx3_512f a, vavx3_512f b) {
    vavx3_512f r;
    r.v0 = _mm256_add_ps(a.v0, b.v0);
    r.v1 = _mm256_add_ps(a.v1, b.v1);
    return r;
}

static inline vavx3_512f RESIDENT_CORE vavx3_sub_512f(vavx3_512f a, vavx3_512f b) {
    vavx3_512f r;
    r.v0 = _mm256_sub_ps(a.v0, b.v0);
    r.v1 = _mm256_sub_ps(a.v1, b.v1);
    return r;
}

static inline vavx3_512f RESIDENT_CORE vavx3_mul_512f(vavx3_512f a, vavx3_512f b) {
    vavx3_512f r;
    r.v0 = _mm256_mul_ps(a.v0, b.v0);
    r.v1 = _mm256_mul_ps(a.v1, b.v1);
    return r;
}

static inline vavx3_512f RESIDENT_CORE vavx3_fmadd_512f(vavx3_512f a, vavx3_512f b, vavx3_512f c) {
    vavx3_512f r;
    r.v0 = _mm256_fmadd_ps(a.v0, b.v0, c.v0);
    r.v1 = _mm256_fmadd_ps(a.v1, b.v1, c.v1);
    return r;
}

static inline vavx3_512f RESIDENT_CORE vavx3_fnmadd_512f(vavx3_512f a, vavx3_512f b, vavx3_512f c) {
    vavx3_512f r;
    r.v0 = _mm256_fnmadd_ps(a.v0, b.v0, c.v0);
    r.v1 = _mm256_fnmadd_ps(a.v1, b.v1, c.v1);
    return r;
}

static inline vavx3_512f RESIDENT_CORE vavx3_abs_512f(vavx3_512f a) {
    vavx3_512f r;
    __m256 sign = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
    r.v0 = _mm256_and_ps(a.v0, sign);
    r.v1 = _mm256_and_ps(a.v1, sign);
    return r;
}

static inline float RESIDENT_CORE vavx3_hsum_512f(vavx3_512f v) {
    __m256 s0 = _mm256_hadd_ps(v.v0, v.v1);
    __m256 s1 = _mm256_hadd_ps(s0, s0);
    __m128 lo = _mm256_castps256_ps128(s1);
    __m128 hi = _mm256_extractf128_ps(s1, 1);
    __m128 sum = _mm_add_ps(lo, hi);
    return _mm_cvtss_f32(sum);
}

// ==================== [ Double 512-bit (8× double) ] ====================

typedef struct { __m256d v0; __m256d v1; } vavx3_512d;

static inline vavx3_512d RESIDENT_CORE vavx3_load_512d(const double* p) {
    vavx3_512d r;
    r.v0 = _mm256_loadu_pd(p);
    r.v1 = _mm256_loadu_pd(p + 4);
    return r;
}

static inline void RESIDENT_CORE vavx3_store_512d(double* p, vavx3_512d v) {
    _mm256_storeu_pd(p, v.v0);
    _mm256_storeu_pd(p + 4, v.v1);
}

static inline vavx3_512d RESIDENT_CORE vavx3_set1_512d(double x) {
    vavx3_512d r;
    r.v0 = _mm256_set1_pd(x);
    r.v1 = _mm256_set1_pd(x);
    return r;
}

static inline vavx3_512d RESIDENT_CORE vavx3_add_512d(vavx3_512d a, vavx3_512d b) {
    vavx3_512d r;
    r.v0 = _mm256_add_pd(a.v0, b.v0);
    r.v1 = _mm256_add_pd(a.v1, b.v1);
    return r;
}

static inline vavx3_512d RESIDENT_CORE vavx3_sub_512d(vavx3_512d a, vavx3_512d b) {
    vavx3_512d r;
    r.v0 = _mm256_sub_pd(a.v0, b.v0);
    r.v1 = _mm256_sub_pd(a.v1, b.v1);
    return r;
}

static inline vavx3_512d RESIDENT_CORE vavx3_mul_512d(vavx3_512d a, vavx3_512d b) {
    vavx3_512d r;
    r.v0 = _mm256_mul_pd(a.v0, b.v0);
    r.v1 = _mm256_mul_pd(a.v1, b.v1);
    return r;
}

static inline vavx3_512d RESIDENT_CORE vavx3_fmadd_512d(vavx3_512d a, vavx3_512d b, vavx3_512d c) {
    vavx3_512d r;
    r.v0 = _mm256_fmadd_pd(a.v0, b.v0, c.v0);
    r.v1 = _mm256_fmadd_pd(a.v1, b.v1, c.v1);
    return r;
}

static inline vavx3_512d RESIDENT_CORE vavx3_fnmadd_512d(vavx3_512d a, vavx3_512d b, vavx3_512d c) {
    vavx3_512d r;
    r.v0 = _mm256_fnmadd_pd(a.v0, b.v0, c.v0);
    r.v1 = _mm256_fnmadd_pd(a.v1, b.v1, c.v1);
    return r;
}

static inline vavx3_512d RESIDENT_CORE vavx3_abs_512d(vavx3_512d a) {
    vavx3_512d r;
    __m256d sign = _mm256_castsi256_pd(_mm256_set1_epi64x(0x7FFFFFFFFFFFFFFF));
    r.v0 = _mm256_and_pd(a.v0, sign);
    r.v1 = _mm256_and_pd(a.v1, sign);
    return r;
}

static inline double RESIDENT_CORE vavx3_hsum_512d(vavx3_512d v) {
    __m256d s0 = _mm256_hadd_pd(v.v0, v.v1);
    __m256d s1 = _mm256_hadd_pd(s0, s0);
    __m128d lo = _mm256_castpd256_pd128(s1);
    __m128d hi = _mm256_extractf128_pd(s1, 1);
    __m128d sum = _mm_add_pd(lo, hi);
    return _mm_cvtsd_f64(sum);
}

// ==================== [ Block GEMM helpers ] ====================

// 4×4 float block FMA (for small matrix GEMM)
static inline void RESIDENT_CORE vavx3_gemm_4x4f(const float* A, const float* B, float* C, int k, float alpha) {
    vavx3_512f c0 = vavx3_set1_512f(0), c1 = vavx3_set1_512f(0);
    for (int i = 0; i < k; i++) {
        vavx3_512f a = vavx3_load_512f(A + i * 4);
        vavx3_512f b0 = vavx3_set1_512f(B[i]);
        vavx3_512f b1 = vavx3_set1_512f(B[i + 4]);
        vavx3_512f b2 = vavx3_set1_512f(B[i + 8]);
        vavx3_512f b3 = vavx3_set1_512f(B[i + 12]);
        c0 = vavx3_fmadd_512f(a, b0, c0);
        c1 = vavx3_fmadd_512f(a, b1, c1);
        c0 = vavx3_fmadd_512f(a, b2, c0);
        c1 = vavx3_fmadd_512f(a, b3, c1);
    }
    vavx3_512f alpha_v = vavx3_set1_512f(alpha);
    vavx3_store_512f(C, vavx3_mul_512f(c0, alpha_v));
    vavx3_store_512f(C + 8, vavx3_mul_512f(c1, alpha_v));
}

// 4×4 double block FMA
static inline void RESIDENT_CORE vavx3_gemm_4x4d(const double* A, const double* B, double* C, int k, double alpha) {
    vavx3_512d c0 = vavx3_set1_512d(0), c1 = vavx3_set1_512d(0);
    for (int i = 0; i < k; i++) {
        vavx3_512d a = vavx3_load_512d(A + i * 4);
        vavx3_512d b0 = vavx3_set1_512d(B[i]);
        vavx3_512d b1 = vavx3_set1_512d(B[i + 4]);
        vavx3_512d b2 = vavx3_set1_512d(B[i + 8]);
        vavx3_512d b3 = vavx3_set1_512d(B[i + 12]);
        c0 = vavx3_fmadd_512d(a, b0, c0);
        c1 = vavx3_fmadd_512d(a, b1, c1);
        c0 = vavx3_fmadd_512d(a, b2, c0);
        c1 = vavx3_fmadd_512d(a, b3, c1);
    }
    vavx3_512d alpha_v = vavx3_set1_512d(alpha);
    vavx3_store_512d(C, vavx3_mul_512d(c0, alpha_v));
    vavx3_store_512d(C + 4, vavx3_mul_512d(c1, alpha_v));
}

#endif // VAVX3_BLAS_H
