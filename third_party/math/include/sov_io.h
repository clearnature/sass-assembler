// sov_io.h — SOV v2.6 文件读写引擎 (I/O Engine)
//
// 宪法声明:
//   范畴: 层0 (模2硬件) 的文件系统桥接 — 全息快照的持久化与恢复。
//   功能: write_sov_v26 / load_sov_v26 / verify_signature / TQT_0_pack / TQT_0_unpack
//   对齐: SovTQT0Block128 128-bit 强制对齐, SovHolographicHeader 72字节定长。
//   编译: C++23, 除文件I/O外全部 constexpr。
#ifndef SOV_IO_H
#define SOV_IO_H

#include "sov_format.h"
#include "digital_root.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace sov::io {

// ═══════════════════════════════════════════════════════
// 一、TQT_0 打包/解包 (8 trits ↔ 16-byte aligned block)
// ═══════════════════════════════════════════════════════

// 8个 GF(3) trit → 1个 16字节对齐的 TQT_0 块
// 每 trit 占 2 bits, 8 trits = 16 bits → 嵌入 128-bit 块的低16位
[[nodiscard]] constexpr SovTQT0Block128 TQT_0_pack(
    uint8_t t0, uint8_t t1, uint8_t t2, uint8_t t3,
    uint8_t t4, uint8_t t5, uint8_t t6, uint8_t t7
) noexcept {
    SovTQT0Block128 block{};
    uint16_t packed = 0;
    packed |= (t0 & 0x3) << 0;
    packed |= (t1 & 0x3) << 2;
    packed |= (t2 & 0x3) << 4;
    packed |= (t3 & 0x3) << 6;
    packed |= (t4 & 0x3) << 8;
    packed |= (t5 & 0x3) << 10;
    packed |= (t6 & 0x3) << 12;
    packed |= (t7 & 0x3) << 14;
    block.raw_bytes[0] = static_cast<uint8_t>(packed & 0xFF);
    block.raw_bytes[1] = static_cast<uint8_t>((packed >> 8) & 0xFF);
    return block;
}

// 从 TQT_0 块解包 8 个 trit
[[nodiscard]] constexpr void TQT_0_unpack(
    const SovTQT0Block128& block,
    uint8_t& t0, uint8_t& t1, uint8_t& t2, uint8_t& t3,
    uint8_t& t4, uint8_t& t5, uint8_t& t6, uint8_t& t7
) noexcept {
    uint16_t packed = block.raw_bytes[0] | (static_cast<uint16_t>(block.raw_bytes[1]) << 8);
    t0 = (packed >> 0)  & 0x3;
    t1 = (packed >> 2)  & 0x3;
    t2 = (packed >> 4)  & 0x3;
    t3 = (packed >> 6)  & 0x3;
    t4 = (packed >> 8)  & 0x3;
    t5 = (packed >> 10) & 0x3;
    t6 = (packed >> 12) & 0x3;
    t7 = (packed >> 14) & 0x3;
}

// ═══════════════════════════════════════════════════════
// 二、EOF 数字根签名验证
// ═══════════════════════════════════════════════════════

