#pragma once

#include <fridge.h>
#include "fridgemulib.h"

// Owns FRIDGE_SYSTEM and its sub-structs on the heap (matches
// fridge_opengl_emulator/mainwindow.cpp's initFridge/destroyFridge), minus
// PAM16 which is disabled for the v1 emulator.
//
// FRIDGE_cpu_reset() installs input_dev/output_dev callbacks for the ROM
// device (FRIDGE_DEV_ROM_ID) which dereference sys->rom; therefore we
// always allocate an empty FRIDGE_ROM. Programs that try to read from ROM
// without boot-loader setup will hit corePanic (acceptable for v1 raw-bin
// loading).

struct FridgeCtx {
    FRIDGE_SYSTEM system;
    FRIDGE_CPU    cpu;
    FRIDGE_GPU    gpu;
    FRIDGE_KEYBOARD_CONTROLLER kbrd;
    FRIDGE_ROM    rom;
};

void FridgeCtx_Init(FridgeCtx& ctx);
void FridgeCtx_Shutdown(FridgeCtx& ctx);

// Reinitialises CPU, GPU and keyboard to a clean post-reset state.
// Memory is zeroed by FRIDGE_cpu_reset.
void FridgeCtx_Reset(FridgeCtx& ctx);