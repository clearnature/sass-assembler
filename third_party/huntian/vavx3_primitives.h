#ifndef VAVX3_PRIMITIVES_H
#define VAVX3_PRIMITIVES_H

// VAVX3 is only supported on x86/x64 architectures
// For non-x86 targets, provide empty stubs to avoid compilation errors

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

#include <immintrin.h>

#define RESIDENT_CORE __attribute__((always_inline, hot, aligned(64)))
#define SYSTOLIC_STAGE __attribute__((flatten))

typedef struct {
    __m256i v0; 
    __m256i v1; 
} vavx3_512i;

// --- [组 I: 基础与闭合公理] ---
static inline vavx3_512i RESIDENT_CORE vavx3_load_512(const void* p) {
    vavx3_512i r;
    r.v0 = _mm256_loadu_si256((const __m256i*)p);
    r.v1 = _mm256_loadu_si256((const __m256i*)((const char*)p + 32));
    return r;
}

static inline vavx3_512i RESIDENT_CORE vavx3_mask_addr_512(vavx3_512i addr, int mask) {
    vavx3_512i r;
    r.v0 = _mm256_and_si256(addr.v0, _mm256_set1_epi32(mask));
    r.v1 = _mm256_and_si256(addr.v1, _mm256_set1_epi32(mask));
    return r;
}

// --- [组 II: 右手螺旋与空间寻址] ---
static inline vavx3_512i RESIDENT_CORE vavx3_geo_vortex_map_512(vavx3_512i r, vavx3_512i theta) {
    vavx3_512i addr;
    addr.v0 = _mm256_add_epi32(_mm256_mullo_epi32(r.v0, _mm256_set1_epi32(36)), theta.v0);
    addr.v1 = _mm256_add_epi32(_mm256_mullo_epi32(r.v1, _mm256_set1_epi32(36)), theta.v1);
    return addr;
}

static inline void RESIDENT_CORE vavx3_geo_rotate_512(vavx3_512i* x, vavx3_512i* y, vavx3_512i angle_scaled) {
    vavx3_512i dx = { _mm256_srli_epi32(y->v0, 2), _mm256_srli_epi32(y->v1, 2) };
    vavx3_512i dy = { _mm256_srli_epi32(x->v0, 2), _mm256_srli_epi32(x->v1, 2) };
    x->v0 = _mm256_sub_epi32(x->v0, dx.v0); x->v1 = _mm256_sub_epi32(x->v1, dx.v1);
    y->v0 = _mm256_add_epi32(y->v0, dy.v0); y->v1 = _mm256_add_epi32(y->v1, dy.v1);
}

// --- [组 III: 共形反演与谱几何] ---
static inline void RESIDENT_CORE vavx3_geo_toroidal_inversion_512(vavx3_512i* x, vavx3_512i* y, int max_d_sq) {
    __m256 f_r2 = _mm256_set1_ps((float)max_d_sq);
    for(int k=0; k<2; k++) {
        __m256 f_x = _mm256_cvtepi32_ps(k==0?x->v0:x->v1);
        __m256 f_y = _mm256_cvtepi32_ps(k==0?y->v0:y->v1);
        __m256 f_d2 = _mm256_add_ps(_mm256_mul_ps(f_x, f_x), _mm256_mul_ps(f_y, f_y));
        __m256 f_fac = _mm256_div_ps(f_r2, _mm256_add_ps(f_d2, _mm256_set1_ps(1e-6f)));
        if(k==0) { x->v0 = _mm256_cvtps_epi32(_mm256_mul_ps(f_x, f_fac)); y->v0 = _mm256_cvtps_epi32(_mm256_mul_ps(f_y, f_fac)); }
        else { x->v1 = _mm256_cvtps_epi32(_mm256_mul_ps(f_x, f_fac)); y->v1 = _mm256_cvtps_epi32(_mm256_mul_ps(f_y, f_fac)); }
    }
}

