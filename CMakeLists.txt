cmake_minimum_required(VERSION 3.28)
project(HunTianSASS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -O2)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-unused-parameter -Wno-unused-variable)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    add_compile_options(-mavx2)
    add_compile_definitions(SASS_AVX2=1)
endif()

# 全局 include（仅项目内部路径）
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/sass
    ${PROJECT_SOURCE_DIR}
)

# ═══════════════════════════════════════════════════════════════
# 第三方库
# ═══════════════════════════════════════════════════════════════

# 主权律算数学库 (GF(3) 纯整数域)
add_subdirectory(third_party/math)

# GGML 推理层 (量化 + 融合算子)
add_subdirectory(third_party/ggml)

# ═══════════════════════════════════════════════════════════════
# 库
# ═══════════════════════════════════════════════════════════════

# SASS 核心编码器 (仅 manifold_scheduler.cpp 需要 huntian 几何引擎)
add_library(sass_core STATIC
    src/sass/block_encoder.cpp
    src/sass/manifold_scheduler.cpp
    src/sass/disassembler.cpp
    src/sass/assembler.cpp
    src/sass/device_backend.cpp
    src/sass/pascal_hal.cpp
)
target_include_directories(sass_core PRIVATE ${PROJECT_SOURCE_DIR}/third_party/huntian ${PROJECT_SOURCE_DIR}/third_party/huntian/llvm/Support)

# Lexer 词法分析器
add_library(sass_lexer STATIC src/lexer/lexer.cpp)

# Parser 语法分析器
add_library(sass_parser STATIC src/parser/parser.cpp)

# ht-as: 汇编器
add_executable(ht-as src/cli/main.cpp)
target_link_libraries(ht-as PRIVATE sass_core sass_lexer sass_parser)

# ht-dis: 反汇编器
add_executable(ht-dis src/cli/disassembler_main.cpp)
target_link_libraries(ht-dis PRIVATE sass_core)

enable_testing()

add_executable(test_lexer tests/test_lexer.cpp)
target_include_directories(test_lexer PRIVATE ${PROJECT_SOURCE_DIR}/include tests)
target_link_libraries(test_lexer PRIVATE sass_lexer)
add_test(NAME LexerTests COMMAND test_lexer)

add_executable(test_parser tests/test_parser.cpp)
target_include_directories(test_parser PRIVATE ${PROJECT_SOURCE_DIR}/include tests)
target_link_libraries(test_parser PRIVATE sass_lexer sass_parser)
add_test(NAME ParserTests COMMAND test_parser)

add_executable(test_operand_codec tests/test_operand_codec.cpp)
target_include_directories(test_operand_codec PRIVATE ${PROJECT_SOURCE_DIR}/include tests)
add_test(NAME OperandCodecTests COMMAND test_operand_codec)

add_executable(test_opcode_table tests/test_opcode_table.cpp)
target_include_directories(test_opcode_table PRIVATE ${PROJECT_SOURCE_DIR}/include tests)
add_test(NAME OpcodeTableTests COMMAND test_opcode_table)

add_executable(test_reuse_detector tests/test_reuse_detector.cpp)
target_include_directories(test_reuse_detector PRIVATE ${PROJECT_SOURCE_DIR}/include tests)
add_test(NAME ReuseDetectorTests COMMAND test_reuse_detector)

add_executable(test_manifold_scheduler tests/test_manifold_scheduler.cpp)
target_include_directories(test_manifold_scheduler PRIVATE ${PROJECT_SOURCE_DIR}/include tests)
target_link_libraries(test_manifold_scheduler PRIVATE sass_core)
add_test(NAME ManifoldSchedulerTests COMMAND test_manifold_scheduler)

add_executable(test_block_encoder tests/test_block_encoder.cpp)
target_include_directories(test_block_encoder PRIVATE ${PROJECT_SOURCE_DIR}/include tests)
target_link_libraries(test_block_encoder PRIVATE sass_core)
add_test(NAME BlockEncoderTests COMMAND test_block_encoder)

