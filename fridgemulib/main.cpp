#include <iostream>
#include <assert.h>
#include <string>

#define FRIDGE_ASSERT(expr, msg) \
        fassert(#expr, expr, __FILE__, __LINE__, msg)

extern "C"
{
#include "fridgemulib.h"
}

using namespace std;

// https://stackoverflow.com/a/37264642
void fassert(const char* expr_str, bool expr, const char* file, int line, string msg)
{
    if (!expr)
    {
        cerr << "Assertion failed:\t" << msg << "\n"
             << "Expected:\t" << expr_str << "\n"
             << "Source:\t\t" << file << ", line " << line << "\n";
        exit(1);
    }
}

bool test_cpu_mov(FRIDGE_CPU* cpu)
{
    FRIDGE_WORD data = 79;
    const FRIDGE_RAM_ADDR addr = 10;
    const int regsN = 8;
    FRIDGE_WORD mov_irs[regsN][regsN] = {
            {NOP   , MOV_AB, MOV_AC, MOV_AD, MOV_AE, MOV_AH, MOV_AL, MOV_AM},
            {MOV_BA, NOP   , MOV_BC, MOV_BD, MOV_BE, MOV_BH, MOV_BL, MOV_BM},
            {MOV_CA, MOV_CB, NOP   , MOV_CD, MOV_CE, MOV_CH, MOV_CL, MOV_CM},
            {MOV_DA, MOV_DB, MOV_DC, NOP   , MOV_DE, MOV_DH, MOV_DL, MOV_DM},
            {MOV_EA, MOV_EB, MOV_EC, MOV_ED, NOP   , MOV_EH, MOV_EL, MOV_EM},
            {MOV_HA, MOV_HB, MOV_HC, MOV_HD, MOV_HE, NOP   , MOV_HL, MOV_HM},
            {MOV_LA, MOV_LB, MOV_LC, MOV_LD, MOV_LE, MOV_LH, NOP   , MOV_LM},
            {MOV_MA, MOV_MB, MOV_MC, MOV_MD, MOV_ME, MOV_MH, MOV_ML, NOP   }
    };
    string regsNames[regsN] = {"A", "B", "C", "D", "E", "H", "L", "M"};
    FRIDGE_WORD* regs[regsN] = {&cpu->rA, &cpu->rB, &cpu->rC, &cpu->rD, &cpu->rE, &cpu->rH, &cpu->rL, &cpu->ram[addr]};

    for (int rfrom = 0; rfrom < regsN; ++rfrom)
        for (int rto = 0; rto < regsN; ++rto)
        {
            FRIDGE_cpu_reset(cpu);
            *regs[rto] = 0;
            *regs[rfrom] = data++;
            if (rfrom == 7 || rto == 7)
            {
                cpu->rH = FRIDGE_HIGH_WORD(addr);
                cpu->rL = FRIDGE_LOW_WORD(addr);
            }
            cpu->ram[0] = mov_irs[rto][rfrom];
            FRIDGE_cpu_tick(cpu);

            FRIDGE_ASSERT(*regs[rto] == *regs[rfrom], "MOV_" + regsNames[rto] + regsNames[rfrom] + " test failed.");
        }

    std::cout << "MOV test passed." << std::endl;
    return true;
}

bool test_cpu_stack(FRIDGE_CPU* cpu)
{
    int stackTestLength = 100;
    FRIDGE_cpu_reset(cpu);
    FRIDGE_RAM_ADDR sp = 0xffff;
#ifdef FRIDGE_ASCENDING_STACK
    sp = 0x1000;
#endif
    FRIDGE_RAM_ADDR pc = 0;
    FRIDGE_WORD* regs[8] = {&cpu->rA, &cpu->rF, &cpu->rB, &cpu->rC, &cpu->rD, &cpu->rE, &cpu->rH, &cpu->rL};
    string pairNames[4] = {"AF", "BC", "DE", "HL"};
    cpu->SP = sp;
    for (int i = 0; i < stackTestLength; ++i)
    {
        int pair = i%4;
        FRIDGE_WORD r1 = i*2;
        FRIDGE_WORD r2 = i*2+1;
        *regs[pair*2] = r1;
        *regs[pair*2+1] = r2;
        cpu->ram[pc++] = PUSH_AF + pair;
        FRIDGE_cpu_tick(cpu);

#ifdef FRIDGE_ASCENDING_STACK
        sp += 2;
        FRIDGE_ASSERT(cpu->ram[sp-2] == r1 && cpu->ram[sp-1] == r2, "PUSH_" + pairNames[pair] + " test failed.");
#else
        sp -= 2;
        FRIDGE_ASSERT(cpu->ram[sp] == r1 && cpu->ram[sp+1] == r2, "PUSH_" + pairNames[pair] + " test failed.");
#endif
    }
    std::cout << "PUSH test passed." << std::endl;

    for (int i = 0; i < stackTestLength; ++i)
    {
        int pair = i%4;
        FRIDGE_WORD r1 = cpu->ram[sp-2];
        FRIDGE_WORD r2 = cpu->ram[sp-1];
        cpu->ram[pc++] = POP_AF + pair;
        FRIDGE_cpu_tick(cpu);

#ifdef FRIDGE_ASCENDING_STACK
        sp -= 2;
        FRIDGE_ASSERT(*regs[pair*2] == r1 && *regs[pair*2+1] == r2, "POP_" + pairNames[pair] + " test failed.");
#else
        sp += 2;
        FRIDGE_ASSERT(*regs[pair*2] == r1 && *regs[pair*2+1] == r2, "POP_" + pairNames[pair] + " test failed.");
#endif
    }
    std::cout << "POP test passed." << std::endl;

    return true;
}


int main() {
    FRIDGE_CPU* cpu = new FRIDGE_CPU();
    FRIDGE_cpu_reset(cpu);
    std::cout << "Hello, World! This is fridgemulib test bench." << std::endl;

    test_cpu_mov(cpu);
    test_cpu_stack(cpu);
    return 0;
}
