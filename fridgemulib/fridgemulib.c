#include "fridgemulib.h"


void setPanicFlag(FRIDGE_CPU* cpu, FRIDGE_WORD flag)
{
    if (flag)
        cpu->rF |= FRIDGE_FLAG_PANIC_MASK;
    else
        cpu->rF &= ~FRIDGE_FLAG_PANIC_MASK;
}

void corePanic(FRIDGE_CPU* cpu)
{
    setPanicFlag(cpu, 1);
    cpu->state = FRIDGE_CPU_HALTED;
}

void stackPush(FRIDGE_CPU* cpu, FRIDGE_WORD hi, FRIDGE_WORD lo)
{
#ifdef FRIDGE_ASCENDING_STACK
    cpu->ram[cpu->SP] = hi;
    cpu->ram[cpu->SP+1] = lo;
    cpu->SP += 2;
#else
    cpu->ram[cpu->SP-1] = lo;
    cpu->ram[cpu->SP-2] = hi;
    cpu->SP -= 2;
#endif
    if (cpu->SP < FRIDGE_EXECUTABLE_OFFSET)
        corePanic(cpu);
}

void stackPushD(FRIDGE_CPU* cpu, FRIDGE_DWORD v)
{
    stackPush(cpu, FRIDGE_HIGH_WORD(v), FRIDGE_LOW_WORD(v));
}

void stackPop(FRIDGE_CPU* cpu, FRIDGE_WORD* hi, FRIDGE_WORD* lo)
{
#ifdef FRIDGE_ASCENDING_STACK
    *hi = cpu->ram[cpu->SP-2];
    *lo = cpu->ram[cpu->SP-1];
    cpu->SP -= 2;
#else
    *hi = cpu->ram[cpu->SP];
    *lo = cpu->ram[cpu->SP+1];
    cpu->SP += 2;
#endif
    if (cpu->SP < FRIDGE_EXECUTABLE_OFFSET)
        corePanic(cpu);
}

void stackPopD(FRIDGE_CPU* cpu, FRIDGE_DWORD* v)
{
    FRIDGE_WORD hi;
    FRIDGE_WORD lo;
    stackPop(cpu, &hi, &lo);
    *v = FRIDGE_DWORD_HL(hi, lo);
}

void defaultSetFlags(FRIDGE_CPU* cpu, FRIDGE_WORD result)
{
    cpu->rF &= ~(FRIDGE_FLAG_SIGN_MASK | FRIDGE_FLAG_ZERO_MASK | FRIDGE_FLAG_PARITY_MASK);
    if (result & 1 << (FRIDGE_WORD_BITS-1))
        cpu->rF |= FRIDGE_FLAG_SIGN_MASK;
    if (result == 0)
        cpu->rF |= FRIDGE_FLAG_ZERO_MASK;
    if (result % 2 == 0) // TODO doesn't meet i8080 specification
        cpu->rF |= FRIDGE_FLAG_PARITY_MASK;
}

void compareSetFlags(FRIDGE_CPU* cpu, FRIDGE_WORD a, FRIDGE_WORD b)
{
    cpu->rF &= ~(FRIDGE_FLAG_SIGN_MASK | FRIDGE_FLAG_ZERO_MASK | FRIDGE_FLAG_PARITY_MASK | FRIDGE_FLAG_CARRY_MASK);
    if (a < b)
        cpu->rF |= FRIDGE_FLAG_SIGN_MASK;
    else if (a > b)
        cpu->rF |= FRIDGE_FLAG_CARRY_MASK;
    else
        cpu->rF |= FRIDGE_FLAG_ZERO_MASK;

    if ((a-b) % 2 == 0)
        cpu->rF |= FRIDGE_FLAG_PARITY_MASK; // TODO doesn't meet i8080 specification
}

void setCarry(FRIDGE_CPU* cpu, FRIDGE_WORD flag)
{
    cpu->rF &= ~FRIDGE_FLAG_CARRY_MASK;
    if (flag != 0)
        cpu->rF |= FRIDGE_FLAG_CARRY_MASK;
}

FRIDGE_WORD pcRead(FRIDGE_CPU* cpu)
{
    if (cpu->state == FRIDGE_CPU_ACTIVE)
    {
        return cpu->ram[cpu->PC++];
    }
    else if (cpu->state == FRIDGE_CPU_HALTED)
    {
        if (cpu->inte == FRIDGE_CPU_INTERRUPTS_READING_ARG0)
        {
            cpu->inte = FRIDGE_CPU_INTERRUPTS_READING_ARG1;
            return cpu->inteArg0;
        }
        else if (cpu->inte == FRIDGE_CPU_INTERRUPTS_READING_ARG1)
        {
            cpu->inte = FRIDGE_CPU_INTERRUPTS_DISABLED; // According to 8080 programmers manual p.59
            return cpu->inteArg1;
        }
    }
    return 0;
}

FRIDGE_DWORD pcReadDouble(FRIDGE_CPU* cpu)
{
    FRIDGE_WORD h = pcRead(cpu);
    FRIDGE_WORD l = pcRead(cpu);
    return FRIDGE_DWORD_HL(h, l);
}

void ir_LHLD(FRIDGE_CPU* cpu)
{
    FRIDGE_RAM_ADDR addr = FRIDGE_DWORD_HL(pcRead(cpu), pcRead(cpu));
    cpu->rH = cpu->ram[addr];
    cpu->rL = cpu->ram[addr+1];
}

void ir_SHLD(FRIDGE_CPU* cpu)
{
    FRIDGE_RAM_ADDR addr = FRIDGE_DWORD_HL(pcRead(cpu), pcRead(cpu));
    cpu->ram[addr] = cpu->rH;
    cpu->ram[addr+1] = cpu->rL;
}

void ir_XCNG(FRIDGE_CPU* cpu)
{
    FRIDGE_WORD t = cpu->rD;
    cpu->rD = cpu->rH;
    cpu->rH = t;

    t = cpu->rE;
    cpu->rE = cpu->rL;
    cpu->rL = t;
}

void ir_ADD(FRIDGE_CPU* cpu, FRIDGE_WORD add)
{
    setCarry(cpu, ~cpu->rA < add);
    cpu->rA += add;
    defaultSetFlags(cpu, cpu->rA);
}

void ir_ADC(FRIDGE_CPU* cpu, FRIDGE_WORD add)
{
    FRIDGE_WORD c = FRIDGE_cpu_flag_CARRY(cpu);
    FRIDGE_SIZE_T t = add + c; // intentionally using superset type
    setCarry(cpu, ~cpu->rA < t);
    cpu->rA += t;
    defaultSetFlags(cpu, cpu->rA);
}

void ir_SUB(FRIDGE_CPU* cpu, FRIDGE_WORD sub)
{
    setCarry(cpu, cpu->rA < sub);
    cpu->rA -= sub;
    defaultSetFlags(cpu, cpu->rA);
}

void ir_SBB(FRIDGE_CPU* cpu, FRIDGE_WORD sub)
{
    FRIDGE_WORD c = FRIDGE_cpu_flag_CARRY(cpu);
    FRIDGE_SIZE_T t = sub + c; // intentionally using superset type
    setCarry(cpu, cpu->rA < t);
    cpu->rA -= t;
    defaultSetFlags(cpu, cpu->rA);
}

void ir_INR(FRIDGE_CPU* cpu, FRIDGE_WORD* w)
{
    setCarry(cpu, ~(*w) == 0);
    (*w) += 1;
    defaultSetFlags(cpu, *w);
}

void ir_DCR(FRIDGE_CPU* cpu, FRIDGE_WORD* w)
{
    setCarry(cpu, (*w) == 0);
    (*w) -= 1;
    defaultSetFlags(cpu, *w);
}

