#pragma once

#include "fridgemulib.h"

// Phase B v1 ROM loading: read a raw .bin and copy its bytes into
// cpu->ram starting at FRIDGE_EXECUTABLE_OFFSET (0x0200). After loading,
// cpu->PC is set to FRIDGE_EXECUTABLE_OFFSET so execution begins there.
//
// Sizes > (FRIDGE_RAM_SIZE - FRIDGE_EXECUTABLE_OFFSET) are rejected.
// No SD-card / FRIDGE_ROM streaming protocol is emulated at this stage.

// Returns true on success. On failure, prints a message to stderr and
// returns false (cpu untouched).
bool RomLoader_LoadRaw(FRIDGE_CPU* cpu, const char* path);