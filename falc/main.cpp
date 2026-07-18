#include <iostream>
#include <string>
#include "FridgeAssemblyLanguageCompiler.h"

#define XCME_VERSION_STR "v1.1"

int main(int argc, char* argv[])
{
    std::cout << "Fridge Assembly Language Compiler " << XCME_VERSION_STR << "\n"
              << "Copyright (c) Plus&Minus Inc. 2020\n\n";

    if (argc >= 3)
    {
        std::string infilename = argv[1];
        std::string outfilename = argv[2];

        size_t slash = infilename.find_last_of("/\\");
        std::string dir;
        std::string fname;
        if (slash == std::string::npos)
        {
            dir = "./";
            fname = infilename;
        }
        else
        {
            dir = infilename.substr(0, slash + 1);
            fname = infilename.substr(slash + 1);
        }

        std::cout << "Starting with " << fname << " in the root directory " << dir << ".\n";

        bool writeVHDL = false;
        if (argc >= 4)
        {
            std::string arg3(argv[3]);
            if (arg3 == "-vhdl")
                writeVHDL = true;
        }

        FridgeAssemblyLanguageCompiler falc(dir, fname, outfilename, std::vector<std::string>(), &std::cout, writeVHDL);
    }
    else
    {
        std::cout << "Usage: falc <input file> <output file> [-vhdl for additional VHDL output]\n";
    }

    return 0;
}