void doubleAdd(FRIDGE_WORD* h, FRIDGE_WORD* l, FRIDGE_DWORD d)
{
    FRIDGE_DWORD t = FRIDGE_DWORD_HL(*h, *l);
    t += d;
    *h = FRIDGE_HIGH_WORD(t);
    *l = FRIDGE_LOW_WORD(t);
}

void ir_ANA(FRIDGE_CPU* cpu, FRIDGE_WORD arg)
{
    setCarry(cpu, 0);
    cpu->rA &= arg;
    defaultSetFlags(cpu, cpu->rA);
}

void ir_ORA(FRIDGE_CPU* cpu, FRIDGE_WORD arg)
{
    setCarry(cpu, 0);
    cpu->rA |= arg;
    defaultSetFlags(cpu, cpu->rA);
}

void ir_XRA(FRIDGE_CPU* cpu, FRIDGE_WORD arg)
{
    setCarry(cpu, 0);
    cpu->rA ^= arg;
    defaultSetFlags(cpu, cpu->rA);
}

void ir_RLC(FRIDGE_CPU* cpu)
{
    setCarry(cpu, (cpu->rA & FRIDGE_HIGHBIT_MASK) > 0);
    cpu->rA = (cpu->rA << 1) | ((cpu->rA & FRIDGE_HIGHBIT_MASK) >> (FRIDGE_WORD_BITS-1));
}

void ir_RRC(FRIDGE_CPU* cpu)
{
    setCarry(cpu, (cpu->rA & FRIDGE_LOWBIT_MASK) > 0);
    cpu->rA = (cpu->rA >> 1) | ((cpu->rA & FRIDGE_LOWBIT_MASK) << (FRIDGE_WORD_BITS-1));
}

void ir_RAL(FRIDGE_CPU* cpu)
{
    FRIDGE_WORD c = (cpu->rA & FRIDGE_HIGHBIT_MASK) > 0;
    cpu->rA = (cpu->rA << 1) | FRIDGE_cpu_flag_CARRY(cpu);
    setCarry(cpu, c);
}

void ir_RAR(FRIDGE_CPU* cpu)
{
    FRIDGE_WORD c = (cpu->rA & FRIDGE_LOWBIT_MASK) > 0;
    cpu->rA = (cpu->rA >> 1) | (FRIDGE_cpu_flag_CARRY(cpu) << (FRIDGE_WORD_BITS-1));
    setCarry(cpu, c);
}

void ir_jump(FRIDGE_CPU* cpu, FRIDGE_WORD condition)
{
    FRIDGE_RAM_ADDR addr = pcReadDouble(cpu);
    if (condition)
        cpu->PC = addr;
}

void ir_call(FRIDGE_CPU* cpu, FRIDGE_WORD condition)
{
    FRIDGE_RAM_ADDR addr = pcReadDouble(cpu);
    if (condition)
    {
        stackPushD(cpu, cpu->PC);
        cpu->PC = addr;
    }
}

void ir_ret(FRIDGE_CPU* cpu, FRIDGE_WORD condition)
{
    if (condition)
    {
        stackPopD(cpu, &cpu->PC);
    }
}

void ir_XTHL(FRIDGE_CPU* cpu)
{
    FRIDGE_WORD th = cpu->rH;
    FRIDGE_WORD tl = cpu->rL;
#ifdef FRIDGE_ASCENDING_STACK
    cpu->rH = cpu->ram[cpu->SP-2];
    cpu->rL = cpu->ram[cpu->SP-1];
    cpu->ram[cpu->SP-2] = th;
    cpu->ram[cpu->SP-1] = tl;
#else
    cpu->rH = cpu->ram[cpu->SP];
    cpu->rL = cpu->ram[cpu->SP+1];
    cpu->ram[cpu->SP] = th;
    cpu->ram[cpu->SP+1] = tl;
#endif
}

void ir_safe_IIN(FRIDGE_SYSTEM* sys)
{
    FRIDGE_CPU* cpu = sys->cpu;
    FRIDGE_WORD dev = pcRead(cpu);
    if (dev < FRIDGE_MAX_IO_DEVICES)
        cpu->rA = cpu->input_dev[dev](sys);
    else
        corePanic(cpu);
}

void ir_safe_IOUT(FRIDGE_SYSTEM* sys)
{
    FRIDGE_CPU* cpu = sys->cpu;
    FRIDGE_WORD dev = pcRead(cpu);
    if (dev < FRIDGE_MAX_IO_DEVICES)
        cpu->output_dev[dev](sys, cpu->rA);
    else
        corePanic(cpu);
}

void ir_VPRE(FRIDGE_SYSTEM* sys)
{
    sys->gpu->frame_hor_offset = sys->cpu->rH % FRIDGE_GPU_FRAME_EGA_WIDTH;
    sys->gpu->frame_ver_offset = sys->cpu->rL % FRIDGE_GPU_FRAME_EGA_HEIGHT;

    if (sys->gpu->vframe == FRIDGE_VIDEO_FRAME_A)
        sys->gpu->vframe = FRIDGE_VIDEO_FRAME_B;
    else
        sys->gpu->vframe = FRIDGE_VIDEO_FRAME_A;
}

void ir_VMODE(FRIDGE_SYSTEM* sys)
{
    FRIDGE_WORD mode = sys->cpu->rA;
    if (mode == 0)
        sys->gpu->vmode = FRIDGE_VIDEO_EGA;
    else if (mode == 1)
        sys->gpu->vmode = FRIDGE_VIDEO_TEXT;
}

void ir_VPAL(FRIDGE_SYSTEM* sys)
{
    FRIDGE_WORD colorPos = 3*sys->cpu->rA;
    if (colorPos+2 < FRIDGE_GPU_PALETTE_SIZE)
    {
        sys->gpu->palette[colorPos] = sys->cpu->rB;
        sys->gpu->palette[colorPos+1] = sys->cpu->rC;
        sys->gpu->palette[colorPos+2] = sys->cpu->rD;
    }
}

void ir_VFSA(FRIDGE_SYSTEM* sys)
{
    FRIDGE_RAM_ADDR addr = FRIDGE_cpu_pair_HL(sys->cpu);
    if (addr < FRIDGE_GPU_FRAME_BUFFER_SIZE)
        FRIDGE_gpu_active_frame(sys->gpu)[addr] = sys->cpu->rA;
    else
        corePanic(sys->cpu);
}

void ir_VFSI(FRIDGE_SYSTEM* sys)
{
    FRIDGE_RAM_ADDR addr = FRIDGE_cpu_pair_HL(sys->cpu);
    if (addr < FRIDGE_GPU_FRAME_BUFFER_SIZE-1)
    {
        FRIDGE_gpu_active_frame(sys->gpu)[addr++] = pcRead(sys->cpu);
        FRIDGE_gpu_active_frame(sys->gpu)[addr++] = pcRead(sys->cpu);
        sys->cpu->rH = FRIDGE_HIGH_WORD(addr);
        sys->cpu->rL = FRIDGE_LOW_WORD(addr);
    }
    else
        corePanic(sys->cpu);
}

void ir_VFSAC(FRIDGE_SYSTEM* sys)
{
    FRIDGE_WORD posX = sys->cpu->rH;
    FRIDGE_WORD posY = sys->cpu->rL;
    FRIDGE_RAM_ADDR addr = (posX + posY*FRIDGE_GPU_FRAME_EGA_WIDTH)/2;
    if (addr < FRIDGE_GPU_FRAME_BUFFER_SIZE)
    {
        FRIDGE_WORD* frame = FRIDGE_gpu_active_frame(sys->gpu);
        if (posX % 2 == 0)
            frame[addr] = FRIDGE_GPU_WORD(
                    FRIDGE_GPU_LEFT_PIXEL(sys->cpu->rA),
                    FRIDGE_GPU_RIGHT_PIXEL(frame[addr]));
        else
            frame[addr] = FRIDGE_GPU_WORD(
                    FRIDGE_GPU_LEFT_PIXEL(frame[addr]),
                    FRIDGE_GPU_RIGHT_PIXEL(sys->cpu->rA));
    }
    else
        corePanic(sys->cpu);
}

