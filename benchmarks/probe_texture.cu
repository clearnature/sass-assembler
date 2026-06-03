/* Pascal GP106 — 纹理指令探针 (legacy texture API) */
texture<float, 1, cudaReadModeElementType> tex1d;
texture<float, 2, cudaReadModeElementType> tex2d;

extern "C" {

__global__ void probe_tex_1d(float* out, float coord) {
    out[0] = tex1D(tex1d, coord);            // TEX
    out[1] = tex1Dfetch(tex1d, (int)coord);  // TLD
}

__global__ void probe_tex_2d(float* out, float u, float v) {
    out[0] = tex2D(tex2d, u, v);             // TEX.2D
}

__global__ void probe_surface_write(int* surf, int val, int idx) {
    surf1Dwrite(surf, idx, val, cudaBoundaryModeTrap);  // SUST
}

__global__ void probe_surface_read(int* out, const int* surf, int idx) {
    out[0] = surf1Dread(surf, idx, cudaBoundaryModeTrap); // SULD
}

} // extern "C"
