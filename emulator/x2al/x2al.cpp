#include "stdafx.h"
#include <iostream>
#include <string>
#include "XCM2AssemblyLanguageCompiler.h"
#include <windows.h>
#include "Shlwapi.h"

#define XCME_VERSION_STR "v1.0"
#define XCME_CONSOLE_TITLE L"XCM2 Assembly Language Compiler"

int _tmain(int argc, _TCHAR* argv[])//char *argv[], char *envp[])
{
    SetConsoleTitle(XCME_CONSOLE_TITLE);
    std::cout << "XCM2 Assembly Language Compiler " << XCME_VERSION_STR << "\n"
        << "Copyright (c) Plus&Minus Inc. 2008\n\n";

    if (argc >= 3)
    {
        wstring winfilename(argv[1]);
        wstring woutfilename = argv[2];
        //PathRemoveFileSpec(argv[1]);       
        //wstring wsearchpath(argv[1]);

        string infilename(winfilename.begin(), winfilename.end());
        string outfilename(woutfilename.begin(), woutfilename.end());
        //string searchpath(wsearchpath.begin(), wsearchpath.end());

        char drive[_MAX_DRIVE];
        char dir[_MAX_DIR];
        char fname[_MAX_FNAME];
        char ext[_MAX_EXT];
        _splitpath(infilename.c_str(), drive, dir, fname, ext);
        std::cout << "Starting with " << fname << ext << " in the root directory " << drive << dir << ".\n";

        bool writeVHDL = false;
        if (argc >= 4)
        {
            wstring warg3(argv[3]);
            string arg3(warg3.begin(), warg3.end());
            if (arg3 == "-vhdl")
                writeVHDL = true;
        }

        coutStreamWrapper swcout(&std::cout);
        XCM2AssemblyLanguageCompiler x2al((string)drive + (string)dir, (string)fname + (string)ext, outfilename, vector<string>(), &swcout, writeVHDL);
        //std::cout << infilename << " " << outfilename << " " << searchpath;
        //XCM2AssemblyLanguageCompiler x2al("..\\..\\xcmcode\\test.x2al", "..\\..\\xcmcode\\test.bin", &std::cout);
        //x2al.printParsed();
        //x2al.printCompiled();
    }
    else
    {
        std::cout << "Usage: x2al.exe <input file> <output file> [-vhdl for additional VHDL output]\n";
    }

    return 0;
}

