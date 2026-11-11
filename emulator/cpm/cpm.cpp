#include "stdafx.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include "CPMCompiler.h"

#define XCME_VERSION_STR "v0.1"

int _tmain(int argc, _TCHAR* argv[])
{
    std::cout << "Fridge CPM compiler " << XCME_VERSION_STR << "\n"
              << "Copyright (c) Plus&Minus Inc. 2026\n\n";

    if (argc == 3 && std::filesystem::path(argv[1]) == "--check")
    {
        const std::filesystem::path source(argv[2]);
        const auto directory = source.parent_path();
        const auto root = directory.empty() ? std::string{} : (directory / "").string();
        CPM::CPMCompiler compiler(root, source.filename().string(),
                                 "", {}, CPM::CPMCompileMode::Check);
        std::cout << compiler.CompilerLog()->GetText();
        return compiler.NoErrors() ? 0 : 1;
    }

    //if (argc != 1)
    {
        std::cerr << "Usage: cpm [--check source.cpm]\n";
        return 2;
    }

    /*CPM::CPMCompiler compiler("", "test.cpm", "test.bin", {});
    compiler.PrintStaticData();
    std::ofstream("test.log") << compiler.CompilerLog()->GetText();
    std::ofstream("testasm.log") << compiler.AsmDebugOutput()->GetText();
    if (!compiler.NoErrors())
        std::cerr << compiler.CompilerLog()->GetText();
    return compiler.NoErrors() ? 0 : 1;*/
}
