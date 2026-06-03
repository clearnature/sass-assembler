/* ============================================================================
 * HunTian SASS 汇编器 — CLI 入口 (ht-as)
 * ============================================================================ */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#include "sass/assembler.h"
#include "sass/disassembler.h"
#include "sass/device_backend.h"

void print_help(const char* prog) {
    std::cout << "HunTian SASS Assembler 1.0.0\n\n"
              << "Usage: " << prog << " [options] input.sass\n\n"
              << "Options:\n"
              << "  -o <file>        Output file (default: input.sabin)\n"
              << "  -opt <level>     Optimization 0-3 (default: 3)\n"
              << "  --4320d          Enable 4320D manifold scheduling\n"
              << "  --fuse           Enable VAVX3 512-bit fusion\n"
              << "  -v, --verbose    Verbose output\n"
              << "  -h, --help       Show this help\n";
}

int main(int argc, char* argv[]) {
    std::cout << "HunTian SASS Assembler 1.0.0 | " << __DATE__ << "\n\n";

    if (argc < 2) { print_help(argv[0]); return 1; }

    std::string input_file, output_file;
    sass::AssemblerConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) output_file = argv[++i];
        else if (a == "-opt" && i + 1 < argc) cfg.opt_level = std::stoi(argv[++i]);
        else if (a == "--4320d") cfg.use_4320d = true;
        else if (a == "--fuse") cfg.use_fuse = true;
        else if (a == "-v" || a == "--verbose") cfg.verbose = true;
        else if (a == "-h" || a == "--help") { print_help(argv[0]); return 0; }
        else if (a == "--list-opcodes") {
            auto be = sass::DeviceBackend::create("sm_61");
            std::cout << "Pascal SM61 opcodes:\n";
            for (int i=0;i<120;i++) {
                auto op = static_cast<sass::Opcode>(i);
                if (be->supports(op))
                    std::cout << "  " << be->opcode_name(op) << "\n";
            }
            return 0;
        }
        else if (a == "--list-archs") {
            for (auto& s : sass::DeviceBackend::supported_archs())
                std::cout << s << "\n";
            return 0;
        }
        else input_file = a;
    }

    if (input_file.empty()) {
        std::cerr << "Error: No input file specified.\n";
        return 1;
    }

    // ─── 1. 读取文件 ───
    std::ifstream ifs(input_file);
    if (!ifs) {
        std::cerr << "Error: Cannot open " << input_file << "\n";
        return 1;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string source = ss.str();

    if (cfg.verbose)
        std::cout << "[Read] " << source.size() << " bytes from " << input_file << "\n";

    // ─── 2-5. 汇编 (Lexer → Parser → Scheduler → Encoder) ───
    sass::Assembler assembler;
    assembler.config = cfg;

    auto code = assembler.assemble(source);
    if (code.empty() && !source.empty()) {
        std::cerr << "Error: Assembly produced no output.\n";
        return 1;
    }

    if (cfg.verbose)
        std::cout << "[Asm]  " << code.size() << " SASS words\n";

    // ─── 6. 输出 ───
    std::string out = output_file.empty()
        ? input_file + ".sabin"
        : output_file;

    std::ofstream ofs(out, std::ios::binary);
    if (!ofs) {
        std::cerr << "Error: Cannot write " << out << "\n";
        return 1;
    }
    ofs.write(reinterpret_cast<const char*>(code.data()),
              code.size() * sizeof(sass::SASSWord));

    std::cout << "[Done]  " << code.size() << " SASS words encoded";
    if (cfg.use_fuse && code.size() > 0)
        std::cout << " (fusion enabled)";
    std::cout << "\n       Output: " << out << "\n";

    // ─── 7. 详细输出 ───
    if (cfg.verbose) {
        std::cout << "\n[Diagnostics]:\n";
        auto backend = sass::DeviceBackend::create("sm_61");
        if (backend) {
            for (auto& d : backend->last_diagnostics()) {
                const char* level = (d.level==sass::DiagLevel::ERROR)?"ERROR":
                    (d.level==sass::DiagLevel::WARN)?"WARN":"NOTE";
                std::cout << "  " << level << ": " << d.message << "\n";
            }
        }
        auto disassembly = sass::disassemble_block(code);
        std::cout << "\n[Disassembly] " << code.size() << " words:\n";
        for (size_t i = 0; i < std::min(code.size(), size_t(10)); ++i) {
            auto text = sass::disassemble(code[i]);
            std::cout << "  /*" << std::hex << i*8 << "*/ " << text << "\n";
        }
        if (code.size() > 10)
            std::cout << "  ... and " << (code.size()-10) << " more\n";
    }

    return 0;
}