void ir_VFLA(FRIDGE_SYSTEM* sys)
{
    FRIDGE_RAM_ADDR addr = FRIDGE_cpu_pair_HL(sys->cpu);
    if (addr < FRIDGE_GPU_FRAME_BUFFER_SIZE)
        sys->cpu->rA = FRIDGE_gpu_active_frame(sys->gpu)[addr];
    else
        corePanic(sys->cpu);
}

void ir_VFLAC(FRIDGE_SYSTEM* sys)
{
    FRIDGE_WORD posX = sys->cpu->rH;
    FRIDGE_WORD posY = sys->cpu->rL;
    FRIDGE_RAM_ADDR addr = (posX + posY*FRIDGE_GPU_FRAME_EGA_WIDTH)/2;
    if (addr < FRIDGE_GPU_FRAME_BUFFER_SIZE)
    {
        FRIDGE_WORD* frame = FRIDGE_gpu_active_frame(sys->gpu);
        if (posX % 2 == 0)
            sys->cpu->rA = FRIDGE_GPU_LEFT_PIXEL(frame[addr]);
        else
            sys->cpu->rA = FRIDGE_GPU_RIGHT_PIXEL(frame[addr]);
    }
    else
        corePanic(sys->cpu);
}

void ir_VS2F(FRIDGE_SYSTEM* sys)
{
    FRIDGE_RAM_ADDR spriteAddr = FRIDGE_cpu_pair_HL(sys->cpu);
    FRIDGE_RAM_ADDR frameAddr = FRIDGE_cpu_pair_BC(sys->cpu);
    if (spriteAddr < FRIDGE_GPU_SPRITE_MEMORY_SIZE && frameAddr < FRIDGE_GPU_FRAME_BUFFER_SIZE)
    {
        FRIDGE_gpu_active_frame(sys->gpu)[frameAddr] = sys->gpu->sprite_mem[spriteAddr];
    }
    else
        corePanic(sys->cpu);
}

void ir_VSSA(FRIDGE_SYSTEM* sys)
{
    FRIDGE_RAM_ADDR addr = FRIDGE_cpu_pair_HL(sys->cpu);
    if (addr < FRIDGE_GPU_SPRITE_MEMORY_SIZE)
        sys->gpu->sprite_mem[addr] = sys->cpu->rA;
    else
        corePanic(sys->cpu);
}

void ir_VSSI(FRIDGE_SYSTEM* sys)
{
    FRIDGE_RAM_ADDR addr = FRIDGE_cpu_pair_HL(sys->cpu);
    if (addr < FRIDGE_GPU_SPRITE_MEMORY_SIZE-1)
    {
        sys->gpu->sprite_mem[addr++] = pcRead(sys->cpu);
        sys->gpu->sprite_mem[addr++] = pcRead(sys->cpu);
        sys->cpu->rH = FRIDGE_HIGH_WORD(addr);
        sys->cpu->rL = FRIDGE_LOW_WORD(addr);
    }
    else
        corePanic(sys->cpu);
}

void ir_VSLA(FRIDGE_SYSTEM* sys)
{
    FRIDGE_RAM_ADDR addr = FRIDGE_cpu_pair_HL(sys->cpu);
    if (addr < FRIDGE_GPU_SPRITE_MEMORY_SIZE)
        sys->cpu->rA = sys->gpu->sprite_mem[addr];
    else
        corePanic(sys->cpu);
}

void ir_VSS(FRIDGE_SYSTEM* sys)
{
    FRIDGE_WORD sid = sys->cpu->rA;
    FRIDGE_WORD width = sys->cpu->rB;
    FRIDGE_WORD height = sys->cpu->rC;
    FRIDGE_RAM_ADDR addr = FRIDGE_cpu_pair_HL(sys->cpu);
    FRIDGE_RAM_ADDR size = width*height;
    FRIDGE_RAM_ADDR endAddr = addr + size/2 + size%2;
    if (sid >= FRIDGE_GPU_MAX_SPRITES || endAddr >= FRIDGE_GPU_SPRITE_MEMORY_SIZE)
        corePanic(sys->cpu);
    else
    {
        sys->gpu->sprite_list[sid].mode = FRIDGE_GPU_SPRITE_INVISIBLE;
        sys->gpu->sprite_list[sid].size_x = width;
        sys->gpu->sprite_list[sid].size_y = height;
        sys->gpu->sprite_list[sid].data = addr;
    }
}

void ir_VSD(FRIDGE_SYSTEM* sys)
{
    FRIDGE_WORD sid = sys->cpu->rA;
    FRIDGE_WORD mode = sys->cpu->rB;
    FRIDGE_WORD position_x = sys->cpu->rH;
    FRIDGE_WORD position_y = sys->cpu->rL;
    if (sid >= FRIDGE_GPU_MAX_SPRITES)
        corePanic(sys->cpu);
    else
    {
        sys->gpu->sprite_list[sid].mode = mode;
        sys->gpu->sprite_list[sid].position_x = position_x;
        sys->gpu->sprite_list[sid].position_y = position_y;
    }
}

FRIDGE_WORD dummyDevInput(FRIDGE_SYSTEM* sys)
{
    return 0;
}

void dummyDevOutput(FRIDGE_SYSTEM* sys, FRIDGE_WORD data)
{

}

FRIDGE_WORD keyboard_controller_dev_input(FRIDGE_SYSTEM* sys)
{
    FRIDGE_KEYBOARD_CONTROLLER* kbrd = sys->kbrd;
    FRIDGE_WORD kdata = kbrd->key_buffer[kbrd->output_index];
    kbrd->key_buffer[kbrd->output_index++] = 0;
    return kdata;
}

void rom_tick(FRIDGE_SYSTEM* sys)
{
    FRIDGE_ROM *rom = sys->rom;
    if (rom)
    {
        if (rom->state == FRIDGE_ROM_OPERATE) {
            FRIDGE_cpu_interrupt(sys, NOP, 0, 0);
            rom->stream_position = 0;
            rom->state = FRIDGE_ROM_STREAMING;
        }
    }
}

void rom_reset_dev_output(FRIDGE_SYSTEM* sys, FRIDGE_WORD data)
{
    if (data > 0)
    {
        FRIDGE_ROM *rom = sys->rom;
        rom->state = FRIDGE_ROM_SELECT_MODE;
        rom->mode = FRIDGE_ROM_MODE_IDLE;
        rom->stream_position = 0;
        rom->active_segment = 0;
    }
}

void rom_dev_output(FRIDGE_SYSTEM* sys, FRIDGE_WORD data)
{
    FRIDGE_ROM *rom = sys->rom;
    if (rom->state == FRIDGE_ROM_SELECT_MODE)
    {
        if (data == FRIDGE_ROM_MODE_LOAD || data == FRIDGE_ROM_MODE_STORE)
        {
            rom->mode = data;
            rom->state = FRIDGE_ROM_SEGHIGH;
        }
        else
        {
            corePanic(sys->cpu);
        }
    }

    else if (rom->state == FRIDGE_ROM_SEGHIGH)
    {
        rom->active_segment = data << FRIDGE_WORD_BITS;
        rom->state = FRIDGE_ROM_SEGLOW;
    }

    else if (rom->state == FRIDGE_ROM_SEGLOW)
    {
        rom->active_segment |= data;
        if (rom->active_segment >= rom->segments_count)
        {
            corePanic(sys->cpu);
            rom->state = FRIDGE_ROM_SELECT_MODE;
        }
        else
            rom->state = FRIDGE_ROM_OPERATE;
    }

    else if (rom->state == FRIDGE_ROM_STREAMING && rom->mode == FRIDGE_ROM_MODE_STORE)
    {
        rom->segments[rom->active_segment].data[rom->stream_position++] = data;
        if (rom->stream_position == FRIDGE_ROM_SEGMENT_SIZE)
        {
            rom->state = FRIDGE_ROM_OPERATE;
        }
    }

    else
    {
        corePanic(sys->cpu);
        rom->state = FRIDGE_ROM_SELECT_MODE;
    }
}

