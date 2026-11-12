#include "stdafx.h"
#include <charconv>
#include <filesystem>
#include <iostream>
#include <string_view>
#include "CPMCompiler.h"
#include "FridgeAssemblyLanguageCompiler.h"

#define XCME_VERSION_STR "v0.1"

static int printUsage()
{
    std::cerr << "Usage: cpm (--check | --asm | --bin) source.cpm [--include directory]...\n"
                 "           [--offset address] [--main-return none|bootloader|halt]\n";
    return 2;
}

static bool parseOffset(std::string_view text, FRIDGE_RAM_ADDR& offset)
{
    int base = 10;
    if (text.substr(0, 2) == "0x" || text.substr(0, 2) == "0X")
    {
        text.remove_prefix(2);
        base = 16;
    }
    unsigned int address;
    const auto end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, address, base);
    if (parsed.ec != std::errc{} || parsed.ptr != end || address > FRIDGE_MAX_DWORD) return false;
    offset = static_cast<FRIDGE_RAM_ADDR>(address);
    return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
    using namespace CPM;
    namespace fs = std::filesystem;
    if (argc < 3) return printUsage();
    const fs::path option(argv[1]);
    if (option != "--check" && option != "--asm" && option != "--bin") return printUsage();

    vector<string> includeFolders;
    CPMCompileOptions options;
    for (int i = 3; i < argc; i += 2)
    {
        if (i + 1 >= argc) return printUsage();
        const fs::path flag(argv[i]);
        const fs::path value(argv[i + 1]);
        if (flag == "--include" || flag == "-I")
            includeFolders.push_back((value / "").string());
        else if (flag == "--offset")
        {
            if (!parseOffset(value.string(), options.offset))
            {
                std::cerr << "--offset requires a decimal or hexadecimal address from 0 to 0xFFFF.\n";
                return 2;
            }
        }
        else if (flag == "--main-return")
        {
            if (value == "none") options.mainReturn = CPMMainReturn::None;
            else if (value == "bootloader") options.mainReturn = CPMMainReturn::Bootloader;
            else if (value == "halt") options.mainReturn = CPMMainReturn::Halt;
            else
            {
                std::cerr << "--main-return requires none, bootloader, or halt.\n";
                return 2;
            }
        }
        else return printUsage();
    }

    const bool checkOnly = option == "--check";
    if (checkOnly)
        std::cout << "Fridge CPM compiler " << XCME_VERSION_STR << "\n"
                  << "Copyright (c) Plus&Minus Inc. 2026\n\n";

    const fs::path source(argv[2]);
    const auto directory = source.parent_path();
    const auto root = directory.empty() ? string{} : (directory / "").string();
    CPMCompiler compiler(root, source.filename().string(), "", includeFolders,
                         checkOnly ? CPMCompileMode::Check : CPMCompileMode::Assembly, options);
    if (checkOnly)
    {
        std::cout << compiler.CompilerLog()->GetText();
        return compiler.NoErrors() ? 0 : 1;
    }
    if (!compiler.NoErrors())
    {
        std::cerr << compiler.CompilerLog()->GetText();
        return 1;
    }
    if (option == "--asm")
    {
        std::cout << compiler.AssemblyOutput();
        return 0;
    }

    auto output = source;
    output.replace_extension(".bin");
    if (output == source)
    {
        std::cerr << "Binary output would overwrite the source file.\n";
        return 1;
    }
    auto assemblyName = source.filename();
    assemblyName.replace_extension(".falc");
    std::istringstream assembly(compiler.AssemblyOutput());
    std::ostringstream log;
    FridgeAssemblyLanguageCompiler assembler(assembly, root, assemblyName.string(), output.string(), &log);
    if (!assembler.succeeded())
    {
        std::cerr << log.str();
        return 1;
    }
    std::cout << "Compiled program is saved to " << output.string() << '\n';
    return 0;
}
