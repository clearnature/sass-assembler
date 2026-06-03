# 社区 SASS 逆向资源索引

> 日期: 2026-05-29

## 关键项目

### 1. CuAssembler (cloudcores) — ⭐590
- **地址**: https://github.com/cloudcores/CuAssembler
- **描述**: 非官方 CUDA 汇编器，支持所有 SASS 世代
- **语言**: Python
- **数据**: OpcodeTable 目录包含多架构 opcode 表
- **目标架构**: Fermi → Ampere (推测)

### 2. SASS King (florianmattana) — ⭐268
- **地址**: https://github.com/florianmattana/sass-king
- **描述**: SM120/SM120a Blackwell 指令逆向 + 模式库
- **语言**: CUDA
- **数据**: 
  - `knowledge/SASS_INSTRUCTIONS_SM120.md` — SM120 指令词汇表
  - `knowledge/encoding/` — LDSM/STSM/QMMA 编码笔记
  - `patterns/` — 29 个可复用 SASS 模式
- **工具**: denvdis (redplait) 作为 bit-level 交叉验证

### 3. denvdis (redplait)
- **地址**: https://github.com/redplait/denvdis
- **描述**: bit-level SASS 反汇编器 + cubin 编辑器
- **数据**: `data12/` 目录包含 SM 系列 opcode 表
- **支持**: sm_50 → sm_120

### 4. MaxAs (NervanaSystems, Intel) — 已归档
- **地址**: https://github.com/NervanaSystems/maxas
- **描述**: Maxwell 架构 SASS 汇编器
- **关联**: Nervana Neon 有大量 SASS 示例

### 5. CudaPAD (SunsetQuest) — ⭐129
- **地址**: https://github.com/SunsetQuest/CudaPAD
- **描述**: PTX/SASS 实时查看器

## 使用方式

```bash
# 下载社区 opcode 表
git clone https://github.com/cloudcores/CuAssembler.git external/CuAssembler
git clone https://github.com/florianmattana/sass-king.git external/sass-king

# 提取 opcode 数据
python3 tools/ingest_community_data.py external/
```
