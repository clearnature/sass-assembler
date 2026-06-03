/* ============================================================================
 * HunTian SASS 反汇编器 — CLI 入口 (ht-dis)
 * ============================================================================ */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>

#include "sass/disassembler.h"

void print_help(const char* prog) {
    std::cout << "HunTian SASS Disassembler 1.0.0\n\n"
              << "Usage: " << prog << " [options] input.sabin\n\n"
              << "Options:\n"
              << "  -o <file>        Output file (default: stdout)\n"
              << "  --comments       Show address comments\n"
              << "  -h, --help       Show this help\n";
}

int main(int argc, char* argv[]) {
    std::cout << "HunTian SASS Disassembler 1.0.0 | " << __DATE__ << "\n\n";

    if (argc < 2) { print_help(argv[0]); return 1; }

    std::string input_file, output_file;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) output_file = argv[++i];
        else if (a == "--comments") { /* reserved */ }
        else if (a == "-h" || a == "--help") { print_help(argv[0]); return 0; }
        else input_file = a;
    }

    if (input_file.empty()) {
        std::cerr << "Error: No input file specified.\n";
        return 1;
    }

    // ─── 读取二进制文件 ───
    std::ifstream ifs(input_file, std::ios::binary | std::ios::ate);
    if (!ifs) {
        std::cerr << "Error: Cannot open " << input_file << "\n";
        return 1;
    }

    std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    if (size % sizeof(sass::SASSWord) != 0) {
        std::cerr << "Error: File size " << size << " is not aligned to "
                  << sizeof(sass::SASSWord) << " bytes\n";
        return 1;
    }

    size_t count = size / sizeof(sass::SASSWord);
    std::vector<sass::SASSWord> code(count);
    if (!ifs.read(reinterpret_cast<char*>(code.data()), size)) {
        std::cerr << "Error: Failed to read " << size << " bytes\n";
        return 1;
    }

    std::cout << "[Read] " << count << " SASS words (" << size << " bytes)\n\n";

    // ─── 反汇编 ───
    auto lines = sass::disassemble_block(code);

    // ─── 输出 ───
    auto write = [&](std::ostream& os) {
        os << ".func disassembled\n";
        for (const auto& line : lines)
            os << "    " << line << "\n";
        os << ".endfunc\n";
    };

    if (output_file.empty()) {
        write(std::cout);
    } else {
        std::ofstream ofs(output_file);
        if (!ofs) {
            std::cerr << "Error: Cannot write " << output_file << "\n";
            return 1;
        }
        write(ofs);
        std::cout << "[Done] Written to " << output_file << "\n";
    }

    return 0;
}