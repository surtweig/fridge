#include "falcfrontend.h"
#include "FridgeAssemblyLanguageCompiler.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

bool FalcCompile(const char* sourcePath, DebugInfo& out, std::string& errBuf)
{
    std::string path(sourcePath);
    size_t slash = path.find_last_of("/\\");
    std::string dir;
    std::string fname;
    if (slash == std::string::npos)
    {
        dir = "./";
        fname = path;
    }
    else
    {
        dir = path.substr(0, slash + 1);
        fname = path.substr(slash + 1);
    }

    std::vector<std::string> includes;
    includes.push_back("../x2al_std/");

    std::ostringstream logss;
    FridgeAssemblyLanguageCompiler falc(dir, fname, "", includes, &logss, false);

    FRIDGE_WORD* obj = falc.getObjectCode();
    if (!obj)
    {
        errBuf = logss.str();
        return false;
    }

    out.offset = falc.getOffset();
    out.mainEntry = falc.getMainEntry();
    FRIDGE_RAM_ADDR size = falc.getProgramSize();
    out.bytes.assign(obj, obj + size);

    out.aliases = falc.getAliases();

    out.entries = falc.getEntries();
    out.subroutines = falc.getSubroutines();

    const auto& resources = falc.getResources();
    for (const auto& kv : resources)
    {
        StaticSym sym;
        sym.name = kv.first;
        sym.address = kv.second.address;
        sym.size = kv.second.size;
        sym.data.assign(kv.second.pdata, kv.second.pdata + kv.second.size);
        out.statics.push_back(sym);
    }

    const auto& lines = falc.getLines();
    bool hasPreamble = (out.mainEntry != out.offset);

    for (const auto& l : lines)
    {
        SourceLine sl;
        sl.address = l.address;
        if (hasPreamble && sl.address > 0)
            sl.address = (FRIDGE_RAM_ADDR)(sl.address + 3);
        sl.rawText = l.rawText;
        sl.sourceFile = l.sourceFile;
        sl.lineNumber = l.lineNumber;

        out.sourceLines.push_back(sl);

        if (sl.address > 0)
            out.addrToLine.push_back({sl.address, out.sourceLines.size() - 1});
    }

    std::sort(out.addrToLine.begin(), out.addrToLine.end());
    out.addrToLine.erase(
        std::unique(out.addrToLine.begin(), out.addrToLine.end(),
                    [](const auto& a, const auto& b) { return a.first == b.first; }),
        out.addrToLine.end());

    if (hasPreamble)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "JMP 0x%04X", out.mainEntry);
        SourceLine pre;
        pre.address = out.offset;
        pre.rawText = buf;
        pre.sourceFile = "<preamble>";
        pre.lineNumber = 0;
        out.sourceLines.insert(out.sourceLines.begin(), pre);

        for (auto& p : out.addrToLine)
            p.second++;

        out.addrToLine.push_back({pre.address, 0});
        std::sort(out.addrToLine.begin(), out.addrToLine.end());
    }

    errBuf = logss.str();
    return true;
}

const SourceLine* DebugInfo::FindLineForPC(FRIDGE_RAM_ADDR pc) const
{
    size_t idx = FindLineIndexForPC(pc);
    if (idx == (size_t)-1)
        return nullptr;
    return &sourceLines[idx];
}

size_t DebugInfo::FindLineIndexForPC(FRIDGE_RAM_ADDR pc) const
{
    if (addrToLine.empty())
        return (size_t)-1;

    auto it = std::lower_bound(addrToLine.begin(), addrToLine.end(), pc,
        [](const std::pair<FRIDGE_RAM_ADDR, size_t>& p, FRIDGE_RAM_ADDR val) {
            return p.first < val;
        });

    if (it == addrToLine.end())
    {
        --it;
        return it->second;
    }

    if (it->first == pc)
        return it->second;

    if (it != addrToLine.begin())
    {
        --it;
        return it->second;
    }

    return addrToLine.begin()->second;
}
