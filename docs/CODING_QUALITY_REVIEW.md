# HunTian SASS Assembler — 代码审计

> 最后更新: 2026-05-28  
> 审查范围: 当前代码库全部源文件  
> C++ 标准: C++23 | 编译: GCC 零错误零警告

---

## 1. 项目架构（现行）

```
.sass → Lexer(60 opcodes) → Parser(55 opcodes + 标号表)
       → ManifoldScheduler(4320D 能量场 + 拉普拉斯曲率 + Yamabe 流)
       → BlockEncoder(VAVX3 融合 + 通用编码)
       → .sabin → ht-dis → 反汇编文本
```

### 源文件

| 层 | 文件 | 行数 | 功能 |
|----|------|------|------|
| CLI | `cli/main.cpp` | ~130 | ht-as 汇编器入口 |
| CLI | `cli/disassembler_main.cpp` | ~90 | ht-dis 反汇编器入口 |
| 前端 | `lexer/lexer.cpp` | ~140 | 词法分析 |
| 前端 | `parser/parser.cpp` | ~200 | 语法分析 + 标号表 |
| 后端 | `sass/block_encoder.cpp` | ~550 | 调度器 + 编码器 + 反汇编 |

### 头文件

| 文件 | 功能 |
|------|------|
| `instruction.h` | 指令 AST + 66 个 Opcode 硬件编码值 |
| `wrapper.h` | x86_64 用真实 huntian；非 x86 回退 |
| `wrapper.h` | 暴露 `vavx3_primitives.h` 的几何算子 |
| `ternary_wrapper.h` | 暴露 `ternary_types.h` + `pybitnet GF(3)` |
| `sass_types.h` | SASSWord、ManifoldResult、BlockEncoder 声明 |

---

## 2. 编译状态

### ✅ 通过 — 零错误零警告

```bash
cmake -S . -B build && cmake --build build
# sass_core.a + sass_lexer.a + sass_parser.a + ht-as + ht-dis
```

### 曾有的阻塞点（已修复）

| 原问题 | 修复方式 |
|--------|----------|
| Opcode/Operand/Instruction 在两个头文件重复定义 | 删除 `pascal_encoding.h`，统一到 `instruction.h` |
| PascalEncoder 声明无实现 | 重构为 BlockEncoder，全部在 `block_encoder.cpp` 实现 |
| include 路径不统一 | 统一为 `sass/xxx.h` 相对路径，CMake 补全 include_directories |
| high_quality_codegen.cpp 未注册 | 该文件已移除 |
| /data/... 注释路径 | 更新为 `third_party/huntian/` |

---

## 3. 包装层与引擎对接状态

### wrapper.h 链路

```
wrapper.h (x86_64)
  └→ wrapper.h
       ├→ third_party/huntian/vavx3_primitives.h  (122行 → 几何算子)
       └→ third_party/huntian/vavx3_blas.h         (206行 → BLAS)
```

**链路已通** ✅。`VAVX3_512i`、`laplacian()`、`yamabe_flow()`、`geo_rotate()` 等函数在 x86_64 上走真实 huntian 实现，非 x86 走标量回退。

### 调度器实际调用情况

| 组件 | 实现 | 是否调 huntian |
|------|------|---------------|
| 能量场投影 | 标量 `double[]` | ❌ 自实现 |
| 拉普拉斯曲率 | x86_64: 4096-bit SIMD (`vavx3_laplacian_512`) | ✅ **接通** |
|  | 非 x86: 标量 5-stencil | ❌ 回退 |
| Yamabe 流扩散 | 标量 `if-else` 邻居写入 | ❌ 自实现（有写冲突） |
| VAVX3 融合编码 | `encode_vavx3()` | ✅ 使用 huntian `vavx3_512i` 类型 |
| 指令编码 | `encode_std()` | ⚠️ 自实现（操作数编码逻辑） |

### 未对接的部分