FRIDGE_WORD rom_dev_input(FRIDGE_SYSTEM* sys)
{
    FRIDGE_ROM *rom = sys->rom;
    if (rom->state == FRIDGE_ROM_STREAMING && rom->mode == FRIDGE_ROM_MODE_LOAD)
    {
        FRIDGE_WORD data = rom->segments[rom->active_segment].data[rom->stream_position++];
        if (rom->stream_position == FRIDGE_ROM_SEGMENT_SIZE)
        {
            rom->state = FRIDGE_ROM_OPERATE;
        }
        return data;
    }
    corePanic(sys->cpu);
    return 0;
}

void FRIDGE_cpu_reset (FRIDGE_CPU* cpu)
{
    cpu->state = FRIDGE_CPU_ACTIVE;
    cpu->PC = 0;
#ifdef FRIDGE_ASCENDING_STACK
    cpu->SP = FRIDGE_EXECUTABLE_OFFSET;
#else
    cpu->SP = FRIDGE_RAM_SIZE-1;
#endif
    cpu->rA = 0;
    cpu->rB = 0;
    cpu->rC = 0;
    cpu->rD = 0;
    cpu->rE = 0;
    cpu->rH = 0;
    cpu->rL = 0;
    cpu->rF = 0;
    cpu->inte = FRIDGE_CPU_INTERRUPTS_DISABLED;
    cpu->inteArg0 = 0;
    cpu->inteArg1 = 0;

    for (FRIDGE_SIZE_T i = 0; i < FRIDGE_RAM_SIZE; ++i)
    {
        cpu->ram[i] = 0;
    }

    for (FRIDGE_SIZE_T i = 0; i < FRIDGE_MAX_IO_DEVICES; ++i)
    {
        cpu->input_dev[i] = &dummyDevInput;
        cpu->output_dev[i] = &dummyDevOutput;
    }

    cpu->input_dev[FRIDGE_DEV_KEYBOARD_ID] = &keyboard_controller_dev_input;
    cpu->input_dev[FRIDGE_DEV_ROM_ID] = &rom_dev_input;
    cpu->output_dev[FRIDGE_DEV_ROM_RESET_ID] = &rom_reset_dev_output;
    cpu->output_dev[FRIDGE_DEV_ROM_ID] = &rom_dev_output;
}

