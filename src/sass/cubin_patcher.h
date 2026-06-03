/* HunTian SASS — 纯 C++ cubin 注入器 (零外部依赖)
 *
 * 从 ptxas 生成的模板 cubin 中:
 *   1. 用 ELF64 解析找到 .text 段
 *   2. 替换 SASS 指令
 *   3. 输出 GPU 可加载的 cubin
 */

#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <cstdio>

namespace sass::cubin {

// ═══ ELF64 解析器 (偏移读取, 无结构体对齐问题) ═══
namespace elf {
    inline uint16_t read16(const uint8_t* p, size_t off) { return *(uint16_t*)(p+off); }
    inline uint32_t read32(const uint8_t* p, size_t off) { return *(uint32_t*)(p+off); }
    inline uint64_t read64(const uint8_t* p, size_t off) { return *(uint64_t*)(p+off); }

    // Section header: 64 bytes at given offset
    struct SectionInfo {
        uint32_t name_idx;
        uint32_t type;
        uint64_t offset;
        uint64_t size;
    };
    inline SectionInfo read_section(const uint8_t* data, size_t shoff, int idx) {
        size_t o = shoff + idx * 64;
        return {read32(data,o), read32(data,o+4), read64(data,o+24), read64(data,o+32)};
    }
}

// ═══ cubin 注入器 ═══
class CubinPatcher {
    std::vector<uint8_t> data_;
    uint64_t text_offset_ = 0;
    uint64_t text_size_ = 0;
    bool valid_ = false;

public:
    // 从 ptxas 生成的 cubin 创建模板
    bool load_template(const char* path) {
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        data_.resize(ftell(f));
        fseek(f, 0, SEEK_SET);
        fread(data_.data(), 1, data_.size(), f);
        fclose(f);

        auto* d = data_.data();
        if (data_.size() < 64) return false;
        if (memcmp(d, "\x7f" "ELF", 4) != 0) return false;

        // ELF64 offset-based parsing (no struct casting)
        uint16_t shnum   = elf::read16(d, 60);
        uint64_t shoff   = elf::read64(d, 40);
        uint16_t shstrndx = elf::read16(d, 62);

        // Read .shstrtab section to get string table
        auto shstr = elf::read_section(d, shoff, shstrndx);
        const char* strtab = (const char*)(d + shstr.offset);

        // Scan sections for .text
        for (int i = 0; i < shnum; i++) {
            auto sec = elf::read_section(d, shoff, i);
            const char* name = strtab + sec.name_idx;
            if (strstr(name, ".text")) {
                text_offset_ = sec.offset;
                text_size_ = sec.size;
            }
        }

        valid_ = (text_offset_ > 0 && text_size_ > 0);
        return valid_;
    }

    // 替换 SASS 指令并写入新 cubin
    bool patch_and_save(const char* out_path, const std::vector<uint64_t>& new_sass) {
        if (!valid_) return false;
        size_t bytes_needed = new_sass.size() * 8;
        if (bytes_needed > text_size_) {
            printf("[cubin] SASS too large: need %zu, have %lu\n", bytes_needed, text_size_);
            return false;
        }

        // 写入新的 SASS (跳过控制流 debug 行)
        size_t sass_idx = 0, byte_pos = 0;
        uint8_t* text = data_.data() + text_offset_;

        while (sass_idx < new_sass.size() && byte_pos + 8 <= text_size_) {
            uint64_t w = new_sass[sass_idx];
            // 如果是控制流 debug 行 (高16位全是0), 跳过
            // 控制行特征: 高32位是 0x001f 或 0x081f 等
            uint32_t hi = (text[byte_pos+3]<<24)|(text[byte_pos+2]<<16)|(text[byte_pos+1]<<8)|text[byte_pos];
            if ((hi & 0xFF00) == 0x1f00 || (hi & 0xFF00) == 0x0800) {
                byte_pos += 8;  // 跳过控制行
                continue;
            }
            memcpy(text + byte_pos, &w, 8);
            byte_pos += 8;
            sass_idx++;
        }

        // 写文件
        FILE* f = fopen(out_path, "wb");
        if (!f) return false;
        fwrite(data_.data(), 1, data_.size(), f);
        fclose(f);

        printf("[cubin] Patched %zu SASS instructions → %s\n", new_sass.size(), out_path);
        return true;
    }
};

} // namespace sass::cubin
