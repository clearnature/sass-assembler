#!/bin/bash
# rocblas-build.sh — rocBLAS + Tensile 编译控制脚本
# GPU: RX 9060 XT (gfx1200 / RDNA4)
# TheRock: 7.13, LLVM: 23.0.0
# 编译目录: /data/ROCm/rocBLAS/build
# 目标: /opt/rocm/core-7.13

set -e

ROCBLAS_DIR="/data/ROCm/rocBLAS"
BUILD_DIR="$ROCBLAS_DIR/build"
INSTALL_PREFIX="/opt/rocm/core-7.13"
VENV_DIR="$HOME/venv/pytorch-rocm"

export CXX="/opt/rocm/core-7.13/lib/llvm/bin/clang++"
export CC="/opt/rocm/core-7.13/lib/llvm/bin/clang"
export CMAKE_BUILD_TYPE="Release"

_TOTAL_CORES=$(nproc)
_MAX_JOBS=$(( (_TOTAL_CORES + 1) / 2 ))
[ "$_MAX_JOBS" -gt 8 ] && _MAX_JOBS=8
export MAX_JOBS="${ROCBLAS_MAX_JOBS:-$_MAX_JOBS}"
echo "CPU: $_TOTAL_CORES cores, jobs: $MAX_JOBS"

activate_venv() {
    if [ -f "$VENV_DIR/bin/activate" ]; then
        source "$VENV_DIR/bin/activate"
        echo "venv: $VIRTUAL_ENV"
    else
        echo "WARN: venv not found at $VENV_DIR, continuing without"
    fi
}

show_stats() {
    if [ ! -d "$BUILD_DIR" ]; then
        echo "build dir: not created"
        return
    fi
    echo "build dir: $BUILD_DIR"
    echo "size: $(du -sh "$BUILD_DIR" 2>/dev/null | cut -f1)"
    if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
        echo "target: $(grep 'AMDGPU_TARGETS' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2)"
        echo "tensile: $(grep 'BUILD_WITH_TENSILE' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2)"
    fi
}

check_env() {
    echo "=== Check ==="
    echo "GPU: RX 9060 XT (gfx1200)"
    echo "Compiler: $CXX"
    $CXX --version 2>/dev/null | head -1
    echo "HIP: $(hipcc --version 2>/dev/null | head -1)"
    echo "Python: $(python3 --version 2>/dev/null)"
    echo "venv: $VENV_DIR"
    echo ""
    show_stats
}

configure() {
    echo "=== Configure ==="
    activate_venv
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake "$ROCBLAS_DIR" \
        -DCMAKE_CXX_COMPILER="$CXX" \
        -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_PREFIX_PATH="$INSTALL_PREFIX" \
        -DAMDGPU_TARGETS=gfx1200 \
        -DBUILD_WITH_TENSILE=ON \
        -DBUILD_WITH_HIPBLASLT=OFF \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -GNinja
    echo "=== Configure OK ==="
}

build() {
    echo "=== Build ==="
    if [ ! -f "$BUILD_DIR/build.ninja" ]; then
        configure
    fi
    cd "$BUILD_DIR"
    ninja -j"$MAX_JOBS"
    echo "=== Build OK ==="
}

install() {
    echo "=== Install ==="
    if [ ! -f "$BUILD_DIR/build.ninja" ]; then
        echo "build.ninja not found, run configure first"
        exit 1
    fi
    cd "$BUILD_DIR"
    sudo ninja install
    echo "=== Install OK ==="
}

clean() {
    echo "=== Clean ==="
    read -p "Delete $BUILD_DIR? (YES): " confirm
    if [ "$confirm" = "YES" ]; then
        rm -rf "$BUILD_DIR"
        echo "deleted"
    else
        echo "cancelled"
    fi
}

test_gpu() {
    echo "=== Test ==="
    activate_venv
    python3 -c "
import ctypes, torch
lib = ctypes.CDLL('$INSTALL_PREFIX/lib/librocblas.so')
print('rocBLAS loaded OK')
print('GPU:', torch.cuda.get_device_name(0))
a = torch.randn(1024,1024,device='cuda',dtype=torch.float16)
b = torch.randn(1024,1024,device='cuda',dtype=torch.float16)
c = a @ b
print('GEMM:', 'OK' if c.isfinite().all() else 'FAIL')
" || echo "Test failed (may need recompile)"
}

show_help() {
    echo "Usage: $0 {check|configure|build|install|clean|test|all|help}"
    echo ""
    echo "  check      - show environment"
    echo "  configure  - cmake only"
    echo "  build      - ninja build (auto-configure if needed)"
    echo "  install    - sudo ninja install"
    echo "  clean      - delete build dir"
    echo "  test       - verify GPU GEMM"
    echo "  all        - configure + build + install + test"
    echo "  help       - this message"
    echo ""
    echo "Config:"
    echo "  GPU:      gfx1200 (RX 9060 XT)"
    echo "  Compiler: $CXX"
    echo "  Prefix:   $INSTALL_PREFIX"
    echo "  Build:    $BUILD_DIR"
}

case "$1" in
    check)     check_env ;;
    configure) configure ;;
    build)     build ;;
    install)   install ;;
    clean)     clean ;;
    test)      test_gpu ;;
    all)       configure && build && install && test_gpu ;;
    help|"")   show_help ;;
    *)         echo "unknown: $1"; show_help; exit 1 ;;
esac
