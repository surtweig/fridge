#include <iostream>
#include <string>
#include "FridgeAssemblyLanguageCompiler.h"

#define XCME_VERSION_STR "v1.1"

int main(int argc, char* argv[])
{
    const char* usage = "Usage: falc <input file> <output file> [-vhdl | -vhdl-aggregate] [--cpm-header]\n";
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
        bool writeVHDLAggregate = false;
        bool writeCpmHeader = false;
        for (int a = 3; a < argc; a++)
        {
            std::string arg(argv[a]);
            if (arg == "-vhdl")
                writeVHDL = true;
            else if (arg == "-vhdl-aggregate")
                writeVHDLAggregate = true;
            else if (arg == "--cpm-header")
                writeCpmHeader = true;
            else
            {
                std::cout << "Unknown option '" << arg << "'.\n"
                          << usage;
                return 1;
            }
        }
        if (writeVHDL && writeVHDLAggregate)
        {
            std::cout << "Use either -vhdl or -vhdl-aggregate, not both.\n";
            return 1;
        }

        FridgeAssemblyLanguageCompiler falc(dir, fname, outfilename, {}, &std::cout,
                                           writeVHDL, writeVHDLAggregate, writeCpmHeader);
        return falc.succeeded() ? 0 : 1;
    }

    std::cout << usage;
    return 2;
}