void cpu_execute(FRIDGE_SYSTEM* sys, FRIDGE_WORD ircode)
{
    FRIDGE_CPU* cpu = sys->cpu;
    switch (ircode)
    {
        case MOV_AB: cpu->rA = cpu->rB; break;
        case MOV_AC: cpu->rA = cpu->rC; break;
        case MOV_AD: cpu->rA = cpu->rD; break;
        case MOV_AE: cpu->rA = cpu->rE; break;
        case MOV_AH: cpu->rA = cpu->rH; break;
        case MOV_AL: cpu->rA = cpu->rL; break;
        case MOV_BA: cpu->rB = cpu->rA; break;
        case MOV_BC: cpu->rB = cpu->rC; break;
        case MOV_BD: cpu->rB = cpu->rD; break;
        case MOV_BE: cpu->rB = cpu->rE; break;
        case MOV_BH: cpu->rB = cpu->rH; break;
        case MOV_BL: cpu->rB = cpu->rL; break;
        case MOV_CA: cpu->rC = cpu->rA; break;
        case MOV_CB: cpu->rC = cpu->rB; break;
        case MOV_CD: cpu->rC = cpu->rD; break;
        case MOV_CE: cpu->rC = cpu->rE; break;
        case MOV_CH: cpu->rC = cpu->rH; break;
        case MOV_CL: cpu->rC = cpu->rL; break;
        case MOV_DA: cpu->rD = cpu->rA; break;
        case MOV_DB: cpu->rD = cpu->rB; break;
        case MOV_DC: cpu->rD = cpu->rC; break;
        case MOV_DE: cpu->rD = cpu->rE; break;
        case MOV_DH: cpu->rD = cpu->rH; break;
        case MOV_DL: cpu->rD = cpu->rL; break;
        case MOV_EA: cpu->rE = cpu->rA; break;
        case MOV_EB: cpu->rE = cpu->rB; break;
        case MOV_EC: cpu->rE = cpu->rC; break;
        case MOV_ED: cpu->rE = cpu->rD; break;
        case MOV_EH: cpu->rE = cpu->rH; break;
        case MOV_EL: cpu->rE = cpu->rL; break;
        case MOV_HA: cpu->rH = cpu->rA; break;
        case MOV_HB: cpu->rH = cpu->rB; break;
        case MOV_HC: cpu->rH = cpu->rC; break;
        case MOV_HD: cpu->rH = cpu->rD; break;
        case MOV_HE: cpu->rH = cpu->rE; break;
        case MOV_HL: cpu->rH = cpu->rL; break;
        case MOV_LA: cpu->rL = cpu->rA; break;
        case MOV_LB: cpu->rL = cpu->rB; break;
        case MOV_LC: cpu->rL = cpu->rC; break;
        case MOV_LD: cpu->rL = cpu->rD; break;
        case MOV_LE: cpu->rL = cpu->rE; break;
        case MOV_LH: cpu->rL = cpu->rH; break;

        case MOV_AM: cpu->rA = cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]; break;
        case MOV_BM: cpu->rB = cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]; break;
        case MOV_CM: cpu->rC = cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]; break;
        case MOV_DM: cpu->rD = cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]; break;
        case MOV_EM: cpu->rE = cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]; break;
        case MOV_HM: cpu->rH = cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]; break;
        case MOV_LM: cpu->rL = cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]; break;

        case MOV_MA: cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)] = cpu->rA; break;
        case MOV_MB: cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)] = cpu->rB; break;
        case MOV_MC: cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)] = cpu->rC; break;
        case MOV_MD: cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)] = cpu->rD; break;
        case MOV_ME: cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)] = cpu->rE; break;
        case MOV_MH: cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)] = cpu->rH; break;
        case MOV_ML: cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)] = cpu->rL; break;

        case MVI_A: cpu->rA = pcRead(cpu); break;
        case MVI_B: cpu->rB = pcRead(cpu); break;
        case MVI_C: cpu->rC = pcRead(cpu); break;
        case MVI_D: cpu->rD = pcRead(cpu); break;
        case MVI_E: cpu->rE = pcRead(cpu); break;
        case MVI_H: cpu->rH = pcRead(cpu); break;
        case MVI_L: cpu->rL = pcRead(cpu); break;
        case MVI_M: cpu->ram[FRIDGE_cpu_pair_HL(cpu)] = pcRead(cpu); break;

        case LXI_BC: cpu->rB = pcRead(cpu); cpu->rC = pcRead(cpu); break;
        case LXI_DE: cpu->rD = pcRead(cpu); cpu->rE = pcRead(cpu); break;
        case LXI_HL: cpu->rH = pcRead(cpu); cpu->rL = pcRead(cpu); break;
        case LXI_SP: cpu->SP = FRIDGE_DWORD_HL(pcRead(cpu), pcRead(cpu)); break;

        case LDA: cpu->rA = cpu->ram[pcReadDouble(cpu)]; break;
        case STA: cpu->ram[pcReadDouble(cpu)] = cpu->rA; break;

        case LHLD: ir_LHLD(cpu); break;
        case SHLD: ir_SHLD(cpu); break;

        case LDAX_BC: cpu->rA = cpu->ram[FRIDGE_DWORD_HL(cpu->rB, cpu->rC)]; break;
        case LDAX_DE: cpu->rA = cpu->ram[FRIDGE_DWORD_HL(cpu->rD, cpu->rE)]; break;
        case LDAX_HL: cpu->rA = cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]; break;
        case STAX_BC: cpu->ram[FRIDGE_DWORD_HL(cpu->rB, cpu->rC)] = cpu->rA; break;
        case STAX_DE: cpu->ram[FRIDGE_DWORD_HL(cpu->rD, cpu->rE)] = cpu->rA; break;
        case STAX_HL: cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)] = cpu->rA; break;

        case XCNG: ir_XCNG(cpu); break;

        case ADD_A: ir_ADD(cpu, cpu->rA); break;
        case ADD_B: ir_ADD(cpu, cpu->rB); break;
        case ADD_C: ir_ADD(cpu, cpu->rC); break;
        case ADD_D: ir_ADD(cpu, cpu->rD); break;
        case ADD_E: ir_ADD(cpu, cpu->rE); break;
        case ADD_H: ir_ADD(cpu, cpu->rH); break;
        case ADD_L: ir_ADD(cpu, cpu->rL); break;

        case ADD_M: ir_ADD(cpu, cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]); break;
        case ADI: ir_ADD(cpu, pcRead(cpu)); break;

        case ADC_A: ir_ADC(cpu, cpu->rA); break;
        case ADC_B: ir_ADC(cpu, cpu->rB); break;
        case ADC_C: ir_ADC(cpu, cpu->rC); break;
        case ADC_D: ir_ADC(cpu, cpu->rD); break;
        case ADC_E: ir_ADC(cpu, cpu->rE); break;
        case ADC_H: ir_ADC(cpu, cpu->rH); break;
        case ADC_L: ir_ADC(cpu, cpu->rL); break;

        case ADC_M: ir_ADC(cpu, cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]); break;
        case ACI: ir_ADC(cpu, pcRead(cpu)); break;

        case SUB_A: ir_SUB(cpu, cpu->rA); break;
        case SUB_B: ir_SUB(cpu, cpu->rB); break;
        case SUB_C: ir_SUB(cpu, cpu->rC); break;
        case SUB_D: ir_SUB(cpu, cpu->rD); break;
        case SUB_E: ir_SUB(cpu, cpu->rE); break;
        case SUB_H: ir_SUB(cpu, cpu->rH); break;
        case SUB_L: ir_SUB(cpu, cpu->rL); break;

        case SUB_M: ir_SUB(cpu, cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]); break;
        case SUI: ir_SUB(cpu, pcRead(cpu)); break;

        case SBB_A: ir_SBB(cpu, cpu->rA); break;
        case SBB_B: ir_SBB(cpu, cpu->rB); break;
        case SBB_C: ir_SBB(cpu, cpu->rC); break;
        case SBB_D: ir_SBB(cpu, cpu->rD); break;
        case SBB_E: ir_SBB(cpu, cpu->rE); break;
        case SBB_H: ir_SBB(cpu, cpu->rH); break;
        case SBB_L: ir_SBB(cpu, cpu->rL); break;

        case SBB_M: ir_SBB(cpu, cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]); break;
        case SBI: ir_SBB(cpu, pcRead(cpu)); break;

        case INR_A: ir_INR(cpu, &cpu->rA); break;
        case INR_B: ir_INR(cpu, &cpu->rB); break;
        case INR_C: ir_INR(cpu, &cpu->rC); break;
        case INR_D: ir_INR(cpu, &cpu->rD); break;
        case INR_E: ir_INR(cpu, &cpu->rE); break;
        case INR_H: ir_INR(cpu, &cpu->rH); break;
        case INR_L: ir_INR(cpu, &cpu->rL); break;
        case INR_M: ir_INR(cpu, &cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]); break;

        case DCR_A: ir_DCR(cpu, &cpu->rA); break;
        case DCR_B: ir_DCR(cpu, &cpu->rB); break;
        case DCR_C: ir_DCR(cpu, &cpu->rC); break;
        case DCR_D: ir_DCR(cpu, &cpu->rD); break;
        case DCR_E: ir_DCR(cpu, &cpu->rE); break;
        case DCR_H: ir_DCR(cpu, &cpu->rH); break;
        case DCR_L: ir_DCR(cpu, &cpu->rL); break;
        case DCR_M: ir_DCR(cpu, &cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]); break;

        case INX_BC: doubleAdd(&cpu->rB, &cpu->rC, 1); break;
        case INX_DE: doubleAdd(&cpu->rD, &cpu->rE, 1); break;
        case INX_HL: doubleAdd(&cpu->rH, &cpu->rL, 1); break;
        case INX_SP: cpu->SP++; break;

        case DCX_BC: doubleAdd(&cpu->rB, &cpu->rC, -1); break;
        case DCX_DE: doubleAdd(&cpu->rD, &cpu->rE, -1); break;
        case DCX_HL: doubleAdd(&cpu->rH, &cpu->rL, -1); break;
        case DCX_SP: cpu->SP--; break;

        case DAD_BC: doubleAdd(&cpu->rH, &cpu->rL, FRIDGE_cpu_pair_BC(cpu)); break;
        case DAD_DE: doubleAdd(&cpu->rH, &cpu->rL, FRIDGE_cpu_pair_DE(cpu)); break;
        case DAD_HL: doubleAdd(&cpu->rH, &cpu->rL, FRIDGE_cpu_pair_HL(cpu)); break;
        case DAD_SP: doubleAdd(&cpu->rH, &cpu->rL, cpu->SP); break;
        case DAI:    doubleAdd(&cpu->rH, &cpu->rL, pcReadDouble(cpu)); break;

        case ANA_A: ir_ANA(cpu, cpu->rA); break;
        case ANA_B: ir_ANA(cpu, cpu->rB); break;
        case ANA_C: ir_ANA(cpu, cpu->rC); break;
        case ANA_D: ir_ANA(cpu, cpu->rD); break;
        case ANA_E: ir_ANA(cpu, cpu->rE); break;
        case ANA_H: ir_ANA(cpu, cpu->rH); break;
        case ANA_L: ir_ANA(cpu, cpu->rL); break;
        case ANA_M: ir_ANA(cpu, cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]); break;
        case ANI  : ir_ANA(cpu, pcRead(cpu)); break;

        case ORA_A: ir_ORA(cpu, cpu->rA); break;
        case ORA_B: ir_ORA(cpu, cpu->rB); break;
        case ORA_C: ir_ORA(cpu, cpu->rC); break;
        case ORA_D: ir_ORA(cpu, cpu->rD); break;
        case ORA_E: ir_ORA(cpu, cpu->rE); break;
        case ORA_H: ir_ORA(cpu, cpu->rH); break;
        case ORA_L: ir_ORA(cpu, cpu->rL); break;
        case ORA_M: ir_ORA(cpu, cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]); break;
        case ORI  : ir_ORA(cpu, pcRead(cpu)); break;

        case XRA_A: ir_XRA(cpu, cpu->rA); break;
        case XRA_B: ir_XRA(cpu, cpu->rB); break;
        case XRA_C: ir_XRA(cpu, cpu->rC); break;
        case XRA_D: ir_XRA(cpu, cpu->rD); break;
        case XRA_E: ir_XRA(cpu, cpu->rE); break;
        case XRA_H: ir_XRA(cpu, cpu->rH); break;
        case XRA_L: ir_XRA(cpu, cpu->rL); break;
        case XRA_M: ir_XRA(cpu, cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]); break;
        case XRI  : ir_XRA(cpu, pcRead(cpu)); break;

        case CMP_A: compareSetFlags(cpu, cpu->rA, cpu->rA); break;
        case CMP_B: compareSetFlags(cpu, cpu->rA, cpu->rB); break;
        case CMP_C: compareSetFlags(cpu, cpu->rA, cpu->rC); break;
        case CMP_D: compareSetFlags(cpu, cpu->rA, cpu->rD); break;
        case CMP_E: compareSetFlags(cpu, cpu->rA, cpu->rE); break;
        case CMP_H: compareSetFlags(cpu, cpu->rA, cpu->rH); break;
        case CMP_L: compareSetFlags(cpu, cpu->rA, cpu->rL); break;
        case CMP_M: compareSetFlags(cpu, cpu->rA, cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]); break;
        case CPI  : compareSetFlags(cpu, cpu->rA, pcRead(cpu)); break;

        case RLC: ir_RLC(cpu); break;
        case RRC: ir_RRC(cpu); break;
        case RAL: ir_RAL(cpu); break;
        case RAR: ir_RAR(cpu); break;

        case CMA: cpu->rA = ~cpu->rA; break;
        case CMC: setCarry(cpu, !FRIDGE_cpu_flag_CARRY(cpu)); break;
        case STC: setCarry(cpu, 1); break;
        case RTC: setCarry(cpu, 0); break;

        case JMP: ir_jump(cpu, 1); break;
        case JNZ: ir_jump(cpu, !FRIDGE_cpu_flag_ZERO(cpu)); break;
        case JZ : ir_jump(cpu,  FRIDGE_cpu_flag_ZERO(cpu)); break;
        case JNC: ir_jump(cpu, !FRIDGE_cpu_flag_CARRY(cpu)); break;
        case JC : ir_jump(cpu,  FRIDGE_cpu_flag_CARRY(cpu)); break;
        case JPO: ir_jump(cpu, !FRIDGE_cpu_flag_PARITY(cpu)); break;
        case JPE: ir_jump(cpu,  FRIDGE_cpu_flag_PARITY(cpu)); break;
        case JP : ir_jump(cpu, !FRIDGE_cpu_flag_SIGN(cpu)); break;
        case JM : ir_jump(cpu,  FRIDGE_cpu_flag_SIGN(cpu)); break;

        case CALL: ir_call(cpu, 1); break;
        case CNZ: ir_call(cpu, !FRIDGE_cpu_flag_ZERO(cpu)); break;
        case CZ : ir_call(cpu,  FRIDGE_cpu_flag_ZERO(cpu)); break;
        case CNC: ir_call(cpu, !FRIDGE_cpu_flag_CARRY(cpu)); break;
        case CC : ir_call(cpu,  FRIDGE_cpu_flag_CARRY(cpu)); break;
        case CPO: ir_call(cpu, !FRIDGE_cpu_flag_PARITY(cpu)); break;
        case CPE: ir_call(cpu,  FRIDGE_cpu_flag_PARITY(cpu)); break;
        case CP : ir_call(cpu, !FRIDGE_cpu_flag_SIGN(cpu)); break;
        case CM : ir_call(cpu,  FRIDGE_cpu_flag_SIGN(cpu)); break;

        case RET: ir_ret(cpu, 1); break;
        case RNZ: ir_ret(cpu, !FRIDGE_cpu_flag_ZERO(cpu)); break;
        case RZ : ir_ret(cpu,  FRIDGE_cpu_flag_ZERO(cpu)); break;
        case RNC: ir_ret(cpu, !FRIDGE_cpu_flag_CARRY(cpu)); break;
        case RC : ir_ret(cpu,  FRIDGE_cpu_flag_CARRY(cpu)); break;
        case RPO: ir_ret(cpu, !FRIDGE_cpu_flag_PARITY(cpu)); break;
        case RPE: ir_ret(cpu,  FRIDGE_cpu_flag_PARITY(cpu)); break;
        case RP : ir_ret(cpu, !FRIDGE_cpu_flag_SIGN(cpu)); break;
        case RM : ir_ret(cpu,  FRIDGE_cpu_flag_SIGN(cpu)); break;

        case PCHL: cpu->PC = FRIDGE_cpu_pair_HL(cpu); break;

        case PUSH_AF: stackPush(cpu, cpu->rA, cpu->rF); break;
        case PUSH_BC: stackPush(cpu, cpu->rB, cpu->rC); break;
        case PUSH_DE: stackPush(cpu, cpu->rD, cpu->rE); break;
        case PUSH_HL: stackPush(cpu, cpu->rH, cpu->rL); break;
        case POP_AF: stackPop(cpu, &cpu->rA, &cpu->rF); break;
        case POP_BC: stackPop(cpu, &cpu->rB, &cpu->rC); break;
        case POP_DE: stackPop(cpu, &cpu->rD, &cpu->rE); break;
        case POP_HL: stackPop(cpu, &cpu->rH, &cpu->rL); break;

        case XTHL: ir_XTHL(cpu); break;
        case SPHL: cpu->SP = FRIDGE_cpu_pair_HL(cpu); break;
        case HLSP: cpu->rH = FRIDGE_HIGH_WORD(cpu->SP); cpu->rL = FRIDGE_LOW_WORD(cpu->SP); break;

        case IIN:  ir_safe_IIN(sys); break;
        case IOUT: ir_safe_IOUT(sys); break;

        case HLT: cpu->state = FRIDGE_CPU_HALTED; break;
        case EI: cpu->inte = FRIDGE_CPU_INTERRUPTS_ENABLED; break;
        case DI: cpu->inte = FRIDGE_CPU_INTERRUPTS_DISABLED; break;

        case VPRE:  ir_VPRE(sys);  break;
        case VMODE: ir_VMODE(sys); break;
        case VPAL:  ir_VPAL(sys);  break;
        case VFSA:  ir_VFSA(sys);  break;
        case VFSI:  ir_VFSI(sys);  break;
        case VFSAC: ir_VFSAC(sys); break;
        case VFLA:  ir_VFLA(sys);  break;
        case VFLAC: ir_VFLAC(sys); break;
        case VS2F:  ir_VS2F(sys);  break;
        case VSSA:  ir_VSSA(sys);  break;
        case VSSI:  ir_VSSI(sys);  break;
        case VSLA:  ir_VSLA(sys);  break;
        case VSS:   ir_VSS(sys);   break;
        case VSD:   ir_VSD(sys);   break;
    }
}

