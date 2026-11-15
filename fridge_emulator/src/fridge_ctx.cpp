#include "fridge_ctx.h"
#include "dummy_bootloader.h"

#include <cstring>

static void ResetCpu(FRIDGE_CPU& cpu)
{
    FRIDGE_cpu_reset(&cpu);
    static_assert(sizeof(dummy_bootloader) <= FRIDGE_EXECUTABLE_OFFSET);
    std::memcpy(cpu.ram, dummy_bootloader, sizeof(dummy_bootloader));
}

void FridgeCtx_Init(FridgeCtx& ctx)
{
    std::memset(&ctx, 0, sizeof(ctx));

    ctx.system.cpu  = &ctx.cpu;
    ctx.system.gpu  = &ctx.gpu;
    ctx.system.rom  = &ctx.rom;
    ctx.system.kbrd = &ctx.kbrd;

    ctx.rom.segments = nullptr;
    ctx.rom.segments_count = 0;
    ctx.rom.state = FRIDGE_ROM_SELECT_MODE;
    ctx.rom.mode = FRIDGE_ROM_MODE_IDLE;
    ctx.rom.active_segment = 0;
    ctx.rom.stream_position = 0;

    ResetCpu(ctx.cpu);
    FRIDGE_gpu_reset(&ctx.gpu);
}

void FridgeCtx_Reset(FridgeCtx& ctx)
{
    ResetCpu(ctx.cpu);
    FRIDGE_gpu_reset(&ctx.gpu);

    ctx.kbrd = {};

    ctx.rom.state = FRIDGE_ROM_SELECT_MODE;
    ctx.rom.mode = FRIDGE_ROM_MODE_IDLE;
    ctx.rom.active_segment = 0;
    ctx.rom.stream_position = 0;
}

void FridgeCtx_Shutdown(FridgeCtx&)
{
}
