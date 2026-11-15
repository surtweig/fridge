#include "rom_loader.h"

#include <cstdio>
#include <cstring>
#include <fstream>

std::vector<FRIDGE_WORD> RomLoader_ReadRaw(const char* path, FRIDGE_RAM_ADDR offset)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
    {
        std::fprintf(stderr, "[rom_loader] cannot open '%s'\n", path);
        return {};
    }

    std::streamoff size = f.tellg();
    if (size <= 0)
    {
        std::fprintf(stderr, "[rom_loader] empty or unreadable file '%s'\n", path);
        return {};
    }

    const auto max_size = FRIDGE_RAM_SIZE - offset;
    if (size > (std::streamoff)max_size)
    {
        std::fprintf(stderr, "[rom_loader] '%s' is %lld bytes, max loadable is %u\n",
                     path, (long long)size, (unsigned)max_size);
        return {};
    }

    f.seekg(0, std::ios::beg);
    std::vector<FRIDGE_WORD> buf(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char*>(buf.data()), size))
    {
        std::fprintf(stderr, "[rom_loader] read failed on '%s'\n", path);
        return {};
    }
    return buf;
}

void RomLoader_Install(FRIDGE_CPU& cpu, const std::vector<FRIDGE_WORD>& bytes, FRIDGE_RAM_ADDR offset)
{
    if (offset == 0) std::memset(cpu.ram, 0, sizeof(cpu.ram));
    std::memcpy(cpu.ram + offset, bytes.data(), bytes.size());
    cpu.PC = offset;
}
