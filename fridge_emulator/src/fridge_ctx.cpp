#include "fridge_ctx.h"

#include <new>
#include <cstring>

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

    FRIDGE_cpu_reset(&ctx.cpu);
    FRIDGE_gpu_reset(&ctx.gpu);
}

void FridgeCtx_Reset(FridgeCtx& ctx)
{
    FRIDGE_cpu_reset(&ctx.cpu);
    FRIDGE_gpu_reset(&ctx.gpu);

    ctx.kbrd.input_index = 0;
    ctx.kbrd.output_index = 0;
    std::memset(ctx.kbrd.key_buffer, 0, sizeof(ctx.kbrd.key_buffer));

    ctx.rom.state = FRIDGE_ROM_SELECT_MODE;
    ctx.rom.mode = FRIDGE_ROM_MODE_IDLE;
    ctx.rom.active_segment = 0;
    ctx.rom.stream_position = 0;
}

void FridgeCtx_Shutdown(FridgeCtx&)
{
}