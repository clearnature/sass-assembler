// Attention CPU — OpenBLAS 744 GFLOPS + AVX2 vectorized exp (8-wide)
#include "ggml_fused_attn.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <cblas.h>
#if defined(__AVX2__)
#include <immintrin.h>
#endif

#define Q_BLOCK 256

#if defined(__AVX2__)
static inline __m256 vexp(__m256 x){
    const __m256 log2e=_mm256_set1_ps(1.44269504f),ln2=_mm256_set1_ps(0.69314718f);
    const __m256 one=_mm256_set1_ps(1.0f),upper=_mm256_set1_ps(88.0f),lower=_mm256_set1_ps(-87.0f);
    x=_mm256_max_ps(lower,_mm256_min_ps(upper,x));
    __m256 kf=_mm256_mul_ps(x,log2e);__m256i ki=_mm256_cvtps_epi32(kf);kf=_mm256_cvtepi32_ps(ki);
    __m256 r=_mm256_sub_ps(x,_mm256_mul_ps(kf,ln2));
    const __m256 c4=_mm256_set1_ps(0.00833333f),c3=_mm256_set1_ps(0.04166667f),c2=_mm256_set1_ps(0.16666667f),c1=_mm256_set1_ps(0.5f);
    __m256 p=_mm256_fmadd_ps(c4,r,c3);p=_mm256_fmadd_ps(p,r,c2);p=_mm256_fmadd_ps(p,r,c1);p=_mm256_fmadd_ps(p,r,one);p=_mm256_fmadd_ps(p,r,one);
    __m256i kib=_mm256_add_epi32(ki,_mm256_set1_epi32(127));kib=_mm256_slli_epi32(kib,23);
    return _mm256_mul_ps(_mm256_castsi256_ps(kib),p);
}
#endif

static void attn_tiling(const float* Q,const float* K,const float* V,float* O,int T,int D,bool causal){
    const float scale=1.0f/sqrtf((float)D);
    float*S=aligned_alloc(64,(size_t)Q_BLOCK*T*sizeof(float));
    float*P=aligned_alloc(64,(size_t)Q_BLOCK*T*sizeof(float));
    float*m=aligned_alloc(64,Q_BLOCK*sizeof(float));
    float*l=aligned_alloc(64,Q_BLOCK*sizeof(float));
    if(!S||!P||!m||!l)goto cleanup;

    for(int qs=0;qs<T;qs+=Q_BLOCK){
        int Br=(qs+Q_BLOCK<T)?Q_BLOCK:T-qs;
        for(int i=0;i<Br;i++){m[i]=-FLT_MAX;l[i]=0.0f;}
        memset(O+qs*D,0,Br*D*sizeof(float));

        // SGEMM: QK^T (OpenBLAS 744 GFLOPS)
        cblas_sgemm(CblasRowMajor,CblasNoTrans,CblasTrans,Br,T,D,scale,Q+qs*D,D,K,D,0.0f,S,T);

        for(int i=0;i<Br;i++){
            if(causal)for(int j=0;j<T;j++)if(qs+i<j)S[i*T+j]=-FLT_MAX;
            float lm=S[i*T+0];for(int j=1;j<T;j++)if(S[i*T+j]>lm)lm=S[i*T+j];
            if(lm<=-FLT_MAX/2.0f)continue;
            float mn=(m[i]>lm)?m[i]:lm,alpha=expf(m[i]-mn);m[i]=mn;l[i]*=alpha;
            cblas_sscal(D,alpha,O+qs*D+i*D,1);

            float lp=0;
            __m256 vm=_mm256_set1_ps(mn);
#if defined(__AVX2__)
            int j;
            for(j=0;j<=T-8;j+=8){
                __m256 sv=_mm256_sub_ps(_mm256_loadu_ps(S+i*T+j),vm);
                __m256 pv=vexp(sv);_mm256_storeu_ps(P+i*T+j,pv);
                __m128 lo=_mm256_castps256_ps128(pv),hi=_mm256_extractf128_ps(pv,1);
                __m128 s128=_mm_add_ps(lo,hi);s128=_mm_hadd_ps(s128,s128);s128=_mm_hadd_ps(s128,s128);
                lp+=_mm_cvtss_f32(s128);
            }
            for(;j<T;j++){float p=expf(S[i*T+j]-mn);P[i*T+j]=p;lp+=p;}
#else
            for(int j=0;j<T;j++){float p=expf(S[i*T+j]-mn);P[i*T+j]=p;lp+=p;}
#endif
            l[i]+=lp;
        }

        // SGEMM: P@V (add to O)
        cblas_sgemm(CblasRowMajor,CblasNoTrans,CblasNoTrans,Br,D,T,1.0f,P,T,V,D,1.0f,O+qs*D,D);
        for(int i=0;i<Br;i++)cblas_sscal(D,1.0f/l[i],O+qs*D+i*D,1);
    }
cleanup:free(S);free(P);free(m);free(l);
}

void fused_attn(const float*q,const float*k,const float*v,float*o,int B,int NH,int T,int D,bool causal){
    #pragma omp parallel for collapse(2)
    for(int b=0;b<B;b++)for(int h=0;h<NH;h++){
        int64_t off=(int64_t)(b*NH+h)*T*D;
        attn_tiling(q+off,k+off,v+off,o+off,T,D,causal);
    }
}

void fused_gated_attn(const float*qr,const float*kr,const float*vr,const float*qp,const float*kp,const float*vp,const float*gb,float*o,int B,int NH,int T,int D,bool causal){
    int64_t hs=(int64_t)T*D,total=(int64_t)B*NH*hs;
    float*Or=calloc(total,sizeof(float)),*Op=calloc(total,sizeof(float));
    if(!Or||!Op){free(Or);free(Op);return;}
    fused_attn(qr,kr,vr,Or,B,NH,T,D,causal);fused_attn(qp,kp,vp,Op,B,NH,T,D,causal);
    #pragma omp parallel for collapse(2)
    for(int b=0;b<B;b++)for(int h=0;h<NH;h++)for(int t=0;t<T;t++){
        float g=1.0f/(1.0f+expf(-gb[t]));
        const float*rp=Or+(b*NH+h)*hs+t*D,*pp=Op+(b*NH+h)*hs+t*D;
        float*op=o+(b*NH+h)*hs+t*D;
        for(int d=0;d<D;d++)op[d]=g*pp[d]+(1.0f-g)*rp[d];
    }
    free(Or);free(Op);
}
