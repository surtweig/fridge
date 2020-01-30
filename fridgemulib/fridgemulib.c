#include <fridgemulib.h>

void stackPush(FRIDGE_CPU* cpu, FRIDGE_WORD hi, FRIDGE_WORD lo)
{
    cpu->ram[cpu->SP-1] = lo;
    cpu->ram[cpu->SP-2] = hi;
    cpu->SP -= 2;
}

void stackPushD(FRIDGE_CPU* cpu, FRIDGE_DWORD v)
{
    stackPush(cpu, FRIDGE_HIGH_WORD(v), FRIDGE_LOW_WORD(v));
}

void stackPop(FRIDGE_CPU* cpu, FRIDGE_WORD* hi, FRIDGE_WORD* lo)
{
    *hi = cpu->ram[cpu->SP];
    *lo = cpu->ram[cpu->SP+1];
    cpu->SP += 2;
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
        return cpu->ram[++cpu->PC];
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
            cpu->inte = FRIDGE_CPU_INTERRUPTS_DISABLED;
            return cpu->inteArg1;
        }
    }
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

void ir_ADD_M(FRIDGE_CPU* cpu)
{
    FRIDGE_WORD m = cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)];
    setCarry(cpu, ~cpu->rA < m);
    cpu->rA += m;
    defaultSetFlags(cpu, cpu->rA);
}

void ir_ADI(FRIDGE_CPU* cpu)
{
    FRIDGE_WORD b = pcRead(cpu);
    setCarry(cpu, ~cpu->rA < b);
    cpu->rA += b;
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

void FRIDGE_cpu_reset (FRIDGE_CPU* cpu)
{
    cpu->state = FRIDGE_CPU_ACTIVE;
    cpu->PC = 0;
    cpu->SP = FRIDGE_RAM_SIZE-1;
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
}

void FRIDGE_cpu_tick (FRIDGE_CPU* cpu)
{
    if (cpu->state == FRIDGE_CPU_ACTIVE)
    {
        FRIDGE_WORD ircode = pcRead(cpu);
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

            case LXI_BC: cpu->rB = pcRead(cpu); cpu->rC = pcRead(cpu); break;
            case LXI_DE: cpu->rD = pcRead(cpu); cpu->rE = pcRead(cpu); break;
            case LXI_HL: cpu->rH = pcRead(cpu); cpu->rL = pcRead(cpu); break;
            case LXI_SP: cpu->SP = FRIDGE_DWORD_HL(pcRead(cpu), pcRead(cpu)); break;

            case LDA: cpu->rA = cpu->ram[FRIDGE_DWORD_HL(pcRead(cpu), pcRead(cpu))]; break;
            case STA: cpu->ram[FRIDGE_DWORD_HL(pcRead(cpu), pcRead(cpu))] = cpu->rA; break;

            case LHLD: ir_LHLD(cpu); break;
            case SHLD: ir_SHLD(cpu); break;

            case LDAX_BC: cpu->rA = cpu->ram[FRIDGE_DWORD_HL(cpu->rB, cpu->rC)]; break;
            case LDAX_DE: cpu->rA = cpu->ram[FRIDGE_DWORD_HL(cpu->rD, cpu->rE)]; break;
            case LDAX_HL: cpu->rA = cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]; break;
            case STAX_BC: cpu->ram[FRIDGE_DWORD_HL(cpu->rB, cpu->rC)] = cpu->rA; break;
            case STAX_DE: cpu->ram[FRIDGE_DWORD_HL(cpu->rD, cpu->rE)] = cpu->rA; break;
            case STAX_HL: cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)] = cpu->rA; break;

            case XCNG: ir_XCNG(cpu); break;

            case ADD_A: setCarry(cpu, ~cpu->rA < cpu->rA); cpu->rA += cpu->rA; defaultSetFlags(cpu, cpu->rA); break;
            case ADD_B: setCarry(cpu, ~cpu->rA < cpu->rB); cpu->rA += cpu->rB; defaultSetFlags(cpu, cpu->rA); break;
            case ADD_C: setCarry(cpu, ~cpu->rA < cpu->rC); cpu->rA += cpu->rC; defaultSetFlags(cpu, cpu->rA); break;
            case ADD_D: setCarry(cpu, ~cpu->rA < cpu->rD); cpu->rA += cpu->rD; defaultSetFlags(cpu, cpu->rA); break;
            case ADD_E: setCarry(cpu, ~cpu->rA < cpu->rE); cpu->rA += cpu->rE; defaultSetFlags(cpu, cpu->rA); break;
            case ADD_H: setCarry(cpu, ~cpu->rA < cpu->rH); cpu->rA += cpu->rH; defaultSetFlags(cpu, cpu->rA); break;
            case ADD_L: setCarry(cpu, ~cpu->rA < cpu->rL); cpu->rA += cpu->rL; defaultSetFlags(cpu, cpu->rA); break;

            case ADD_M: ir_ADD_M(cpu); break;
            case ADI: ir_ADI(cpu); break;

            case ADC_A: ir_ADC(cpu, cpu->rA); break;
            case ADC_B: ir_ADC(cpu, cpu->rB); break;
            case ADC_C: ir_ADC(cpu, cpu->rC); break;
            case ADC_D: ir_ADC(cpu, cpu->rD); break;
            case ADC_E: ir_ADC(cpu, cpu->rE); break;
            case ADC_H: ir_ADC(cpu, cpu->rH); break;
            case ADC_L: ir_ADC(cpu, cpu->rL); break;
            case ADC_M: ir_ADC(cpu, cpu->ram[FRIDGE_DWORD_HL(cpu->rH, cpu->rL)]); break;
            case ACI: ir_ADC(cpu, pcRead(cpu)); break;
        }
    }
}

void FRIDGE_cpu_interrupt (FRIDGE_CPU* cpu, FRIDGE_WORD ircode, FRIDGE_WORD arg0, FRIDGE_WORD arg1)
{

}

void FRIDGE_cpu_ram_read (FRIDGE_CPU* cpu, FRIDGE_WORD* buffer, FRIDGE_RAM_ADDR position, FRIDGE_SIZE_T size)
{

}

void FRIDGE_cpu_ram_write (FRIDGE_CPU* cpu, FRIDGE_WORD* buffer, FRIDGE_RAM_ADDR position, FRIDGE_SIZE_T size)
{

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

}

void FRIDGE_gpu_tick (FRIDGE_GPU* gpu)
{

}

FRIDGE_WORD* FRIDGE_gpu_visible_frame (const FRIDGE_GPU* gpu)
{

}


