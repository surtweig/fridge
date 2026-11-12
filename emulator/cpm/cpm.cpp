#include "stdafx.h"
#include <filesystem>
#include <iostream>
#include "CPMCompiler.h"

#define XCME_VERSION_STR "v0.1"

int _tmain(int argc, _TCHAR* argv[])
{
    using namespace CPM;
    const auto option = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path();
    if (argc != 3 || (option != "--check" && option != "--asm"))
    {
        std::cerr << "Usage: cpm (--check | --asm) source.cpm\n";
        return 2;
    }

    const bool assembly = option == "--asm";
    if (!assembly)
        std::cout << "Fridge CPM compiler " << XCME_VERSION_STR << "\n"
                  << "Copyright (c) Plus&Minus Inc. 2026\n\n";

    const std::filesystem::path source(argv[2]);
    const auto directory = source.parent_path();
    const auto root = directory.empty() ? std::string{} : (directory / "").string();
    CPMCompiler compiler(root, source.filename().string(), "", {},
                         assembly ? CPMCompileMode::Assembly : CPMCompileMode::Check);
    if (!assembly)
        std::cout << compiler.CompilerLog()->GetText();
    else if (compiler.NoErrors())
        std::cout << compiler.AssemblyOutput();
    else
        std::cerr << compiler.CompilerLog()->GetText();
    return compiler.NoErrors() ? 0 : 1;
}
