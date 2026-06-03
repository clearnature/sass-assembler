// sov_format.h — 律算主权状态机 SOV v2.6 全息拓扑快照协议
//
// 宪法声明:
//   1. 彻底废弃 "权重字典" 概念。SOV 现为 "动态活体冻结仓"。
//   2. 严格的层级范畴标签 (Layer Tags)，禁止跨代数域污染。
//   3. 包含 LCM 384位高精度累加器快照 (系统的进位历史记忆)。
//   4. 包含陈数Q16、大泵相位、C3孤子相位、仲吕闭合计数 (空间与能级记忆)。
//   5. 摒弃 MD5/SHA256，采用基于 3-6-9 吸引子的数字根拓扑签名。
//
// 编译标准: C++23, 零浮点, 内存紧凑布局 (POD)
// 宪法依据: ARCHITECTURE_CONSTITUTION.md §4 (全息快照协议)
#ifndef SOV_FORMAT_V2_6_H
#define SOV_FORMAT_V2_6_H

#include <cstdint>
#include <cstddef>

namespace sov::io {

// ============================================================================
// 一、魔数与版本 — v2.6 硬隔离旧格式
// ============================================================================

inline constexpr uint32_t SOV_MAGIC       = 0x564F5354; // "TSOV" (Topological SOV)
inline constexpr uint32_t SOV_VERSION_2_6 = 0x00020600;

// ============================================================================
// 二、范畴标签 (Layer Tags) — 严格的物理隔离, 禁止跨代数域污染
// ============================================================================

enum class LayerTag : uint32_t {
    // [层1] GF(3) 纯量子态: 仅限 {T0,T1,T2} 模3运算, TQT_0 128-bit对齐存储
    TAG_L1_GF3_TENSOR   = 0x10000001,

    // [层2] Z/3¹¹Z 截断环: 包含进位历史, Tryte 存储 (0-728), 模177147运算
    TAG_L2_Z3R_RING     = 0x20000002,

    // [层3] 手性离合状态: 五行耦合系数 (空转/啮合/半联动/打滑/脱开)
    TAG_L3_CHIRAL_STATE = 0x30000003,

    // [层4-6] 泛音与拓扑网格数据: T⁶环面格点, C3孤子, 仲吕倍频, 频谱指纹
    TAG_TOPOLOGY_GRID   = 0x40000004
};

// ============================================================================
// 三、全息Header: 宇宙动力学快照 (SovHolographicHeader)
// ============================================================================
// 64字节定长, 记录主权状态机冻结瞬间的完整时空坐标。
// pragma pack(1) 确保跨平台二进制布局一致性, 可直接 mmap 映射。

#pragma pack(push, 1)
struct SovHolographicHeader {
    uint32_t magic;                   // [0]  必须是 SOV_MAGIC (0x564F5354)
    uint32_t version;                 // [4]  必须是 SOV_VERSION_2_6

    // --- 空间记忆 (Space & Phase) ---
    uint32_t grand_pump_step;         // [8]  [层4] 大泵呼吸步数 (0 - 6623)
    int32_t  chern_number_q16;        // [12] [层7] 陈数守卫Q16快照 (C=-2.000 → -131072)
    uint16_t c3_soliton_phase;        // [16] [层5] C3纳音孤子当前相位 (0 - 1499)
    uint32_t zhonglv_closure_count;   // [18] [层6] 仲吕闭合累计次数 (31000步→2583次)
    uint16_t padding_align;           // [22] [层0] 对齐至24字节边界

    // --- 时间记忆 (Time & Carry History) ---
    // [桥/层0] 384位 LCM 累加器 (6 × 64-bit limbs, 小端序)
    // 丢失此项将导致宇宙重启 — 177147进位历史全在此处
    // 加载时直接 memcpy 回 lcm_accumulator_t.value.limbs
    uint64_t lcm_accumulator_limbs[6]; // [24-72]
};
#pragma pack(pop)

static_assert(sizeof(SovHolographicHeader) == 72,
    "Header 必须精确为 72 字节: 4+4+4+4+2+4+2+48=72");
static_assert(offsetof(SovHolographicHeader, magic) == 0);
static_assert(offsetof(SovHolographicHeader, chern_number_q16) == 12);
static_assert(offsetof(SovHolographicHeader, zhonglv_closure_count) == 18);
static_assert(offsetof(SovHolographicHeader, lcm_accumulator_limbs) == 24);

// ============================================================================
// 四、张量块描述符 (SovBlockDescriptor) — 范畴隔离
// ============================================================================

#pragma pack(push, 1)
struct SovBlockDescriptor {
    LayerTag layer_tag;          // [0]  代数域范畴声明 — 加载时据此路由到L1/L2/L3
    uint32_t name_len;           // [4]  张量名称长度 (字节)
    uint32_t shape[4];           // [8]  拓扑维度 (N, C, H, W)
    uint64_t byte_size;          // [24] Payload 字节数 (必须是 16 的倍数)
};
#pragma pack(pop)

static_assert(sizeof(SovBlockDescriptor) == 32, "BlockDescriptor = 32 bytes");

// ============================================================================
// 五、TQT_0 物理存储契约 (SovTQT0Block128) — L1→L0桥接
// ============================================================================
// 128-bit 强制对齐, 内部绝对禁止定义加减乘除算符。
// 在解包成独立 trits 之前，只允许按位搬运。

struct alignas(16) SovTQT0Block128 {
    uint8_t raw_bytes[16];

    SovTQT0Block128() = default;
};
static_assert(sizeof(SovTQT0Block128) == 16, "TQT_0 block must be 128-bit");
static_assert(alignof(SovTQT0Block128) == 16, "TQT_0 block must be 128-bit aligned");

// ============================================================================
// 六、拓扑防篡改数字根签名 (SovDigitalRootSignature) — EOF 16字节
// ============================================================================
// 位于 SOV 文件末尾。将脆弱的浮点 Hash 替换为高维环面 {3,6,9} 不变量签名。
// 即便文件损坏几个比特，只要整体宏观数字根结构维持在 {3,6,9} 吸引子上，
// 就证明主权结构未崩塌 (拓扑鲁棒性)。

#pragma pack(push, 1)
struct SovDigitalRootSignature {
    uint32_t magic_tail;              // [0] 0x524F4F54 ("ROOT")
    uint8_t  payload_digital_root;    // [4] 载荷字节流的数字根 → 必须 ∈ {3,6,9}
    uint8_t  dynamic_phase_root;      // [5] 动力参数 (泵+孤子) 的复合数字根
    uint8_t  reserved[10];            // [6] 对齐到 16 字节
};
#pragma pack(pop)

static_assert(sizeof(SovDigitalRootSignature) == 16, "Signature must be 16 bytes");

} // namespace sov::io

#endif // SOV_FORMAT_V2_6_H
