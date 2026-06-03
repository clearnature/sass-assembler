/* ============================================================================
 * HunTian SASS 汇编器 — 设备后端统一接口
 *
 * 抽象不同 GPU 架构的编码/反汇编差异。
 * 每个架构实现一个后端:
 *   PascalBackend  → sm_60, sm_61, sm_62
 *   VoltaBackend   → sm_70, sm_72
 *   TuringBackend  → sm_75
 *   AmpereBackend  → sm_80, sm_86
 *   AdaBackend     → sm_89
 *   HopperBackend  → sm_90
 *   BlackwellBackend → sm_100+
 *
 * 使用:
 *   auto backend = DeviceBackend::create("sm_80");
 *   backend->encode(program);
 * ============================================================================ */

#ifndef SASS_DEVICE_BACKEND_H
#define SASS_DEVICE_BACKEND_H

#include "sass_types.h"
#include "instruction.h"
#include "diagnostic.h"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace sass {

// ─── 架构描述 ───
struct ArchInfo {
    std::string name;       // "Pascal"
    std::string sm;         // "sm_61"
    int compute_capability; // 61
    int generation;         // 6 (Pascal)
};

// ─── 设备后端抽象接口 ───
class IDeviceBackend {
public:
    virtual ~IDeviceBackend() = default;

    // ─── 元信息 ───
    virtual ArchInfo arch() const = 0;

    // ─── 编码: Instruction → SASSWord ───
    virtual std::vector<SASSWord> encode(const ManifoldResult& result) = 0;
    virtual std::vector<SASSWord> encode_with_labels(
        const ManifoldResult& result,
        const std::unordered_map<std::string, uint32_t>& labels) = 0;

    // ─── 反汇编: SASSWord → 文本 ───
    virtual std::string disassemble(SASSWord w) const = 0;
    virtual std::vector<std::string> disassemble_block(
        const std::vector<SASSWord>& code) const = 0;

    // ─── Opcode 查询 ───
    virtual uint8_t opcode_byte(Opcode op) const = 0;
    virtual const char* opcode_name(Opcode op) const = 0;
    virtual int operand_count(Opcode op) const = 0;

    // ─── 验证 ───
    virtual bool supports(Opcode op) const = 0;
    virtual size_t total_instructions() const = 0;

    // ─── 诊断 (工业标准) ───
    virtual const std::vector<Diagnostic>& last_diagnostics() const {
        static std::vector<Diagnostic> empty;
        return empty;
    }
};

// ─── 后端工厂 ───
class DeviceBackend {
public:
    static std::unique_ptr<IDeviceBackend> create(const std::string& sm_version);
    static std::vector<std::string> supported_archs();
};

} // namespace sass

#endif // SASS_DEVICE_BACKEND_H