- **Yamabe 流扩散**: 因相邻点存在写冲突，无法直接 SIMD 化
- **能量场投影**: 每条指令映射到不同槽位（scatter），SIMD 化收益有限
- **三进制引擎**: `ternary_wrapper.h` → `ternary_types.h` + `pybitnet` 链路已通，但调度器和编码器未调用

---

## 4. 功能完整度

### 汇编器 (ht-as)

| 功能 | 状态 |
|------|------|
| .sass 文本 → Token | ✅ 60 opcodes，含点分名 (xmad.mrg, vavx3.add) |
| Token → Instruction AST | ✅ 55 opcodes，标号表，标号引用解析 |
| 4320D 流形调度 | ✅ 能量场 → 拉普拉斯曲率 → Yamabe 流扩散 → 能量排序 |
| 4096-bit SIMD 加速 | ✅ 拉普拉斯计算 128 元素/块 (33 块覆盖 4320 槽位) |
| VAVX3 8:1 融合 | ✅ 连续同类型 8 条 → 1 条 512-bit |
| 操作数编码 4 种 | ✅ REG / IMM / MEM / LABEL |
| 分支偏移计算 | ✅ 标号表 → PC 相对偏移 |
| 输出 .sabin | ✅ 二进制写入 |

### 反汇编器 (ht-dis)

| 功能 | 状态 |
|------|------|
| 读 .sabin | ✅ |
| 55 opcodes 名称解码 | ✅ |
| 操作数解码 | ✅ 4 种类型 |
| 操作数个数控制 | ✅ 每类精确 (0/1/2/3) |
| 输出 .sass 文本 | ✅ |

### 指令集 (66 opcodes)

| 类 | 数量 | 状态 |
|----|------|------|
| 整数 | 12 | ✅ |
| 位操作 | 3 | ✅ |
| 浮点 | 7 | ✅ |
| XMAD | 4 | ✅ |
| 内存 | 5 | ✅ |
| 控制流 | 7 | ✅ |
| 同步 | 3 | ✅ |
| 数据移动 | 4 | ✅ |
| 转换+比较 | 5 | ✅ |
| VAVX3 | 8 | ✅ |
| 三进制 | 4 | ✅ |

---

## 5. 未解决的问题

### P0 — 效果验证

**无自动化 vs nvcc 对比基准**。当前"8:1 压缩"是在自生成的测试数据上测的，不是对真实 NVIDIA SASS 输出的对比。需要一个工具链：

```
nvcc kernel.cu → cuobjdump -sass → 实际 SASS 指令数
                                     ↓ 对比
ht-as kernel.sass              → 输出指令数
```

### P1 — 调度器优化

- **Yamabe 流扩散**: 当前标量实现有邻居写冲突，未 SIMD 化
- **能量场投影**: 散射式写入，未向量化
- **三进制引擎**: 链路已通但未集成到调度流程

### P2 — 架构扩展

- Volta/Turing 编码表（未实现）
- RDNA4 AMD 后端（未实现）
- 单元测试（零测试文件）

---

## 6. 结论

| 维度 | 评级 | 依据 |
|------|------|------|
| 编译 | ✅ | 零错误零警告 |
| 汇编流水线 | ✅ | .sass → .sabin 完整闭环 |
| 反汇编器 | ✅ | .sabin → .sass 双向验证通过 |
| 指令集覆盖 | ✅ | 55 条 Pascal + 8 VAVX3 + 4 三进制 |
| 4096-bit SIMD | ✅ | 拉普拉斯 128 元素并行 |
| 引擎对接 | ⚠️ | 链路通，部分未调 |
| vs nvcc 对比 | ❌ | 无自动化基准 |
| 测试 | ❌ | 零单元测试 |

**一句话**: 项目从"编译阻塞"推进到"可运行原型"。汇编反汇编完整闭环，4096-bit SIMD 已接入拉普拉斯计算。最缺的还是 vs nvcc 的自动对比基准。
