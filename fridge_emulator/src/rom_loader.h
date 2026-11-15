#pragma once

#include <vector>

#include "fridgemulib.h"

// Returns an empty vector and reports an error if the file is unreadable,
// empty, or too large for RAM at the given offset.
std::vector<FRIDGE_WORD> RomLoader_ReadRaw(const char* path, FRIDGE_RAM_ADDR offset);

// Install the bytes returned by RomLoader_ReadRaw at the same offset and
// start execution there. An image at offset zero replaces all of RAM.
void RomLoader_Install(FRIDGE_CPU& cpu, const std::vector<FRIDGE_WORD>& bytes, FRIDGE_RAM_ADDR offset);
