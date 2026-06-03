/* ============================================================================
 * HunTian SASS 汇编器 — 编码器接口
 *
 * 抽象编码后端，支持 HunTian/Pascal/Volta 多后端。
 * ============================================================================ */

#ifndef SASS_ENCODER_INTERFACE_H
#define SASS_ENCODER_INTERFACE_H

#include "sass_types.h"
#include <vector>
#include <unordered_map>
#include <string>

namespace sass {

class IEncoder {
public:
    virtual ~IEncoder() = default;
    virtual std::vector<SASSWord> encode(const ManifoldResult& result) = 0;
    virtual std::vector<SASSWord> encode_with_labels(
        const ManifoldResult& result,
        const std::unordered_map<std::string, uint32_t>& labels) = 0;
};

} // namespace sass

#endif // SASS_ENCODER_INTERFACE_H