static inline vavx3_512i RESIDENT_CORE vavx3_laplacian_512(vavx3_512i c, vavx3_512i l, vavx3_512i r, vavx3_512i t, vavx3_512i b) {
    vavx3_512i res;
    res.v0 = _mm256_sub_epi32(_mm256_add_epi32(_mm256_add_epi32(l.v0, r.v0), _mm256_add_epi32(t.v0, b.v0)), _mm256_slli_epi32(c.v0, 2));
    res.v1 = _mm256_sub_epi32(_mm256_add_epi32(_mm256_add_epi32(l.v1, r.v1), _mm256_add_epi32(t.v1, b.v1)), _mm256_slli_epi32(c.v1, 2));
    return res;
}

// --- [组 IV: 浑天仪、Phi^4 与拓扑编织] ---
static inline vavx3_512i RESIDENT_CORE vavx3_phi4_scale_512(vavx3_512i v) {
    vavx3_512i r;
    r.v0 = _mm256_sub_epi32(_mm256_slli_epi32(v.v0, 3), v.v0); 
    r.v1 = _mm256_sub_epi32(_mm256_slli_epi32(v.v1, 3), v.v1);
    return r;
}

static inline vavx3_512i RESIDENT_CORE vavx3_topological_braid_512(vavx3_512i a, vavx3_512i b) {
    vavx3_512i r;
    r.v0 = _mm256_alignr_epi8(a.v0, b.v0, 4);
    r.v1 = _mm256_alignr_epi8(a.v1, b.v1, 4);
    return r;
}

// --- [组 V: 自修复、非交换与纠缠] ---
static inline vavx3_512i RESIDENT_CORE vavx3_yamabe_flow_512(vavx3_512i psi, vavx3_512i laplacian) {
    vavx3_512i r;
    r.v0 = _mm256_add_epi32(psi.v0, _mm256_srai_epi32(laplacian.v0, 2));
    r.v1 = _mm256_add_epi32(psi.v1, _mm256_srai_epi32(laplacian.v1, 2));
    return r;
}

static inline vavx3_512i RESIDENT_CORE vavx3_entangle_512(vavx3_512i v) {
    vavx3_512i r;
    r.v0 = _mm256_permute2x128_si256(v.v0, v.v1, 0x20);
    r.v1 = _mm256_permute2x128_si256(v.v0, v.v1, 0x31);
    return r;
}

#else // Non-x86 architectures - provide empty stubs

// For non-x86 targets, define empty macros to avoid errors
#define RESIDENT_CORE
#define SYSTOLIC_STAGE

// Empty stub type for non-x86
typedef struct { long long _dummy[8]; } vavx3_512i;

// Empty stub functions for non-x86 targets
static inline vavx3_512i vavx3_load_512(const void* p) { vavx3_512i r = {0}; return r; }
static inline vavx3_512i vavx3_mask_addr_512(vavx3_512i addr, int mask) { vavx3_512i r = {0}; return r; }
static inline vavx3_512i vavx3_geo_vortex_map_512(vavx3_512i r, vavx3_512i theta) { vavx3_512i res = {0}; return res; }
static inline void vavx3_geo_rotate_512(vavx3_512i* x, vavx3_512i* y, vavx3_512i angle) { }
static inline void vavx3_geo_toroidal_inversion_512(vavx3_512i* x, vavx3_512i* y, int max_d_sq) { }
static inline vavx3_512i vavx3_laplacian_512(vavx3_512i c, vavx3_512i l, vavx3_512i r, vavx3_512i t, vavx3_512i b) { vavx3_512i res = {0}; return res; }
static inline vavx3_512i vavx3_phi4_scale_512(vavx3_512i v) { vavx3_512i r = {0}; return r; }
static inline vavx3_512i vavx3_topological_braid_512(vavx3_512i a, vavx3_512i b) { vavx3_512i r = {0}; return r; }
static inline vavx3_512i vavx3_yamabe_flow_512(vavx3_512i psi, vavx3_512i laplacian) { vavx3_512i r = {0}; return r; }
static inline vavx3_512i vavx3_entangle_512(vavx3_512i v) { vavx3_512i r = {0}; return r; }

#endif // __x86_64__ || __i386__

#endif
