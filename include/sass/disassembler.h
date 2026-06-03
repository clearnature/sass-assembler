/* ============================================================================
 * HunTian SASS 汇编器 — 反汇编器接口
 *
 * 将 SASSWord 二进制编码解码为 .sass 可读文本。
 * ============================================================================ */

#ifndef SASS_DISASSEMBLER_H
#define SASS_DISASSEMBLER_H

#include "sass_types.h"
#include <vector>
#include <string>

namespace sass {

std::string disassemble(SASSWord w);
std::vector<std::string> disassemble_block(const std::vector<SASSWord>& code);

} // namespace sass

#endif // SASS_DISASSEMBLER_H
