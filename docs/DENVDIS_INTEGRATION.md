# denvdis 数据使用说明

## 已下载文件

| 文件 | 大小 | 行数 | 架构 |
|------|------|------|------|
| `external/denvdis_data/sm75_1.txt` | 4.0 MB | Turing |
| `external/denvdis_data/sm80_1.txt` | 4.3 MB | 108,789 | Ampere GA100 |
| `external/denvdis_data/sm86_1.txt` | 4.5 MB | Ampere GA104 |
| `external/denvdis_data/sm120_1.txt` | 7.4 MB | Blackwell GB202 |

## 格式说明

denvdis 使用自定义架构描述语言。关键段:
- `ARCHITECTURE "Volta"` — 架构声明
- `REGISTERS` — 寄存器定义
- `PSEUDO_OPCODE` — 指令变体映射 (MOV/SHL/ISCADD/IADD)
- `EXTRACT` — 位域提取定义
- 指令定义 — 包含完整操作数格式

## 解析要点

1. PSEUDO_OPCODE 段: 多用途 opcode 的实际指令映射
   ```
   PSEUDO_OPCODE1 "nopseudo_opcode"=0, "SHL"=1, "ISCADD"=2, "IADD"=3, "MOV"=4;
   ```
2. Register@RZ -> PSEUDO_OPCODE1@MOV 表示 RZ 寄存器触发 MOV 编码

## 与 HunTian 集成

这些数据可用于:
- 验证我们已有的 Volta/Ampere opcode
- 补全缺失的指令编码
- 构建 Ampere+ FP16/Tensor 精确编码器
- Blackwell (SM120) 后端开发

## 下一步

编写 denvdis 解析器，生成 HunTian 兼容的 opcode 映射表。
