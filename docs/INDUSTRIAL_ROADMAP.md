# HunTian → 工业标准路线图

## 架构重组

```
当前:
  PascalBackend(563行): 编码+反汇编混在一起
  VoltaBackend: 编码(JIT)+反汇编(if-else)+Ampere继承
  disassembler.cpp: HunTian .sabin格式反汇编 (不是真实SASS)

目标:
  include/sass/encoder.h       编码器统一接口
  include/sass/disassembler.h  反汇编器统一接口 + Error类型
  src/sass/encoder/
    pascal_encoder.cpp          纯编码 (从PascalBackend拆分)
    volta_encoder.cpp           JIT编码
    ampere_encoder.cpp          精确编码
  src/sass/disassembler/
    pascal_disasm.cpp           真实Pascal SASS → 文本
    volta_disasm.cpp            表驱动Volta反汇编
    ampere_disasm.cpp           表驱动Ampere反汇编
```

## 差距清单

### A. 错误诊断系统 (P0) — 4h

当前: 6处静默失败, 0错误消息
目标: Diagnostics 类型 + 文件:行:列 定位

```cpp
struct Diagnostic { int line,col; string msg; enum Level{WARN,ERROR,FATAL}; };
struct EncodeResult { vector<SASSWord> code; vector<Diagnostic> diags; };
```

### B. Pascal 反汇编器 (P0) — 3h

当前: disassembler.cpp 反汇编 .sabin (HunTian格式)
目标: 真实 Pascal SASS hex → 文本

已有 22 opcode 家族编码表 (pascal_backend.h)
反汇编过程: 读 byte7 → 查家族表 → 解码寄存器/立即数
→ 输出 "FFMA R0, R1, R2, RZ" 格式

实现: 表驱动, 每个家族一个 decode 函数

### C. Volta 反汇编器 重写 (P1) — 4h

当前: 53条 if-else 链
目标: constexpr 查找表 + 结构化输出

已有 volta_opcode_table (256条目的constexpr数组)
反汇编: opcode16 → 查表 → 输出文本
表结构: {uint16_t op; const char* mnemonic; int operand_count;}

### D. Ampere 反汇编器 独立 (P1) — 2h

当前: 继承 Volta, 仅覆写12条
目标: 独立 ampere_opcode_table + 新指令

需要: FP16 (HADD2/HMUL2/HFMA2) + Tensor (MMA) + barrier新编码

### E. 编码器拆分 (P1) — 2h

PascalBackend(563行) → pascal_encoder.cpp + pascal_disasm.cpp

### F. 往返验证 (P1) — 1h

encode → disassemble → 检查: 操作码+寄存器一致

### G. 统一错误传播 (P0) — 2h

所有 encode() 返回 EncodeResult (不是 vector<SASSWord>)
所有 disassemble() 返回字串 + diag

### H. ht-as 诊断输出 (P0) — 1h

--verbose 模式输出: 指令数, 优化pass效果, 模式分析, 寄存器压力

### I. 文档生成 (P2) — 2h

ht-as --list-opcodes → 输出所有支持的操作码表
ht-as --list-archs → 输出架构列表

### J. 性能回归测试 (P2) — 2h

固定输入 → 固定输出 → 对比基准
100k指令编码 < 100ms
100k指令反汇编 < 50ms

## 实现顺序

P0 (工业可用门槛): A→B→G→H = 10h
P1 (完整): C→D→E→F = 9h
P2 (完善): I→J = 4h

总计: 23h (~3天)
