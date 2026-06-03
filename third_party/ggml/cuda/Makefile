# Sovereign V4 — CUDA Kernel Build
# 编译: make all
# ROCm 迁移: make HIP=1

NVCC = /usr/local/cuda-12.8/bin/nvcc
ARCH = -arch=sm_61
CC = gcc-14
BASE_FLAGS = -O3 -ccbin $(CC) -allow-unsupported-compiler -Xcompiler "-fPIC" -shared

# HIP 迁移预备 (AMD gfx1200)
ifdef HIP
    COMPILER = hipcc
    ARCH_FLAGS = --offload-arch=gfx1200
else
    COMPILER = $(NVCC)
    ARCH_FLAGS = $(ARCH)
endif

all: libsovereign_cuda.so libsparse_final.so libsparse_4k.so libsparse_bwd_4k.so libattn_fixed2.so libattn2.so

libsovereign_cuda.so: src/cuda_ops.cu
	$(COMPILER) $(BASE_FLAGS) $(ARCH_FLAGS) $< -o $@

libsparse_final.so: src/sparse_final.cu
	$(COMPILER) -O2 $(ARCH_FLAGS) -ccbin $(CC) -allow-unsupported-compiler -Xcompiler "-fPIC" -shared $< -o $@

libsparse_4k.so: src/sparse_4k.cu
	$(COMPILER) $(BASE_FLAGS) $(ARCH_FLAGS) $< -o $@

libsparse_bwd_4k.so: src/sparse_bwd_4k.cu
	$(COMPILER) $(BASE_FLAGS) $(ARCH_FLAGS) $< -o $@

libattn_fixed2.so: src/attn_fixed2.cu
	$(COMPILER) $(BASE_FLAGS) $(ARCH_FLAGS) $< -o $@

libattn2.so: src/attn2.cu
	$(COMPILER) $(BASE_FLAGS) $(ARCH_FLAGS) $< -o $@

clean:
	rm -f *.so

.PHONY: all clean