void FRIDGE_sys_tick (FRIDGE_SYSTEM* sys)
{
    FRIDGE_CPU* cpu = sys->cpu;
    if (cpu->state == FRIDGE_CPU_ACTIVE)
    {
        FRIDGE_WORD ircode = pcRead(cpu);
        cpu_execute(sys, ircode);
        rom_tick(sys);
    }
}

void FRIDGE_cpu_interrupt (FRIDGE_SYSTEM* sys, FRIDGE_WORD ircode, FRIDGE_WORD arg0, FRIDGE_WORD arg1)
{
    FRIDGE_CPU* cpu = sys->cpu;
    if (cpu->inte == FRIDGE_CPU_INTERRUPTS_ENABLED)
    {
        cpu->state = FRIDGE_CPU_HALTED;
        cpu->inteArg0 = arg0;
        cpu->inteArg1 = arg1;
        cpu->inte = FRIDGE_CPU_INTERRUPTS_READING_ARG0;
        cpu_execute(sys, ircode);
        cpu->inte = FRIDGE_CPU_INTERRUPTS_DISABLED; // According to 8080 programmers manual p.59
    }
    cpu->state = FRIDGE_CPU_ACTIVE;
}

void FRIDGE_sys_timer_tick(FRIDGE_SYSTEM* sys)
{
    FRIDGE_cpu_interrupt(sys, CALL, FRIDGE_HIGH_WORD(FRIDGE_IRQ_SYS_TIMER), FRIDGE_LOW_WORD(FRIDGE_IRQ_SYS_TIMER));
}

