HunTian SASS 汇编器 — 思维导图
=====================================

                    ┌─────────────────────────────────┐
                    │         ht-as CLI (main.cpp)      │
                    │   -o output  --4320d  --fuse      │
                    └──────────────┬──────────────────┘
                                   │ assembler.assemble(source)
                    ┌──────────────▼──────────────────┐
                    │     Assembler Facade             │
                    │     (src/sass/assembler.cpp)      │
                    │                                  │
                    │  tokenize → parse → optimize →   │
                    │  encode                         │
                    └──────┬───────┬───────┬─────────┘
                           │       │       │
         ┌─────────────────┘       │       └──────────────────┐
         ▼                         ▼                          ▼
┌─────────────────┐    ┌────────────────────┐    ┌──────────────────────┐
│   Lexer         │    │   Parser           │    │   Scheduler          │
│ (lexer.cpp)     │    │ (parser.cpp)       │    │ (manifold_           │
│                 │    │                    │    │  scheduler.cpp)      │
│ Tokenize:       │    │ Parse:             │    │                      │
│  OPCODE         │    │  func              │    │ 4320D 流形调度:       │
│  REGISTER       │    │  instruction       │    │  能量场投影(4320槽)   │
│  IMMEDIATE      │    │  operands          │    │  拉普拉斯曲率        │
│  MEM            │    │  labels            │    │  Yamabe流平滑         │
│  LABEL          │    │                    │    │  能量排序输出         │
│  VAVX3          │    │ → ParseResult      │    │                      │
│                 │    │   + instructions    │    │ → ManifoldResult      │
│ → Token[]       │    │   + labels         │    │   + optimized_seq     │
└─────────────────┘    └────────────────────┘    │   + bubble_score     │
                                                  └──────────┬───────────┘
                                                             │
                                    ┌────────────────────────┘
                                    ▼
                         ┌─────────────────────────────────────┐
                         │        Encoder (多后端)              │
                         │                                     │
                         │  BlockEncoder (HunTian .sabin)       │
                         │    ├─ VAVX3 8:1 融合 (0xF1/0xF2)     │
                         │    ├─ .reuse 自动检测                 │
                         │    └─ .sabin 64位自定义格式           │
                         │                                     │
                         │  PascalBackend (真实GPU)              │
                         │    ├─ optimize_p1() 优化管线          │
                         │    │   ├─ schedule_order (4320D)      │
                         │    │   ├─ auto_reuse (.reuse检测)     │
                         │    │   ├─ dead_code_elimination       │
                         │    │   ├─ const_propagation           │
                         │    │   ├─ ilp_schedule (GP106校准)    │
                         │    │   └─ predicate_optimize          │
                         │    ├─ VAVX3 can_fuse + encode_vavx3   │
                         │    └─ encode_instruction()            │
                         │        ├─ 0xe3 EXIT/RET               │
                         │        ├─ 0x59 FFMA                   │
                         │        ├─ 0x4e/0x4f/0x5b XMAD家族     │
                         │        ├─ 0x4c ALU (MOV/IADD/SHL...)  │
                         │        ├─ 0xf0 FLOAT (FADD/FMUL...)   │
                         │        ├─ 0xee/0xef MEM (STG/LDG...)  │
                         │        ├─ ... (22 opcode家族)         │
                         │        └─ 0x50 MUFU/NOP/VOTE          │
                         │                                     │
                         │  VoltaBackend (128-bit)               │
                         │    └─ constexpr O(1) fast_encode      │
                         │                                     │
                         │  AmpereBackend (FP16/Tensor)          │
                         │  Ada/Hopper/Blackwell (继承)          │
                         │                                     │
                         │ → std::vector<SASSWord>               │
                         └──────────────────┬──────────────────┘
                                            │
                              ┌─────────────┴─────────────┐
                              ▼                           ▼
                     ┌──────────────┐          ┌────────────────────┐
                     │  .sabin      │          │  .cubin (GPU执行)   │
                     │  HunTian格式 │          │  cubin_patcher.h    │
                     │  自定义64位  │          │  纯C ELF注入器      │
                     └──────────────┘          │  → GTX1060验证 ✅   │
                                               └────────────────────┘

═══════════════════════════════════════════════════════════════════
                              优化器管线
═══════════════════════════════════════════════════════════════════

