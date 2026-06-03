# HunTian SASS Assembler — 项目状态

> 最后更新: 2026-05-30

## 构建+测试
```bash
cmake -S . -B build && cmake --build build && ctest --test-dir build
# 20套件 440测试 100%通过
```

## 架构
- `include/sass/` — 15个API头 (instruction.h, assembler.h, device_backend.h...)
- `src/sass/` — 核心实现 (pascal_backend.h 518行, block_encoder.cpp, manifold_scheduler.cpp, optimizer.h, ilp_scheduler.h...)
- `src/lexer/` `src/parser/` — 词法/语法分析
- `tests/` — 20套件
- `docs/` — 文档 (IMPLEMENTATION_STATUS.md, MINDMAP.md...)

## 编码管线
```
.sass → Lexer → Parser → Scheduler(4320D) → PascalBackend.encode()
  ├─ optimize_p1() [排序+.reuse+死代码+常量+ILP+谓词]
  ├─ VAVX3 8:1融合
  └─ encode_instruction() [22 opcode家族, 108条指令]
    → .sabin / cubin注入 → GPU
```

## GPU验证
GTX 1060: SASS编码✅ cubin加载✅ 执行✅

## GP106硬件模型
/data/rtl-sdr/ptx_gp106/ — FMA=4cyc, Bank=4.85x, Reg>64=153x slow

<!-- gitnexus:start -->
# GitNexus — Code Intelligence

This project is indexed by GitNexus as **rtl-sdr** (719 symbols, 1392 relationships, 24 execution flows). Use the GitNexus MCP tools to understand code, assess impact, and navigate safely.

> If any GitNexus tool warns the index is stale, run `npx gitnexus analyze` in terminal first.

## Always Do

- **MUST run impact analysis before editing any symbol.** Before modifying a function, class, or method, run `gitnexus_impact({target: "symbolName", direction: "upstream"})` and report the blast radius (direct callers, affected processes, risk level) to the user.
- **MUST run `gitnexus_detect_changes()` before committing** to verify your changes only affect expected symbols and execution flows.
- **MUST warn the user** if impact analysis returns HIGH or CRITICAL risk before proceeding with edits.
- When exploring unfamiliar code, use `gitnexus_query({query: "concept"})` to find execution flows instead of grepping. It returns process-grouped results ranked by relevance.
- When you need full context on a specific symbol — callers, callees, which execution flows it participates in — use `gitnexus_context({name: "symbolName"})`.

## Never Do

- NEVER edit a function, class, or method without first running `gitnexus_impact` on it.
- NEVER ignore HIGH or CRITICAL risk warnings from impact analysis.
- NEVER rename symbols with find-and-replace — use `gitnexus_rename` which understands the call graph.
- NEVER commit changes without running `gitnexus_detect_changes()` to check affected scope.

## Resources

| Resource | Use for |
|----------|---------|
| `gitnexus://repo/rtl-sdr/context` | Codebase overview, check index freshness |
| `gitnexus://repo/rtl-sdr/clusters` | All functional areas |
| `gitnexus://repo/rtl-sdr/processes` | All execution flows |
| `gitnexus://repo/rtl-sdr/process/{name}` | Step-by-step execution trace |

## CLI

| Task | Read this skill file |
|------|---------------------|
| Understand architecture / "How does X work?" | `.claude/skills/gitnexus/gitnexus-exploring/SKILL.md` |
| Blast radius / "What breaks if I change X?" | `.claude/skills/gitnexus/gitnexus-impact-analysis/SKILL.md` |
| Trace bugs / "Why is X failing?" | `.claude/skills/gitnexus/gitnexus-debugging/SKILL.md` |
| Rename / extract / split / refactor | `.claude/skills/gitnexus/gitnexus-refactoring/SKILL.md` |
| Tools, resources, schema reference | `.claude/skills/gitnexus/gitnexus-guide/SKILL.md` |
| Index, status, clean, wiki CLI commands | `.claude/skills/gitnexus/gitnexus-cli/SKILL.md` |

<!-- gitnexus:end -->