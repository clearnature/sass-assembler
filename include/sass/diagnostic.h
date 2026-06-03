/* HunTian SASS — 统一诊断系统 (工业标准)
 *
 * 替代所有静默失败:
 *   之前: return {}; (用户不知道发生了什么)
 *   现在: EncodeResult{code, diags} (带文件:行:列定位)
 */

#ifndef SASS_DIAGNOSTIC_H
#define SASS_DIAGNOSTIC_H

#include <string>
#include <vector>
#include <cstdint>

namespace sass {

// ─── 诊断级别 ───
enum class DiagLevel { NOTE, WARN, ERROR, FATAL };

// ─── 单条诊断 ───
struct Diagnostic {
    DiagLevel level;
    int line = 0, col = 0;
    std::string message;

    static Diagnostic note(const std::string& msg)    { return {DiagLevel::NOTE,0,0,msg}; }
    static Diagnostic warn(const std::string& msg)    { return {DiagLevel::WARN,0,0,msg}; }
    static Diagnostic error(const std::string& msg)   { return {DiagLevel::ERROR,0,0,msg}; }
    static Diagnostic fatal(const std::string& msg)   { return {DiagLevel::FATAL,0,0,msg}; }

    bool is_error() const { return level >= DiagLevel::ERROR; }
};

// ─── 带诊断的编码结果 ───
struct EncodeResult {
    std::vector<uint64_t> code;
    std::vector<Diagnostic> diags;

    bool ok() const {
        for (auto& d : diags) if (d.is_error()) return false;
        return !code.empty();
    }

    static EncodeResult fail(const std::string& msg) {
        EncodeResult r;
        r.diags.push_back(Diagnostic::error(msg));
        return r;
    }
};

// ─── 反汇编结果 ───
struct DisasmResult {
    std::string text;
    std::vector<Diagnostic> diags;
    bool ok() const { return !diags.empty() ? false : !text.empty(); }
};

} // namespace sass

#endif
