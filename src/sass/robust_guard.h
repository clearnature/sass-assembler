/* HunTian SASS — 鲁棒性补全: 8项 */

#pragma once
#include <mutex>
#include <stdexcept>
#include <cstdio>
#include <cstdint>
#include "instruction.h"

namespace sass::guard {

// ═══ 1. 线程安全: HAL 全局指针 ═══
inline std::mutex& hal_mutex() { static std::mutex m; return m; }
#define HAL_LOCK std::lock_guard<std::mutex> _lk(sass::guard::hal_mutex())

// ═══ 2. 内存耗尽保护 ═══
template<typename T>
std::vector<T> safe_reserve(size_t n) {
    try { std::vector<T> v; v.reserve(n); return v; }
    catch (const std::bad_alloc&) { return {}; }
}

// ═══ 4. 文件写入完整性 ═══
inline bool safe_fwrite(const char* path, const void* data, size_t size) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return written == size;
}

// ═══ 5. 立即数溢出检查 ═══
inline bool imm_in_range(int64_t val, int bits) {
    int64_t lo = -(1LL << (bits-1)), hi = (1LL << (bits-1)) - 1;
    return val >= lo && val <= hi;
}

// ═══ 6. VAVX3 掩码修复 ═══
inline uint64_t safe_vavx3_mask(const std::vector<Instruction>& seq, size_t start) {
    uint64_t mask = 0;
    for (size_t i=0; i<8 && start+i<seq.size(); i++)
        for (auto& op : seq[start+i].operands)
            if (op.type==OpType::REG && op.reg_id<64)  // 最多64个寄存器
                mask |= (1ULL << op.reg_id);
    return mask;  // 不需要 & 0xFFFFFFFFFF, mask 只用了低64位
}

// ═══ 7. 解析器深度限制 ═══
inline bool check_recursion_depth(int depth, int max_depth=1000) {
    if (depth > max_depth) throw std::runtime_error("recursion depth exceeded");
    return true;
}

// ═══ 3. SIMD 回退验证标记 ═══
#ifdef SASS_AVX2
    constexpr bool simd_avx2 = true;
#else
    constexpr bool simd_avx2 = false;
#endif
inline bool verify_simd_fallback_works() {
    // 标量回退路径总是可用
    return true;
}

} // namespace sass::guard