void FRIDGE_cpu_ram_read (FRIDGE_CPU* cpu, FRIDGE_WORD* buffer, FRIDGE_RAM_ADDR position, FRIDGE_SIZE_T size)
{
    for (FRIDGE_RAM_ADDR i = 0; i < size; ++i)
    {
        cpu->ram[position + i] = buffer[i];
    }
}

void FRIDGE_cpu_ram_write (FRIDGE_CPU* cpu, FRIDGE_WORD* buffer, FRIDGE_RAM_ADDR position, FRIDGE_SIZE_T size)
{
    for (FRIDGE_RAM_ADDR i = 0; i < size; ++i)
    {
        buffer[i] = cpu->ram[position + i];
    }
}

FRIDGE_WORD FRIDGE_cpu_flag_SIGN (const FRIDGE_CPU* cpu)
{
    return (cpu->rF & FRIDGE_FLAG_SIGN_MASK) > 0;
}

FRIDGE_WORD FRIDGE_cpu_flag_ZERO (const FRIDGE_CPU* cpu)
{
    return (cpu->rF & FRIDGE_FLAG_ZERO_MASK) > 0;
}

FRIDGE_WORD FRIDGE_cpu_flag_AUX (const FRIDGE_CPU* cpu)
{
    return (cpu->rF & FRIDGE_FLAG_AUX_MASK) > 0;
}

FRIDGE_WORD FRIDGE_cpu_flag_PANIC (const FRIDGE_CPU* cpu)
{
    return (cpu->rF & FRIDGE_FLAG_PANIC_MASK) > 0;
}

FRIDGE_WORD FRIDGE_cpu_flag_PARITY (const FRIDGE_CPU* cpu)
{
    return (cpu->rF & FRIDGE_FLAG_PARITY_MASK) > 0;
}

FRIDGE_WORD FRIDGE_cpu_flag_CARRY (const FRIDGE_CPU* cpu)
{
    return (cpu->rF & FRIDGE_FLAG_CARRY_MASK) > 0;
}

FRIDGE_DWORD FRIDGE_cpu_pair_BC (const FRIDGE_CPU* cpu)
{
    return FRIDGE_DWORD_HL(cpu->rB, cpu->rC);
}

FRIDGE_DWORD FRIDGE_cpu_pair_DE (const FRIDGE_CPU* cpu)
{
    return FRIDGE_DWORD_HL(cpu->rD, cpu->rE);
}

FRIDGE_DWORD FRIDGE_cpu_pair_HL (const FRIDGE_CPU* cpu)
{
    return FRIDGE_DWORD_HL(cpu->rH, cpu->rL);
}

void FRIDGE_gpu_reset (FRIDGE_GPU* gpu)
{
    gpu->vmode = FRIDGE_VIDEO_TEXT;
    gpu->vframe = FRIDGE_VIDEO_FRAME_A;
    for (int i = 0; i < FRIDGE_GPU_FRAME_BUFFER_SIZE; ++i)
    {
        gpu->frame_a[i] = 0;
        gpu->frame_b[i] = 0;
    }
    for (int i = 0; i < FRIDGE_GPU_SPRITE_MEMORY_SIZE; ++i)
    {
        gpu->sprite_mem[i] = 0;
    }
    for (int i = 0; i < FRIDGE_GPU_MAX_SPRITES; ++i)
    {
        gpu->sprite_list[i].mode = FRIDGE_GPU_SPRITE_INVISIBLE;
    }
    for (int i = 0; i < FRIDGE_GPU_PALETTE_SIZE; ++i)
    {
        gpu->palette[i] = FRIDGE_gpu_default_palette[i];
    }
    gpu->frame_hor_offset = 0;
    gpu->frame_ver_offset = 0;
}

void FRIDGE_gpu_tick(FRIDGE_GPU* gpu)
{

}

FRIDGE_WORD* FRIDGE_gpu_visible_frame(FRIDGE_GPU* gpu)
{
    if (gpu->vframe == FRIDGE_VIDEO_FRAME_A)
        return gpu->frame_b;
    else if (gpu->vframe == FRIDGE_VIDEO_FRAME_B)
        return gpu->frame_a;
    return 0;
}

FRIDGE_WORD* FRIDGE_gpu_active_frame(FRIDGE_GPU* gpu)
{
    if (gpu->vframe == FRIDGE_VIDEO_FRAME_A)
        return gpu->frame_a;
    else if (gpu->vframe == FRIDGE_VIDEO_FRAME_B)
        return gpu->frame_b;
    return 0;
}

FRIDGE_VIDEO_MODE FRIDGE_gpu_vmode(FRIDGE_GPU* gpu)
{
    return gpu->vmode;
}

void FRIDGE_gpu_render_ega_rgb8(FRIDGE_GPU* gpu, unsigned char* pixels)
{
    FRIDGE_gpu_render_ega_rgb8_area(gpu, pixels, 0, 0, FRIDGE_GPU_FRAME_EGA_WIDTH, FRIDGE_GPU_FRAME_EGA_HEIGHT, 0);
}