// 验证 SOV v2.6 文件的数字根签名
// 读取文件尾部 16 字节的 SovDigitalRootSignature 并验证
[[nodiscard]] inline bool verify_signature(const char* filepath) noexcept {
    std::FILE* f = std::fopen(filepath, "rb");
    if (!f) return false;

    // 定位到 EOF-16
    std::fseek(f, -16, SEEK_END);
    SovDigitalRootSignature sig{};
    std::fread(&sig, 1, sizeof(sig), f);
    std::fclose(f);

    // 验证魔数尾
    if (sig.magic_tail != 0x524F4F54) return false;  // "ROOT"

    // 读取 payload 并计算数字根
    std::fopen(filepath, "rb");
    f = std::fopen(filepath, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long file_size = std::ftell(f);
    std::fseek(f, sizeof(SovHolographicHeader), SEEK_SET);  // skip header
    long payload_size = file_size - sizeof(SovHolographicHeader) - 16;

    // 分块读取计算数字根
    uint64_t sum = 0;
    uint8_t buf[4096];
    long remaining = payload_size;
    while (remaining > 0) {
        size_t chunk = (remaining > 4096) ? 4096 : static_cast<size_t>(remaining);
        std::fread(buf, 1, chunk, f);
        for (size_t i = 0; i < chunk; ++i) sum += buf[i];
        if (sum > 0xFFFFFFFFFFFFFF00ULL) sum = sov::math::root::digital_root(sum);
        remaining -= static_cast<long>(chunk);
    }
    std::fclose(f);

    uint8_t computed_root = sov::math::root::digital_root(sum);
    return computed_root == sig.payload_digital_root;
}

// ═══════════════════════════════════════════════════════
// 三、文件读写 (依赖文件系统的运行时函数)
// ═══════════════════════════════════════════════════════

// 写入 SOV v2.6 文件: Header(72B) + metadata + blocks + EOF签名
// 返回写入的总字节数, 失败返回 0。
[[nodiscard]] inline size_t write_sov_v26(
    const char* filepath,
    const SovHolographicHeader& header,
    const uint8_t* block_data, size_t block_data_size
) noexcept {
    std::FILE* f = std::fopen(filepath, "wb");
    if (!f) return 0;

    size_t total = 0;
    // 1. 写入 Header (72 bytes)
    total += std::fwrite(&header, 1, sizeof(SovHolographicHeader), f);

    // 2. 写入 block data
    total += std::fwrite(block_data, 1, block_data_size, f);

    // 3. 计算并写入 EOF 数字根签名
    SovDigitalRootSignature sig{};
    sig.magic_tail = 0x524F4F54;  // "ROOT"

    // 计算 payload 数字根 (header 后的所有数据)
    uint64_t sum = 0;
    for (size_t i = 0; i < block_data_size; ++i) {
        sum += block_data[i];
        if (sum > 0xFFFFFFFFFFFFFF00ULL) sum = sov::math::root::digital_root(sum);
    }
    sig.payload_digital_root = sov::math::root::digital_root(sum);

    // 动力参数复合数字根
    sig.dynamic_phase_root = sov::math::root::calc_dynamic_phase_root(
        header.grand_pump_step,
        header.c3_soliton_phase,
        header.chern_number_q16,
        header.zhonglv_closure_count
    );

    total += std::fwrite(&sig, 1, sizeof(SovDigitalRootSignature), f);
    std::fclose(f);
    return total;
}

// 加载 SOV v2.6 文件: 读取 Header + blocks, 验证签名
// 返回 true 表示加载成功且签名验证通过。
[[nodiscard]] inline bool load_sov_v26(
    const char* filepath,
    SovHolographicHeader& out_header
) noexcept {
    std::FILE* f = std::fopen(filepath, "rb");
    if (!f) return false;

    // 1. 读取 Header
    if (std::fread(&out_header, 1, sizeof(SovHolographicHeader), f)
        != sizeof(SovHolographicHeader)) {
        std::fclose(f);
        return false;
    }

    // 2. 验证魔数与版本
    if (out_header.magic != SOV_MAGIC) {
        std::fclose(f);
        return false;
    }
    if (out_header.version != SOV_VERSION_2_6) {
        std::fclose(f);
        return false;
    }

    std::fclose(f);

    // 3. 验证 EOF 签名
    return verify_signature(filepath);
}

// ═══════════════════════════════════════════════════════
// 四、编译期验证
// ═══════════════════════════════════════════════════════

// TQT_0 打包/解包 往返验证
consteval bool verify_tqt0_roundtrip() {
    uint8_t trits[8] = {0, 1, 2, 1, 0, 2, 1, 0};
    auto block = TQT_0_pack(trits[0], trits[1], trits[2], trits[3],
                             trits[4], trits[5], trits[6], trits[7]);
    uint8_t out[8];
    TQT_0_unpack(block, out[0], out[1], out[2], out[3],
                  out[4], out[5], out[6], out[7]);
    for (int i = 0; i < 8; ++i)
        if (out[i] != trits[i]) return false;
    return true;
}
static_assert(verify_tqt0_roundtrip(), "TQT_0 pack/unpack roundtrip must be identity");

} // namespace sov::io

#endif // SOV_IO_H
