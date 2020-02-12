#include <tchar.h>
#include <iostream>
#include <string>
#include "FridgeAssemblyLanguageCompiler.h"

#define XCME_VERSION_STR "v1.1"

int _tmain(int argc, _TCHAR* argv[])//char *argv[], char *envp[])
{
    //SetConsoleTitle(XCME_CONSOLE_TITLE);
    std::cout << "Fridge Assembly Language Compiler " << XCME_VERSION_STR << "\n"
              << "Copyright (c) Plus&Minus Inc. 2020\n\n";

    if (argc >= 3)
    {
        string infilename = argv[1];
        string outfilename = argv[2];

        char drive[_MAX_DRIVE];
        char dir[_MAX_DIR];
        char fname[_MAX_FNAME];
        char ext[_MAX_EXT];
        _splitpath(infilename.c_str(), drive, dir, fname, ext);
        std::cout << "Starting with " << fname << ext << " in the root directory " << drive << dir << ".\n";

        bool writeVHDL = false;
        if (argc >= 4)
        {
            string arg3(argv[3]);
            if (arg3 == "-vhdl")
                writeVHDL = true;
        }

        FridgeAssemblyLanguageCompiler falc((string)drive + (string)dir, (string)fname + (string)ext, outfilename, vector<string>(), &std::cout, writeVHDL);
        //std::cout << infilename << " " << outfilename << " " << searchpath;
        //XCM2AssemblyLanguageCompiler x2al("..\\..\\xcmcode\\test.x2al", "..\\..\\xcmcode\\test.bin", &std::cout);
        //x2al.printParsed();
        //x2al.printCompiled();
    }
    else
    {
        std::cout << "Usage: falc.exe <input file> <output file> [-vhdl for additional VHDL output]\n";
    }

    return 0;
}