optimize_p0():                          optimize_p1(): 调用P0 +:
  schedule_order (4320D cycle排序)        const_propagation (常量折叠)
  auto_reuse    (.reuse检测)              ilp_schedule    (ILP Scoreboard)
  dead_code_elim (EXIT后删除)             predicate_optimize (谓词合并)

optimize_p1() 已集成到 PascalBackend.encode()
reg_allocate() 可选 (opt_reg_alloc=true)
loop_unroll() 已实现但未集成 (仅降低复杂度分)

═══════════════════════════════════════════════════════════════════
                              硬件模型
═══════════════════════════════════════════════════════════════════

/data/rtl-sdr/ptx_gp106/             集成到:
  GP106拓扑: 10 SMs, 128 cores/SM     ILP调度器延迟模型
  FMA延迟: 4 cycles                   fp_latency=4
  INT延迟: 4 cycles                   int_latency=4
  Bank冲突: stride=32 → 4.85x        共享内存感知
  寄存器压力: >64 → 153x slowdown     MAX_REGS=48
  SASS编码规则                        PascalBackend opcode表

═══════════════════════════════════════════════════════════════════
                              测试覆盖
═══════════════════════════════════════════════════════════════════

20 套件 | 440 测试 | 100%通过

LexerTests           76  词法分析
ParserTests          32  语法分析
OperandCodecTests    ~10 操作数编解码
OpcodeTableTests     ~20 操作码表
ReuseDetectorTests   ~10 .reuse检测
ManifoldSchedulerTests 8 4320D调度
BlockEncoderTests    ~15 VAVX3融合
AssemblerTests       ~10 全流程
PascalBackendTests   ~30 Pascal编码
DisassemblerTests    ~10 反汇编
SimdKernelTests      ~8  4096-bit SIMD
HuntianIntegrationTests ~10 集成测试
PascalVerifyTests    ~265 cuobjdump验证
VoltaBackendTests    ~30 Volta后端
AmpereBackendTests   ~15 Ampere后端
VoltaEncodeTests     ~35 Volta编码
NewArchTests         ~11 Ada/Hopper/Blackwell
E2EArchTests         ~5  端到端
AmpereEncodeTests    ~20 Ampere编码
OptimizerTests       12  优化器

═══════════════════════════════════════════════════════════════════
                          指令系统 (127条)
═══════════════════════════════════════════════════════════════════

108 原生 CUDA (Pascal→Blackwell)
  整数:   IADD,IADD3,ISUB,IMAD,IMUL,IMNMX,SHL,SHR,BFE,BFI,LOP3...
  浮点:   FADD,FMUL,FFMA,FSET,FMNMX,FSEL,FRCP,FSQRT,FCMP,MUFU...
  内存:   LDG,LDS,STG,STS,LDC,LDL,STL
  控制:   BRA,BRX,CAL,RET,EXIT,BAR,DEPBAR,MEMBAR
  数据:   MOV,MOV32I,SEL,S2R,PRMT,SHFL
  双精度: DADD,DMUL,DFMA,DMNMX,DSET,DSETP
  XMAD:   XMAD,XMAD_MRG,XMAD_PSL,XMAD_CHI
  原子:   ATOM,RED
  纹理:   TEX,TLD,TLD4,TXQ,SULD,SULEA,SUST,SURED,SUQ

8 VAVX3 虚拟指令 (0xF0-0xF7)
  VAVX3_ADD/MUL/MAD/MMA_512, GEOM, SHUFFLE, BRAID, LAPLACIAN

3 三进制 (0xF8-0xFB)
  TMAD, TMUL, TCONV, TRYTE_OP

8 Tensor/Hopper (0x80-0x87)
  WGMMA_ARRIVE/COMMIT/WAIT, TMA_LOAD/STORE, MBARRIER_ARRIVE, ...

═══════════════════════════════════════════════════════════════════
                          数据流
═══════════════════════════════════════════════════════════════════

.sass文本 → Lexer(Token[]) → Parser(ParseResult)
  → ManifoldScheduler(4320D → ManifoldResult)
    → PascalBackend.encode()
       ├─ optimize_p1() [排序+.reuse+死代码+常量+ILP+谓词]
       ├─ VAVX3检测 [8条连续FFMA/XMAD?]
       ├─ encode_vavx3() [0xF1标记+寄存器掩码]
       └─ encode_instruction() [22个opcode家族]
         → SASSWord[]
           ├─ .sabin (HunTian格式, 磁盘)
           └─ cubin注入 (ELF, GPU执行✅)
