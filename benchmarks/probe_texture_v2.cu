/* Pascal GP106 — 纹理指令探针 v2 (CUDA 12 texture object API) */
#include <cuda_runtime.h>

extern "C" {

__global__ void probe_tex(float* out, cudaTextureObject_t tex, float coord) {
    out[0] = tex1Dfetch<float>(tex, (int)coord);    // TLD
    out[1] = tex1D<float>(tex, coord);               // TEX
}

__global__ void probe_surface(int* out, cudaSurfaceObject_t surf, int idx) {
    surf1Dread<int>(out, surf, idx * sizeof(int));   // SULD
    surf1Dwrite(surf, idx, idx * sizeof(int));        // SUST
}

} // extern "C"
