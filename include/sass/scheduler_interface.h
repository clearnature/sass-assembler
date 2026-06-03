/* ============================================================================
 * HunTian SASS 汇编器 — 调度器接口
 *
 * 抽象调度器，支持依赖注入和 Mock 测试。
 * ============================================================================ */

#ifndef SASS_SCHEDULER_INTERFACE_H
#define SASS_SCHEDULER_INTERFACE_H

#include "sass_types.h"
#include <span>

namespace sass {

class IScheduler {
public:
    virtual ~IScheduler() = default;
    virtual ManifoldResult optimize(const std::span<const Instruction>& block) = 0;
};

} // namespace sass

#endif // SASS_SCHEDULER_INTERFACE_H
