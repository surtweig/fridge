#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <fridge.h>

struct SourceLine
{
    FRIDGE_RAM_ADDR address;
    std::string rawText;
    std::string sourceFile;
    int lineNumber;
};

struct StaticSym
{
    std::string name;
    FRIDGE_RAM_ADDR address;
    FRIDGE_DWORD size;
    std::vector<unsigned char> data;
};

struct DebugInfo
{
    std::vector<unsigned char> bytes;
    FRIDGE_RAM_ADDR offset;
    FRIDGE_RAM_ADDR mainEntry;
    std::map<std::string, std::string> aliases;
    std::map<std::string, FRIDGE_RAM_ADDR> entries;
    std::map<std::string, FRIDGE_RAM_ADDR> subroutines;
    std::vector<StaticSym> statics;
    std::vector<SourceLine> sourceLines;
    std::vector<std::pair<FRIDGE_RAM_ADDR, size_t>> addrToLine;

    const SourceLine* FindLineForPC(FRIDGE_RAM_ADDR pc) const;
    size_t FindLineIndexForPC(FRIDGE_RAM_ADDR pc) const;
};

bool FalcCompile(const char* sourcePath, DebugInfo& out, std::string& err);
