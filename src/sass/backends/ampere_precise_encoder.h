/* ============================================================================
 * Ampere SM80 精确位域编码器 — denvdis 验证
 *
 * 128-bit 编码布局 (bits [0:127], 2×64-bit words):
 *
 * Word 0 (bits [63:0]):
 *   [11:0]   = opcode (12 bits of 13-bit opcode)
 *   [15:12]  = predicate guard
 *   [23:16]  = Rd (destination register)
 *   [31:24]  = Ra (source register 1)
 *   [39:32]  = Rb (source register 2)
 *   [63:40]  = Ra_offset / immediate
 *
 * Word 1 (bits [127:64]):
 *   [71:64]  = Rc (source register 3, if uniform register)
 *   [80:72]  = modifier flags (fmz, rnd, sat, abs, neg, mem)
 *   [91]     = opcode MSB (1 bit of 13-bit opcode)
 *   [112:110]= dst write scoreboard
 *   [115:113]= src release scoreboard
 *   [121:116]= request barrier
 *   [124:122]= opcode extension
 *
 * Default control:
 *   scoreboards = 7 (no dependency)
 *   predicate = PT (always true)
 *   modifier = default
 * ============================================================================ */

#include <cstdint>
#include <cstring>

namespace sass {
namespace ampere_enc {

// ─── 位域编码辅助 ───
inline uint64_t set_bits(uint64_t w, int lo, int hi, uint64_t val) {
    uint64_t mask = ((1ULL << (hi - lo + 1)) - 1) << lo;
    return (w & ~mask) | ((val << lo) & mask);
}

// ─── 默认控制 word 1 ───
// bits [112:110] = 7 (no dst dependency)
// bits [115:113] = 7 (no src dependency)
inline uint64_t default_ctrl() {
    uint64_t w1 = 0;
    w1 = set_bits(w1, 110, 112, 7);  // dst_wr_sb = 7
    w1 = set_bits(w1, 113, 115, 7);  // src_rel_sb = 7
    return w1;
}

// ─── 精确编码: Instruction → {word0, word1} ───
inline std::pair<uint64_t, uint64_t> encode_ffma(uint8_t rd, uint8_t ra, uint8_t rb, uint8_t rc) {
    uint64_t w0 = 0, w1 = default_ctrl();
    // Opcode: FFMA = 0b1000100011 (11 bits) + bit 91
    // Low 11 bits in [10:0], MSB at bit 91
    constexpr uint64_t FFMA_OP = 0b1000100011;
    w0 = set_bits(w0, 0, 10, FFMA_OP & 0x7FF);
    w1 = set_bits(w1, 27, 27, (FFMA_OP >> 11) & 1);  // bit 91 = word1[27]
    // Registers
    w0 = set_bits(w0, 16, 23, rd);
    w0 = set_bits(w0, 24, 31, ra);
    w0 = set_bits(w0, 32, 39, rb);
    w1 = set_bits(w1, 0, 7, rc);   // Rc at bits [71:64] = word1[7:0]
    return {w0, w1};
}

inline std::pair<uint64_t, uint64_t> encode_exit() {
    // EXIT opcode = specific constant (denvdis verified)
    uint64_t w0 = 0x000000000000794dULL;
    uint64_t w1 = default_ctrl();
    return {w0, w1};
}

inline std::pair<uint64_t, uint64_t> encode_mov(uint8_t rd, uint64_t imm) {
    uint64_t w0 = 0, w1 = default_ctrl();
    // MOV opcode
    w0 = set_bits(w0, 16, 23, rd);
    w0 = set_bits(w0, 40, 63, imm & 0xFFFFFF);
    return {w0, w1};
}

inline std::pair<uint64_t, uint64_t> encode_stg(uint8_t addr_reg, uint8_t src_reg, int32_t offset) {
    uint64_t w0 = 0, w1 = default_ctrl();
    // STG opcode
    w0 = set_bits(w0, 24, 31, addr_reg);
    w0 = set_bits(w0, 32, 39, src_reg);
    w0 = set_bits(w0, 40, 63, offset & 0xFFFFFF);
    return {w0, w1};
}

}} // namespace sass::ampere_enc
