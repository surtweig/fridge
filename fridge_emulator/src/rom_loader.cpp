#include "rom_loader.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

bool RomLoader_LoadRaw(FRIDGE_CPU* cpu, const char* path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
    {
        std::fprintf(stderr, "[rom_loader] cannot open '%s'\n", path);
        return false;
    }

    std::streamoff size = f.tellg();
    if (size <= 0)
    {
        std::fprintf(stderr, "[rom_loader] empty or unreadable file '%s'\n", path);
        return false;
    }

    const FRIDGE_RAM_ADDR max_size = FRIDGE_RAM_SIZE - FRIDGE_EXECUTABLE_OFFSET;
    if (size > (std::streamoff)max_size)
    {
        std::fprintf(stderr, "[rom_loader] '%s' is %lld bytes, max loadable is %u\n",
                     path, (long long)size, (unsigned)max_size);
        return false;
    }

    f.seekg(0, std::ios::beg);
    std::vector<unsigned char> buf((size_t)size);
    if (!f.read(reinterpret_cast<char*>(buf.data()), size))
    {
        std::fprintf(stderr, "[rom_loader] read failed on '%s'\n", path);
        return false;
    }

    std::memcpy(cpu->ram + FRIDGE_EXECUTABLE_OFFSET, buf.data(), (size_t)size);
    cpu->PC = FRIDGE_EXECUTABLE_OFFSET;
    return true;
}