add_executable(test_assembler tests/test_assembler.cpp)
target_include_directories(test_assembler PRIVATE ${PROJECT_SOURCE_DIR}/include tests)
target_link_libraries(test_assembler PRIVATE sass_core sass_lexer sass_parser)
add_test(NAME AssemblerTests COMMAND test_assembler)

add_executable(test_pascal_backend tests/test_pascal_backend.cpp)
target_include_directories(test_pascal_backend PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/sass tests)
target_link_libraries(test_pascal_backend PRIVATE sass_core)
add_test(NAME PascalBackendTests COMMAND test_pascal_backend)

add_executable(test_disassembler tests/test_disassembler.cpp)
target_include_directories(test_disassembler PRIVATE ${PROJECT_SOURCE_DIR}/include tests)
target_link_libraries(test_disassembler PRIVATE sass_core)
add_test(NAME DisassemblerTests COMMAND test_disassembler)

add_executable(test_simd_kernels tests/test_simd_kernels.cpp)
target_include_directories(test_simd_kernels PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/sass tests)
add_test(NAME SimdKernelTests COMMAND test_simd_kernels)

add_executable(test_huntian_integration tests/test_huntian_integration.cpp)
target_include_directories(test_huntian_integration PRIVATE ${PROJECT_SOURCE_DIR}/include tests)
add_test(NAME HuntianIntegrationTests COMMAND test_huntian_integration)

add_executable(test_pascal_verify tests/test_pascal_verify.cpp)
target_include_directories(test_pascal_verify PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/sass tests)
target_link_libraries(test_pascal_verify PRIVATE sass_core)
add_test(NAME PascalVerifyTests COMMAND test_pascal_verify)

add_executable(test_volta_backend tests/test_volta_backend.cpp)
target_include_directories(test_volta_backend PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/sass tests)
target_link_libraries(test_volta_backend PRIVATE sass_core)
add_test(NAME VoltaBackendTests COMMAND test_volta_backend)

add_executable(test_ampere_backend tests/test_ampere_backend.cpp)
target_include_directories(test_ampere_backend PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/sass tests)
target_link_libraries(test_ampere_backend PRIVATE sass_core)
add_test(NAME AmpereBackendTests COMMAND test_ampere_backend)

add_executable(test_volta_encode tests/test_volta_encode.cpp)
target_include_directories(test_volta_encode PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/sass tests)
target_link_libraries(test_volta_encode PRIVATE sass_core)
add_test(NAME VoltaEncodeTests COMMAND test_volta_encode)

add_executable(test_new_archs tests/test_new_archs.cpp)
target_include_directories(test_new_archs PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/sass tests)
target_link_libraries(test_new_archs PRIVATE sass_core)
add_test(NAME NewArchTests COMMAND test_new_archs)

add_executable(test_e2e_archs tests/test_e2e_archs.cpp)
target_include_directories(test_e2e_archs PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/sass tests)
target_link_libraries(test_e2e_archs PRIVATE sass_core sass_lexer sass_parser)
add_test(NAME E2EArchTests COMMAND test_e2e_archs)

add_executable(test_ampere_encode tests/test_ampere_encode.cpp)
target_include_directories(test_ampere_encode PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/sass tests)
target_link_libraries(test_ampere_encode PRIVATE sass_core)
add_test(NAME AmpereEncodeTests COMMAND test_ampere_encode)

add_executable(test_optimizer tests/test_optimizer.cpp)
target_include_directories(test_optimizer PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/sass tests)
add_test(NAME OptimizerTests COMMAND test_optimizer)

add_executable(test_task_sched benchmarks/test_task_sched.cpp)
target_include_directories(test_task_sched PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/sass tests)
target_link_libraries(test_task_sched PRIVATE sass_core)
add_test(NAME TaskSchedTests COMMAND test_task_sched)

add_executable(test_industrial tests/test_industrial.cpp)
target_include_directories(test_industrial PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/sass tests)
target_link_libraries(test_industrial PRIVATE sass_core)
add_test(NAME IndustrialTests COMMAND test_industrial)

install(TARGETS ht-as ht-dis RUNTIME DESTINATION bin)
