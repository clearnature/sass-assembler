cmake_minimum_required(VERSION 3.20)
project(ggml_phase3 C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -mavx2 -mfma -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -mavx2 -mfma -fPIC")

# ─── 引用 math 库 (主权律算数学库) ───
# math 库保持独立在 third_party/math/，通过 include 路径引用
set(MATH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../math/include)

# ─── GGML 核心量化库 ───
add_library(ggml_quants SHARED ggml-quants.c)
target_include_directories(ggml_quants PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MATH_INCLUDE_DIR}
)

# ─── CPU 融合算子库 ───
add_library(ggml_fused STATIC
    cpu/ggml_fused.c
    cpu/ggml_fused_attn.c
    cpu/ggml_fused_attn_blas.c
    cpu/ggml_fused_gate.c
    cpu/ggml_fused_norm.c
    cpu/ggml_fused_rope.c
    cpu/ggml_sparse.c
    cpu/ggml_final.c
)
target_include_directories(ggml_fused PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu
    ${MATH_INCLUDE_DIR}
)
target_link_libraries(ggml_fused PRIVATE m openblas)

# ─── CUDA 后端 (可选) ───
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    add_library(ggml_cuda SHARED
        cuda/src/cuda_ops.h
        cuda/sovereign_cuda_extension.cpp
    )
    target_include_directories(ggml_cuda PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/cuda
        ${MATH_INCLUDE_DIR}
    )
    set_target_properties(ggml_cuda PROPERTIES CUDA_ARCHITECTURES "61;70;80")
endif()