void FRIDGE_gpu_render_ega_rgb8_area(FRIDGE_GPU* gpu, unsigned char* pixels,
                                     FRIDGE_WORD x, FRIDGE_WORD y, FRIDGE_WORD w, FRIDGE_WORD h, int pixelsRowOffset)
{
    FRIDGE_RAM_ADDR spritesDataSize[FRIDGE_GPU_MAX_SPRITES];
    for (int sid = 0; sid < FRIDGE_GPU_MAX_SPRITES; ++sid)
    {
        FRIDGE_GPU_SPRITE* spr = &gpu->sprite_list[sid];
        spritesDataSize[sid] = spr->size_x*spr->size_y;
        spritesDataSize[sid] = spritesDataSize[sid] % 2 + spritesDataSize[sid] / 2;
    }

    FRIDGE_WORD* frame = FRIDGE_gpu_visible_frame(gpu);
    unsigned int output_addr = 0;

    if (w > FRIDGE_GPU_FRAME_EGA_WIDTH)
        w = FRIDGE_GPU_FRAME_EGA_WIDTH;
    if (h > FRIDGE_GPU_FRAME_EGA_HEIGHT)
        h = FRIDGE_GPU_FRAME_EGA_HEIGHT;

    for (FRIDGE_WORD iy = y; iy < y + h; ++iy)
    {
        for (FRIDGE_WORD ix = x; ix < x + w; ++ix)
        {
            if (ix >= FRIDGE_GPU_FRAME_EGA_WIDTH)
                continue;
            if (iy >= FRIDGE_GPU_FRAME_EGA_HEIGHT)
                continue;

            FRIDGE_WORD px = (ix + gpu->frame_hor_offset) % FRIDGE_GPU_FRAME_EGA_WIDTH;
            FRIDGE_WORD py = (iy + gpu->frame_ver_offset) % FRIDGE_GPU_FRAME_EGA_HEIGHT;

            FRIDGE_RAM_ADDR addr = (px + py*FRIDGE_GPU_FRAME_EGA_WIDTH)/2;
            FRIDGE_WORD color;
            if (px % 2 == 0)
                color = FRIDGE_GPU_LEFT_PIXEL(frame[addr]);
            else
                color = FRIDGE_GPU_RIGHT_PIXEL(frame[addr]);
            FRIDGE_WORD rgb[3] = {gpu->palette[color*3], gpu->palette[color*3+1], gpu->palette[color*3+2]};

            int sprCounter = 0;
            for (int sid = 0; sid < FRIDGE_GPU_MAX_SPRITES; ++sid)
            {
                FRIDGE_GPU_SPRITE* spr = &gpu->sprite_list[sid];
                if (spr->mode == FRIDGE_GPU_SPRITE_INVISIBLE)
                    continue;

                if (sprCounter >= FRIDGE_GPU_MAX_SPRITES_PER_PIXEL)
                    break;

                FRIDGE_WORD sx = px - spr->position_x;
                FRIDGE_WORD sy = py - spr->position_y;

                if (sx < spr->size_x && sy < spr->size_y)
                {
                    ++sprCounter;
                    FRIDGE_RAM_ADDR pixelAddr = spritesDataSize[sid] + (sx + sy*spr->size_x)/2;
                    FRIDGE_WORD sprcolor = 0;
                    if (pixelAddr < FRIDGE_GPU_SPRITE_MEMORY_SIZE)
                        sprcolor = gpu->sprite_mem[pixelAddr];
                    FRIDGE_WORD srgb[3] = {gpu->palette[sprcolor*3], gpu->palette[sprcolor*3+1], gpu->palette[sprcolor*3+2]};

                    if (spr->mode == FRIDGE_GPU_SPRITE_OPAQUE)
                    {
                        rgb[0] = srgb[0];
                        rgb[1] = srgb[1];
                        rgb[2] = srgb[2];
                    }
                    else if (spr->mode == FRIDGE_GPU_SPRITE_TRANSPARENT0)
                    {
                        if (sprcolor != 0)
                        {
                            rgb[0] = srgb[0];
                            rgb[1] = srgb[1];
                            rgb[2] = srgb[2];
                        }
                    }
                    else if (spr->mode == FRIDGE_GPU_SPRITE_ADDITIVE)
                    {
                        int rgb0 = rgb[0] + srgb[0];
                        int rgb1 = rgb[1] + srgb[1];
                        int rgb2 = rgb[2] + srgb[2];
                        rgb[0] = rgb0 < 0xff ? rgb0 : 0xff;
                        rgb[1] = rgb1 < 0xff ? rgb1 : 0xff;
                        rgb[2] = rgb2 < 0xff ? rgb2 : 0xff;
                    }
                    else if (spr->mode == FRIDGE_GPU_SPRITE_SUBTRACTIVE)
                    {
                        int rgb0 = rgb[0] - srgb[0];
                        int rgb1 = rgb[1] - srgb[1];
                        int rgb2 = rgb[2] - srgb[2];
                        rgb[0] = rgb0 > 0 ? rgb0 : 0;
                        rgb[1] = rgb1 > 0 ? rgb1 : 0;
                        rgb[2] = rgb2 > 0 ? rgb2 : 0;
                    }
                    else if (spr->mode == FRIDGE_GPU_SPRITE_BITWISE_AND)
                    {
                        rgb[0] = rgb[0] & srgb[0];
                        rgb[1] = rgb[1] & srgb[1];
                        rgb[2] = rgb[2] & srgb[2];
                    }
                    else if (spr->mode == FRIDGE_GPU_SPRITE_BITWISE_OR)
                    {
                        rgb[0] = rgb[0] | srgb[0];
                        rgb[1] = rgb[1] | srgb[1];
                        rgb[2] = rgb[2] | srgb[2];
                    }
                    else if (spr->mode == FRIDGE_GPU_SPRITE_BITWISE_XOR)
                    {
                        rgb[0] = rgb[0] ^ srgb[0];
                        rgb[1] = rgb[1] ^ srgb[1];
                        rgb[2] = rgb[2] ^ srgb[2];
                    }
                }
            }
            pixels[output_addr++] = rgb[0];
            pixels[output_addr++] = rgb[1];
            pixels[output_addr++] = rgb[2];
        }
        output_addr += pixelsRowOffset*3;
    }
}

void FRIDGE_gpu_render_txt_rgb8(FRIDGE_GPU* gpu, unsigned char* pixels, const FRIDGE_WORD* glyphBitmap)
{
    FRIDGE_WORD* frame = FRIDGE_gpu_visible_frame(gpu);
    unsigned int output_addr = 0;

    FRIDGE_WORD w = FRIDGE_GPU_FRAME_EGA_WIDTH;
    FRIDGE_WORD h = FRIDGE_GPU_FRAME_EGA_HEIGHT;

    for (FRIDGE_WORD iy = 0; iy < 0 + h; ++iy)
    {
        for (FRIDGE_WORD ix = 0; ix < 0 + w; ++ix)
        {
            if (ix >= FRIDGE_GPU_FRAME_EGA_WIDTH)
                continue;
            if (iy >= FRIDGE_GPU_FRAME_EGA_HEIGHT)
                continue;

            FRIDGE_WORD row = iy / FRIDGE_GPU_TEXT_GLYPH_HEIGHT;
            FRIDGE_WORD col = ix / FRIDGE_GPU_TEXT_GLYPH_WIDTH;
            FRIDGE_WORD chary = iy % FRIDGE_GPU_TEXT_GLYPH_HEIGHT;
            FRIDGE_WORD charx = ix % FRIDGE_GPU_TEXT_GLYPH_WIDTH;

            FRIDGE_RAM_ADDR charAddr = (row*FRIDGE_GPU_TEXT_FRAME_WIDTH + col)*2;
            FRIDGE_RAM_ADDR colorAddr = charAddr+1;

            FRIDGE_WORD charCode = frame[charAddr];
            FRIDGE_WORD foreColor = FRIDGE_GPU_RIGHT_PIXEL(frame[colorAddr]);
            FRIDGE_WORD backColor = FRIDGE_GPU_LEFT_PIXEL(frame[colorAddr]);

            FRIDGE_WORD glyphPixelColumn = glyphBitmap[charCode*FRIDGE_GPU_TEXT_GLYPH_WIDTH+charx];
            FRIDGE_WORD glyphPixel = (glyphPixelColumn << chary) & FRIDGE_HIGHBIT_MASK;

            FRIDGE_WORD color = glyphPixel ? foreColor : backColor;

            /*
            FRIDGE_WORD px = (ix + gpu->frame_hor_offset) % FRIDGE_GPU_FRAME_EGA_WIDTH;
            FRIDGE_WORD py = (iy + gpu->frame_ver_offset) % FRIDGE_GPU_FRAME_EGA_HEIGHT;
            FRIDGE_RAM_ADDR addr = (px + py*FRIDGE_GPU_FRAME_EGA_WIDTH)/2;

            FRIDGE_WORD color;
            if (px % 2 == 0)
                color = FRIDGE_GPU_LEFT_PIXEL(frame[addr]);
            else
                color = FRIDGE_GPU_RIGHT_PIXEL(frame[addr]);
                */

            FRIDGE_WORD rgb[3] = {gpu->palette[color*3], gpu->palette[color*3+1], gpu->palette[color*3+2]};

            pixels[output_addr++] = rgb[0];
            pixels[output_addr++] = rgb[1];
            pixels[output_addr++] = rgb[2];
        }
    }
}

void FRIDGE_keyboard_press(FRIDGE_SYSTEM* sys, FRIDGE_WORD key)
{
    FRIDGE_KEYBOARD_CONTROLLER* kbrd = sys->kbrd;
    FRIDGE_WORD keyData = FRIDGE_KEYBOARD_KEY_STATE_MASK | (FRIDGE_KEYBOARD_KEY_CODE_MASK & key);
    kbrd->key_buffer[kbrd->input_index++] = keyData;
    if (kbrd->input_index >= FRIDGE_KEYBOARD_BUFFER_SIZE)
        kbrd->input_index = 0;
}

void FRIDGE_keyboard_release(FRIDGE_SYSTEM* sys, FRIDGE_WORD key)
{
    FRIDGE_KEYBOARD_CONTROLLER* kbrd = sys->kbrd;
    FRIDGE_WORD keyData = FRIDGE_KEYBOARD_KEY_CODE_MASK & key;
    kbrd->key_buffer[kbrd->input_index++] = keyData;
    if (kbrd->input_index >= FRIDGE_KEYBOARD_BUFFER_SIZE)
        kbrd->input_index = 0;
}

