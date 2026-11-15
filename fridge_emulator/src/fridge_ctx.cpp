#include "fridge_ctx.h"

#include <new>
#include <cstring>

static void ResetCpu(FRIDGE_CPU& cpu)
{
    FRIDGE_cpu_reset(&cpu);
    // Supply default IRQ handlers for programs loaded without a bootloader.
    const FRIDGE_WORD timer[] = {EI, RET, NOP};
    std::memcpy(cpu.ram + FRIDGE_IRQ_SYS_TIMER, timer, sizeof(timer));

    constexpr FRIDGE_RAM_ADDR keyboard_handler = FRIDGE_IRQ_KEYBOARD + 3;
    const FRIDGE_WORD keyboard[] = {
        JMP, FRIDGE_HIGH_WORD(keyboard_handler), FRIDGE_LOW_WORD(keyboard_handler),
        PUSH_AF, IIN, FRIDGE_DEV_KEYBOARD_ID, POP_AF, EI, RET
    };
    std::memcpy(cpu.ram + FRIDGE_IRQ_KEYBOARD, keyboard, sizeof(keyboard));
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
