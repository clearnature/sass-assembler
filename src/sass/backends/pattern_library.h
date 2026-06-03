/* HunTian SASS — 模式识别库
 *
 * DeepGEMM / SASS King 启发:
 *   识别常见的 SASS 编译模式，连接到源级优化决策。
 *
 * 用法:
 *   auto patterns = PatternMatcher::analyze(sass_code);
 *   for (auto& p : patterns) printf("%s\n", p.description);
 */

#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

namespace sass::patterns {

// ─── 模式类型 ───
enum class PatternType {
    MMA_CHAIN,           // HMMA/QMMA 累加器链
    ASYNC_COPY_PIPELINE,  // LDGSTS→DEPBAR→LDSM→MMA
    WARP_REDUCTION,       // SHFL 树形归约
    REGISTER_SPILL,       // STL/LDL 寄存器溢出
    LOOP_BACKEDGE,        // BRA 循环回边
    PREDICATED_EXIT,      // @P0 EXIT 边界检查
    FFMA_FUSION,          // FFMA 融合乘加
    CONSTANT_LOAD,        // LDC 常量加载
    SCOREBOARD_STALL,     // 依赖导致的等待
    UNKNOWN
};

struct Pattern {
    PatternType type;
    std::string name;
    std::string description;
    std::vector<uint64_t> signature;  // opcode sequence
    int confidence;  // 0-100
};

// ─── 模式匹配器 ───
class PatternMatcher {
public:
    static std::vector<Pattern> analyze(const std::vector<uint64_t>& code) {
        std::vector<Pattern> results;

        // 模式1-5: 检测常见模式
        if (detect_ffma_fusion(code))
            results.push_back(Pattern{PatternType::FFMA_FUSION, "FFMA融合",
                "连续FFMA→编译器已融合乘加", {}, 90});
        if (detect_loop_backedge(code))
            results.push_back(Pattern{PatternType::LOOP_BACKEDGE, "循环回边",
                "BRA负数偏移→循环体", {}, 95});
        if (detect_spill(code))
            results.push_back(Pattern{PatternType::REGISTER_SPILL, "寄存器溢出",
                "STL/LDL对→寄存器压力过高", {}, 80});
        if (detect_barrier_sync(code))
            results.push_back(Pattern{PatternType::WARP_REDUCTION, "屏障同步",
                "BAR.SYNC→CTA级同步点", {}, 95});
        if (detect_predicated_exit(code))
            results.push_back(Pattern{PatternType::PREDICATED_EXIT, "边界检查",
                "@P0 EXIT→循环边界检查", {}, 90});

        return results;
    }

private:
    static uint16_t op(uint64_t w) { return w & 0xFFFF; }

    static bool detect_ffma_fusion(const std::vector<uint64_t>& code) {
        int ffma_count = 0;
        for (auto w : code) {
            if (op(w) == 0x1000 || op(w) == 0x5980 || (w>>56)==0x59) ffma_count++;
        }
        return ffma_count >= 3;  // 3+ FFMA → 融合链
    }

    static bool detect_loop_backedge(const std::vector<uint64_t>& code) {
        for (auto w : code) {
            uint16_t o = op(w);
            if ((o == 0x7947 || (w>>56)==0xe2)) return true;  // BRA found
        }
        return false;
    }

    static bool detect_spill(const std::vector<uint64_t>& code) {
        bool has_stl=false, has_ldl=false;
        for (auto w : code) { uint16_t o=op(w); if(o==0x7980)has_stl=true; if(o==0x7980)has_ldl=true; }
        return has_stl && has_ldl;
    }

    static bool detect_barrier_sync(const std::vector<uint64_t>& code) {
        for (auto w : code) if (op(w)==0x7b1d||op(w)==0xc0ff||(w>>56)==0xf0) return true;
        return false;
    }

    static bool detect_predicated_exit(const std::vector<uint64_t>& code) {
        // @P0 EXIT pattern
        for (size_t i=0; i+1<code.size(); i++) {
            if (op(code[i])==0x794d && (code[i]>>56)==0xe3) return true;
        }
        return false;
    }
};

} // namespace sass::patterns
