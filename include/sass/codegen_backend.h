/* ============================================================================
 * HunTian SASS 汇编器 — 编码后端抽象接口
 *
 * 支持多 GPU 架构后端：
 *   HunTianBackend  — 当前自定义 .sabin 格式 (默认)
 *   PascalBackend   — Pascal GP106 真实 SASS 编码 (进行中)
 *   VoltaBackend    — Volta SM7.0 (未来)
 * ============================================================================ */

#ifndef SASS_CODEGEN_BACKEND_H
#define SASS_CODEGEN_BACKEND_H

#include "sass_types.h"
#include <vector>
#include <string>

namespace sass {

class ICodegenBackend {
public:
    virtual ~ICodegenBackend() = default;
    virtual std::vector<SASSWord> encode(const ManifoldResult& result) = 0;
    virtual std::string name() const = 0;
};

} // namespace sass

#endif // SASS_CODEGEN_BACKEND_H
