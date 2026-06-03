/* ============================================================================
 * HunTian SASS 汇编器 — 多架构后端工厂实现
 * ============================================================================ */

#include "device_backend.h"
#include "pascal_backend.h"
#include "backends/volta_backend.h"
#include "backends/x86_backend.h"
#include "backends/amd_rdna4.h"
#include <algorithm>

namespace sass {

// 前向声明各后端
class PascalBackend;
class VoltaBackend;
class AmpereBackend;

std::unique_ptr<IDeviceBackend> DeviceBackend::create(const std::string& sm) {
    // Pascal 家族 (sm_60-62): 64-bit 指令, 已验证
    if (sm == "sm_60" || sm == "sm_61" || sm == "sm_62")
        return std::make_unique<PascalBackend>();

    // Volta 家族 (sm_70-72): 128-bit 指令, 编码格式不同
    if (sm == "sm_70" || sm == "sm_72")
        return std::make_unique<VoltaBackend>();

    // Turing (sm_75): 128-bit, 同Volta
    if (sm == "sm_75")
        return std::make_unique<TuringBackend>();

    // Ampere (sm_80, sm_86)
    if (sm == "sm_80" || sm == "sm_86")
        return std::make_unique<AmpereBackend>();

    // Ada (sm_89)
    if (sm == "sm_89")
        return std::make_unique<AdaBackend>();

    // Hopper (sm_90)
    if (sm == "sm_90")
        return std::make_unique<HopperBackend>();

    // Blackwell (sm_100+)
    if (sm == "sm_100" || sm == "sm_101" || sm == "sm_120")
        return std::make_unique<BlackwellBackend>();

    // x86_64
    if (sm == "x86_64" || sm == "cpu" || sm == "broadwell")
        return std::make_unique<X86Backend>();
    if (sm == "zen3" || sm == "zen4" || sm == "zen5" || sm == "amd64" || sm == "amd")
        return std::make_unique<X86Backend>();  // 共享x86指令集, 调优不同

    // AMD GPU
    if (sm == "gfx1200" || sm == "rdna4" || sm == "rx9060")
        return std::make_unique<AmdRDNA4Backend>();

    return nullptr;
}

std::vector<std::string> DeviceBackend::supported_archs() {
    return {
        "sm_60", "sm_61", "sm_62",     // Pascal: 64-bit, verified
        "sm_70", "sm_72",               // Volta: 128-bit
        "sm_75",                        // Turing: 128-bit
        "sm_80", "sm_86",               // Ampere: 128-bit
        "sm_89",                        // Ada: 128-bit
        "sm_90",                        // Hopper: 128-bit
        "sm_100", "sm_101", "sm_120",   // Blackwell: 128-bit
        "x86_64", "cpu", "broadwell",    // x86 CPU
        "zen3", "zen4", "zen5", "amd64", "amd",  // AMD64
        "gfx1200", "rdna4", "rx9060"               // AMD GPU
    };
}

} // namespace sass
