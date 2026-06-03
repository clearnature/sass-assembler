# CMakeLists.txt — 主权律算数学库 (C++23, 纯整数域)
# pyBitNet 版本 — 供 sass-assembler 项目使用
cmake_minimum_required(VERSION 3.25)
project(sov_math LANGUAGES CXX C
    DESCRIPTION "Sovereign Law-Computation Math Library — GF(3) Pure Integer Domain"
    VERSION 2.5
)

# C++23 强制
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 优化标志
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -O3 -march=native")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2 -O3 -march=native")

# ============================================================================
# 一、库目标: sov_math (headers-only + C内核绑定)
# ============================================================================

add_library(sov_math INTERFACE)
target_include_directories(sov_math INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(sov_math INTERFACE cxx_std_23)

# 链接猎天C内核 (可选)
find_library(VAVX3_KERNEL
    NAMES vavx3_s2_kernels
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../huntian
    NO_DEFAULT_PATH
)
if(VAVX3_KERNEL)
    message(STATUS "VAVX3内核: ${VAVX3_KERNEL}")
    target_link_libraries(sov_math INTERFACE ${VAVX3_KERNEL})
else()
    message(STATUS "VAVX3内核未找到, 将使用纯C++回退路径")
endif()

# ============================================================================
# 二、测试
# ============================================================================

enable_testing()

# 桥接误差分析 (宪法标准)
add_executable(test_bridge_error tests/test_bridge_error.cpp)
target_link_libraries(test_bridge_error PRIVATE sov_math)

# 单元测试: 编译期LUT验证
add_executable(test_l5_l8_verify tests/test_l5_l8_verify.cpp)
target_link_libraries(test_l5_l8_verify PRIVATE sov_math)

# LUT 编译器测试
add_executable(test_lut_compile tests/test_lut_compile.cpp)
target_link_libraries(test_lut_compile PRIVATE sov_math)

# GMP 大数验证
add_executable(test_gmp_verify tests/test_gmp_verify.cpp)
target_link_libraries(test_gmp_verify PRIVATE sov_math)

# NSE 求解器测试
add_executable(test_nse_solver tests/test_nse_solver.cpp)
target_link_libraries(test_nse_solver PRIVATE sov_math)

# NSE GF(3) 验证
add_executable(test_nse_verify tests/test_nse_verify.cpp)
target_link_libraries(test_nse_verify PRIVATE sov_math)

# ============================================================================
# 三、安装
# ============================================================================

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/pybitnet
    FILES_MATCHING PATTERN "*.h"
)
install(TARGETS sov_math EXPORT sov_math_targets)
