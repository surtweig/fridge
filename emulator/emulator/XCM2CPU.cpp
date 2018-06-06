#include "stdafx.h"
#include "XCM2CPU.h"
#include <Windows.h>
#define IRDECL(x) demux[##x] = &XCM2CPU::ir_##x;

XCM2CPU::XCM2CPU(XCM2RAM* memory, IGraphicAdapter* gpu)
{
	this->memory = memory;
	this->gpu = gpu;
	ZeroMemory(devices, sizeof(IIODevice*)*XCM2_MAX_IO_DEVICES);
	reset();

	IRDECL(NOP)

	IRDECL(MOV_AB) IRDECL(MOV_AC) IRDECL(MOV_AD) IRDECL(MOV_AE) IRDECL(MOV_AH) IRDECL(MOV_AL)
	IRDECL(MOV_BA) IRDECL(MOV_BC) IRDECL(MOV_BD) IRDECL(MOV_BE) IRDECL(MOV_BH) IRDECL(MOV_BL)
	IRDECL(MOV_CA) IRDECL(MOV_CB) IRDECL(MOV_CD) IRDECL(MOV_CE) IRDECL(MOV_CH) IRDECL(MOV_CL)
	IRDECL(MOV_DA) IRDECL(MOV_DB) IRDECL(MOV_DC) IRDECL(MOV_DE) IRDECL(MOV_DH) IRDECL(MOV_DL)
	IRDECL(MOV_EA) IRDECL(MOV_EB) IRDECL(MOV_EC) IRDECL(MOV_ED) IRDECL(MOV_EH) IRDECL(MOV_EL)
	IRDECL(MOV_HA) IRDECL(MOV_HB) IRDECL(MOV_HC) IRDECL(MOV_HD)	IRDECL(MOV_HE) IRDECL(MOV_HL)
	IRDECL(MOV_LA) IRDECL(MOV_LB) IRDECL(MOV_LC) IRDECL(MOV_LD)	IRDECL(MOV_LE) IRDECL(MOV_LH)
	IRDECL(MOV_AM)
	IRDECL(MOV_BM)
	IRDECL(MOV_CM)
	IRDECL(MOV_DM)
	IRDECL(MOV_EM)
	IRDECL(MOV_HM)
	IRDECL(MOV_LM)
	IRDECL(MOV_ML)
	IRDECL(MOV_MA)
	IRDECL(MOV_MB)
	IRDECL(MOV_MC)
	IRDECL(MOV_MD)
	IRDECL(MOV_ME)
	IRDECL(MOV_MH)

	IRDECL(MVI_A) IRDECL(MVI_B) IRDECL(MVI_C) IRDECL(MVI_D) IRDECL(MVI_E) IRDECL(MVI_H) IRDECL(MVI_L)

	IRDECL(LXI_BC) IRDECL(LXI_DE) IRDECL(LXI_HL) IRDECL(LXI_SP)

	IRDECL(LDA)
	IRDECL(STA)
	IRDECL(LHLD)
	IRDECL(SHLD)

    IRDECL(LDAX_BC) IRDECL(LDAX_DE) IRDECL(LDAX_HL)
    IRDECL(STAX_BC) IRDECL(STAX_DE) IRDECL(STAX_HL)

	IRDECL(XCNG)

	IRDECL(ADD_A) IRDECL(ADD_B) IRDECL(ADD_C) IRDECL(ADD_D) IRDECL(ADD_E) IRDECL(ADD_H) IRDECL(ADD_L) IRDECL(ADD_M)
	IRDECL(ADI)
	IRDECL(ADC_A) IRDECL(ADC_B) IRDECL(ADC_C) IRDECL(ADC_D) IRDECL(ADC_E) IRDECL(ADC_H) IRDECL(ADC_L) IRDECL(ADC_M)
	IRDECL(ACI)

	IRDECL(SUB_A) IRDECL(SUB_B) IRDECL(SUB_C) IRDECL(SUB_D) IRDECL(SUB_E) IRDECL(SUB_H) IRDECL(SUB_L) IRDECL(SUB_M)
	IRDECL(SUI)
	IRDECL(SBB_A) IRDECL(SBB_B) IRDECL(SBB_C) IRDECL(SBB_D) IRDECL(SBB_E) IRDECL(SBB_H) IRDECL(SBB_L) IRDECL(SBB_M)
	IRDECL(SBI)

	IRDECL(INR_A) IRDECL(INR_B) IRDECL(INR_C) IRDECL(INR_D) IRDECL(INR_E) IRDECL(INR_H) IRDECL(INR_L) IRDECL(INR_M)
	IRDECL(DCR_A) IRDECL(DCR_B) IRDECL(DCR_C) IRDECL(DCR_D) IRDECL(DCR_E) IRDECL(DCR_H) IRDECL(DCR_L) IRDECL(DCR_M)

	IRDECL(INX_BC) IRDECL(INX_DE) IRDECL(INX_HL) IRDECL(INX_SP)
	IRDECL(DCX_BC) IRDECL(DCX_DE) IRDECL(DCX_HL) IRDECL(DCX_SP)

	IRDECL(DAD_BC) IRDECL(DAD_DE) IRDECL(DAD_HL) IRDECL(DAD_SP)

//	IRDECL(DAA)

	IRDECL(ANA_A) IRDECL(ANA_B) IRDECL(ANA_C) IRDECL(ANA_D) IRDECL(ANA_E) IRDECL(ANA_H) IRDECL(ANA_L) IRDECL(ANA_M)
	IRDECL(ANI)
	IRDECL(ORA_A) IRDECL(ORA_B) IRDECL(ORA_C) IRDECL(ORA_D) IRDECL(ORA_E) IRDECL(ORA_H) IRDECL(ORA_L) IRDECL(ORA_M)
	IRDECL(ORI)
	IRDECL(XRA_A) IRDECL(XRA_B) IRDECL(XRA_C) IRDECL(XRA_D) IRDECL(XRA_E) IRDECL(XRA_H) IRDECL(XRA_L) IRDECL(XRA_M)
	IRDECL(XRI)
	IRDECL(CMP_A) IRDECL(CMP_B) IRDECL(CMP_C) IRDECL(CMP_D) IRDECL(CMP_E) IRDECL(CMP_H) IRDECL(CMP_L) IRDECL(CMP_M)
	IRDECL(CPI)

	IRDECL(RLC)
	IRDECL(RRC)
	IRDECL(RAL)
	IRDECL(RAR)
	IRDECL(CMA)
	IRDECL(CMC)
	IRDECL(STC)
    IRDECL(RTC)

	IRDECL(JMP) IRDECL(JNZ) IRDECL(JZ) IRDECL(JNC) IRDECL(JC) IRDECL(JPO) IRDECL(JPE) IRDECL(JP) IRDECL(JM)
	IRDECL(CALL) IRDECL(CNZ) IRDECL(CZ) IRDECL(CNC) IRDECL(CC) IRDECL(CPO) IRDECL(CPE) IRDECL(CP) IRDECL(CM)
	IRDECL(RET) IRDECL(RNZ) IRDECL(RZ) IRDECL(RNC) IRDECL(RC) IRDECL(RPO) IRDECL(RPE) IRDECL(RP) IRDECL(RM)

	IRDECL(PCHL)

	IRDECL(PUSH_AF) IRDECL(PUSH_BC) IRDECL(PUSH_DE) IRDECL(PUSH_HL)
	IRDECL(POP_AF) IRDECL(POP_BC) IRDECL(POP_DE) IRDECL(POP_HL)

    IRDECL(XTHL) IRDECL(SPHL) IRDECL(HLSP)

    IRDECL(IIN) IRDECL(IOUT) IRDECL(HLT) IRDECL(EI) IRDECL(DI)

	IRDECL(VFSA) 
	IRDECL(VFSAC)
	IRDECL(VFLA) 
	IRDECL(VFLAC)
	IRDECL(VFCLR)

	IRDECL(VSSA) 
	IRDECL(VSSAC)
	IRDECL(VSLA) 
	IRDECL(VSLAC)

	IRDECL(VPRE)
	IRDECL(VMODE)
	IRDECL(VPAL) 

    IRDECL(VSS)
    IRDECL(VSDQ)
    IRDECL(VSDT)
    IRDECL(VSDA)
    IRDECL(VSBA)
    IRDECL(VSBO)
    IRDECL(VSBX)

    IRDECL(IR250)
    IRDECL(IR251)
    IRDECL(IR252)
    IRDECL(IR253)
    IRDECL(IR254)
    IRDECL(IR255)
}

void XCM2CPU::reset()
{
	halted = false;
	PC = 0;
	SP = XCM2_RAM_SIZE-1;
	rA = 0;
	rB = 0;
	rC = 0;
	rD = 0;
	rE = 0;
	rH = 0;
	rL = 0;
	rF = 0;
	inteArg0 = 0;
	inteArg1 = 0;
	inteReadArg0 = false;
    interruptsEnabled = false;
    currentDebugFrame = {};
}

void XCM2CPU::setDevice(int id, IIODevice* device)
{
	devices[id] = device;
}

void XCM2CPU::in_contact_INTERRUPT(XCM2_Instruction ir)
{
    if (interruptsEnabled)
    {
        if (ir.ircode == CALL) // ir_CALL will push onto the stack PC+2
            PC -= 2; 
        halted = true;
        inteReadArg0 = true;
        inteArg0 = ir.arg0;
        inteArg1 = ir.arg1;
        (this->*demux[ir.ircode])();
        interruptsEnabled = false; // According to 8080 programmers manual p.59
    }
	halted = false;
}

void XCM2CPU::tick()
{
	if (!halted)
	{
#ifdef XCM2CPU_DEBUG
        currentDebugFrame.PC = PC;
        currentDebugFrame.ircode = (XCM2_IRCODE)pcRead();
        (this->*demux[currentDebugFrame.ircode])();
        currentDebugFrame.rA = rA;
        currentDebugFrame.rB = rB;
        currentDebugFrame.rC = rC;
        currentDebugFrame.rD = rD;
        currentDebugFrame.rE = rE;
        currentDebugFrame.rH = rH;
        currentDebugFrame.rL = rL;
        currentDebugFrame.SP = SP;
        currentDebugFrame.fAux = fAux();
        currentDebugFrame.fParity = fParity();
        currentDebugFrame.fCarry = fCarry();
        currentDebugFrame.fZero = fZero();
        currentDebugFrame.fSign = fSign();
#else
		(this->*demux[pcRead()])();
#endif
	}
}

XCM2_WORD XCM2CPU::pcRead()
{
	if (!halted)
	{
		XCM2_WORD membyte = memory->load(PC);
		PC++;
		return membyte;
	}
	else
	{
		if (inteReadArg0)
		{
			inteReadArg0 = false;
			return inteArg0;
		}
		else
			return inteArg1;
	}
}

XCM2_DWORD XCM2CPU::pcReadDouble()
{
	// Big Endian
	return pcRead() << XCM2_WORD_BITS | pcRead();
}

void XCM2CPU::stackPush(XCM2_WORD hi, XCM2_WORD lo)
{
	memory->store(SP-1, lo);
	memory->store(SP-2, hi);
	SP -= 2;
}

void XCM2CPU::stackPush(XCM2_DWORD v)
{
	stackPush(XCM2_HIGH_WORD(v), XCM2_LOW_WORD(v));
}

void XCM2CPU::stackPop(XCM2_WORD &hi, XCM2_WORD &lo)
{
	hi = memory->load(SP);
	lo = memory->load(SP+1);
	SP += 2;
}

void XCM2CPU::stackPop(XCM2_DWORD &v)
{
	XCM2_WORD hi;
	XCM2_WORD lo;
	stackPop(hi, lo);
	v = XCM2_DWORD_HL(hi, lo);
}

void XCM2CPU::defaultSetFlags(XCM2_WORD result)
{
	rF &= ~(XCM2_FLAG_SIGN_MASK | XCM2_FLAG_ZERO_MASK | XCM2_FLAG_PARITY_MASK);
	if (result & 1 << (XCM2_WORD_BITS-1))
		rF |= XCM2_FLAG_SIGN_MASK;
	if (result == 0)
		rF |= XCM2_FLAG_ZERO_MASK;
	if (result % 2 == 0) // TODO doesn't meet i8080 specification
		rF |= XCM2_FLAG_PARITY_MASK;
}

void XCM2CPU::compareSetFlags(XCM2_WORD a, XCM2_WORD b)
{
	rF &= ~(XCM2_FLAG_SIGN_MASK | XCM2_FLAG_ZERO_MASK | XCM2_FLAG_PARITY_MASK | XCM2_FLAG_CARRY_MASK);
	if (a < b)
		rF |= XCM2_FLAG_SIGN_MASK;
	else if (a > b)
		rF |= XCM2_FLAG_CARRY_MASK;
	else
		rF |= XCM2_FLAG_ZERO_MASK;

	if ((a-b) % 2 == 0)
		rF |= XCM2_FLAG_PARITY_MASK; // TODO doesn't meet i8080 specification
}

void XCM2CPU::setCarry(bool flag)
{
	rF &= ~XCM2_FLAG_CARRY_MASK;
	if (flag)
		rF |= XCM2_FLAG_CARRY_MASK;
}

XCM2CPU::~XCM2CPU()
{
}

void XCM2CPU::ir_NOP() { }
void XCM2CPU::ir_MOV_AB() { rA = rB; }
void XCM2CPU::ir_MOV_BA() { rB = rA; }
void XCM2CPU::ir_MOV_CA() { rC = rA; }
void XCM2CPU::ir_MOV_DA() { rD = rA; }
void XCM2CPU::ir_MOV_EA() { rE = rA; }
void XCM2CPU::ir_MOV_HA() { rH = rA; }
void XCM2CPU::ir_MOV_LA() { rL = rA; }
void XCM2CPU::ir_MOV_AC() { rA = rC; }
void XCM2CPU::ir_MOV_BC() { rB = rC; }
void XCM2CPU::ir_MOV_CB() { rC = rB; }
void XCM2CPU::ir_MOV_DB() { rD = rB; }
void XCM2CPU::ir_MOV_EB() { rE = rB; }
void XCM2CPU::ir_MOV_HB() { rH = rB; }
void XCM2CPU::ir_MOV_LB() { rL = rB; }
void XCM2CPU::ir_MOV_AD() { rA = rD; }
void XCM2CPU::ir_MOV_BD() { rB = rD; }
void XCM2CPU::ir_MOV_CD() { rC = rD; }
void XCM2CPU::ir_MOV_DC() { rD = rC; }
void XCM2CPU::ir_MOV_EC() { rE = rC; }
void XCM2CPU::ir_MOV_HC() { rH = rC; }
void XCM2CPU::ir_MOV_LC() { rL = rC; }
void XCM2CPU::ir_MOV_AE() { rA = rE; }
void XCM2CPU::ir_MOV_BE() { rB = rE; }
void XCM2CPU::ir_MOV_CE() { rC = rE; }
void XCM2CPU::ir_MOV_DE() { rD = rE; }
void XCM2CPU::ir_MOV_ED() { rE = rD; }
void XCM2CPU::ir_MOV_HD() { rH = rD; }
void XCM2CPU::ir_MOV_LD() { rL = rD; }
void XCM2CPU::ir_MOV_AH() { rA = rH; }
void XCM2CPU::ir_MOV_BH() { rB = rH; }
void XCM2CPU::ir_MOV_CH() { rC = rH; }
void XCM2CPU::ir_MOV_DH() { rD = rH; }
void XCM2CPU::ir_MOV_EH() { rE = rH; }
void XCM2CPU::ir_MOV_HE() { rH = rE; }
void XCM2CPU::ir_MOV_LE() { rL = rE; }
void XCM2CPU::ir_MOV_AL() { rA = rL; }
void XCM2CPU::ir_MOV_BL() { rB = rL; }
void XCM2CPU::ir_MOV_CL() { rC = rL; }
void XCM2CPU::ir_MOV_DL() { rD = rL; }
void XCM2CPU::ir_MOV_EL() { rE = rL; }
void XCM2CPU::ir_MOV_HL() { rH = rL; }
void XCM2CPU::ir_MOV_LH() { rL = rH; }
void XCM2CPU::ir_MOV_AM() { rA = memory->load(rH, rL); }
void XCM2CPU::ir_MOV_BM() { rB = memory->load(rH, rL); }
void XCM2CPU::ir_MOV_CM() { rC = memory->load(rH, rL); }
void XCM2CPU::ir_MOV_DM() { rD = memory->load(rH, rL); }
void XCM2CPU::ir_MOV_EM() { rE = memory->load(rH, rL); }
void XCM2CPU::ir_MOV_HM() { rH = memory->load(rH, rL); }
void XCM2CPU::ir_MOV_LM() { rL = memory->load(rH, rL); }
void XCM2CPU::ir_MOV_MA() { memory->store(rH, rL, rA); }
void XCM2CPU::ir_MOV_MB() { memory->store(rH, rL, rB); }
void XCM2CPU::ir_MOV_MC() { memory->store(rH, rL, rC); }
void XCM2CPU::ir_MOV_MD() { memory->store(rH, rL, rD); }
void XCM2CPU::ir_MOV_ME() { memory->store(rH, rL, rE); }
void XCM2CPU::ir_MOV_MH() { memory->store(rH, rL, rH); }
void XCM2CPU::ir_MOV_ML() { memory->store(rH, rL, rL); }

void XCM2CPU::ir_MVI_A() { rA = pcRead(); } 
void XCM2CPU::ir_MVI_B() { rB = pcRead(); }
void XCM2CPU::ir_MVI_C() { rC = pcRead(); }
void XCM2CPU::ir_MVI_D() { rD = pcRead(); }
void XCM2CPU::ir_MVI_E() { rE = pcRead(); }
void XCM2CPU::ir_MVI_H() { rH = pcRead(); }
void XCM2CPU::ir_MVI_L() { rL = pcRead(); }

void XCM2CPU::ir_LXI_BC() { rB = pcRead(); rC = pcRead();}
void XCM2CPU::ir_LXI_DE() { rD = pcRead(); rE = pcRead();}
void XCM2CPU::ir_LXI_HL() { rH = pcRead(); rL = pcRead();}
void XCM2CPU::ir_LXI_SP() { SP = pcReadDouble(); }

void XCM2CPU::ir_LDA() { rA = memory->load(XCM2_DWORD_HL(pcRead(), pcRead())); }
void XCM2CPU::ir_STA() { memory->store(XCM2_DWORD_HL(pcRead(), pcRead()), rA); }

void XCM2CPU::ir_LHLD()
{
	XCM2_RAM_ADDR addr = pcReadDouble();
	rH = memory->load(addr);
	rL = memory->load(addr+1);
}

void XCM2CPU::ir_SHLD()
{
	XCM2_RAM_ADDR addr = pcReadDouble();
	memory->store(addr, rH);
	memory->store(addr+1, rL);
}

void XCM2CPU::ir_LDAX_BC() { rA = memory->load(rB, rC); }
void XCM2CPU::ir_LDAX_DE() { rA = memory->load(rD, rE); }
void XCM2CPU::ir_LDAX_HL() { rA = memory->load(rH, rL); }
void XCM2CPU::ir_STAX_BC() { memory->store(rB, rC, rA); }
void XCM2CPU::ir_STAX_DE() { memory->store(rD, rE, rA); }
void XCM2CPU::ir_STAX_HL() { memory->store(rH, rL, rA); }

void XCM2CPU::ir_XCNG()
{
	XCM2_WORD t;
	t = rD;
	rD = rH;
	rH = t;
	t = rE;
	rE = rL;
	rL = t;
}

void XCM2CPU::ir_ADD_A() { setCarry(~rA < rA); rA += rA; defaultSetFlags(rA); }
void XCM2CPU::ir_ADD_B() { setCarry(~rA < rB); rA += rB; defaultSetFlags(rA); }
void XCM2CPU::ir_ADD_C() { setCarry(~rA < rC); rA += rC; defaultSetFlags(rA); }
void XCM2CPU::ir_ADD_D() { setCarry(~rA < rD); rA += rD; defaultSetFlags(rA); }
void XCM2CPU::ir_ADD_E() { setCarry(~rA < rE); rA += rE; defaultSetFlags(rA); }
void XCM2CPU::ir_ADD_H() { setCarry(~rA < rH); rA += rH; defaultSetFlags(rA); }
void XCM2CPU::ir_ADD_L() { setCarry(~rA < rL); rA += rL; defaultSetFlags(rA); }
void XCM2CPU::ir_ADD_M()
{
	XCM2_WORD m = memory->load(rH, rL);
	setCarry(~rA < m);
	rA += m;
	defaultSetFlags(rA);
}

void XCM2CPU::ir_ADI()
{ 
	XCM2_WORD b = pcRead(); 
	setCarry(~rA < b);
	rA += b;
	defaultSetFlags(rA);
}

void XCM2CPU::ir_ADC_A() { XCM2_WORD c = bCarry(); setCarry(~rA < rA + c); rA += rA + c; defaultSetFlags(rA); }
void XCM2CPU::ir_ADC_B() { XCM2_WORD c = bCarry(); setCarry(~rA < rB + c); rA += rB + c; defaultSetFlags(rA); }
void XCM2CPU::ir_ADC_C() { XCM2_WORD c = bCarry(); setCarry(~rA < rC + c); rA += rC + c; defaultSetFlags(rA); }
void XCM2CPU::ir_ADC_D() { XCM2_WORD c = bCarry(); setCarry(~rA < rD + c); rA += rD + c; defaultSetFlags(rA); }
void XCM2CPU::ir_ADC_E() { XCM2_WORD c = bCarry(); setCarry(~rA < rE + c); rA += rE + c; defaultSetFlags(rA); }
void XCM2CPU::ir_ADC_H() { XCM2_WORD c = bCarry(); setCarry(~rA < rH + c); rA += rH + c; defaultSetFlags(rA); }
void XCM2CPU::ir_ADC_L() { XCM2_WORD c = bCarry(); setCarry(~rA < rL + c); rA += rL + c; defaultSetFlags(rA); }
void XCM2CPU::ir_ADC_M()
{
	XCM2_WORD m = memory->load(rH, rL);
	XCM2_WORD c = bCarry();
	setCarry(~rA < m + c);
	rA += m + c;
	defaultSetFlags(rA);
}

void XCM2CPU::ir_ACI()
{ 
	XCM2_WORD b = pcRead(); 
	XCM2_WORD c = bCarry();
	setCarry(~rA < b + c);
	rA += b + c;
	defaultSetFlags(rA);
}

void XCM2CPU::ir_SUB_A() { setCarry(rA < rA); rA -= rA; defaultSetFlags(rA); }
void XCM2CPU::ir_SUB_B() { setCarry(rA < rB); rA -= rB; defaultSetFlags(rA); }
void XCM2CPU::ir_SUB_C() { setCarry(rA < rC); rA -= rC; defaultSetFlags(rA); }
void XCM2CPU::ir_SUB_D() { setCarry(rA < rD); rA -= rD; defaultSetFlags(rA); }
void XCM2CPU::ir_SUB_E() { setCarry(rA < rE); rA -= rE; defaultSetFlags(rA); }
void XCM2CPU::ir_SUB_H() { setCarry(rA < rH); rA -= rH; defaultSetFlags(rA); }
void XCM2CPU::ir_SUB_L() { setCarry(rA < rL); rA -= rL; defaultSetFlags(rA); }
void XCM2CPU::ir_SUB_M()
{
	XCM2_WORD m = memory->load(rH, rL);
	setCarry(rA < m);
	rA -= m;
	defaultSetFlags(rA);
}

void XCM2CPU::ir_SUI()
{ 
	XCM2_WORD b = pcRead(); 
	setCarry(rA < b);
	rA -= b;
	defaultSetFlags(rA);
}

void XCM2CPU::ir_SBB_A() { XCM2_WORD c = bCarry(); setCarry(rA < rA + c); rA -= (rA + c); defaultSetFlags(rA); }
void XCM2CPU::ir_SBB_B() { XCM2_WORD c = bCarry(); setCarry(rA < rB + c); rA -= (rB + c); defaultSetFlags(rA); }
void XCM2CPU::ir_SBB_C() { XCM2_WORD c = bCarry(); setCarry(rA < rC + c); rA -= (rC + c); defaultSetFlags(rA); }
void XCM2CPU::ir_SBB_D() { XCM2_WORD c = bCarry(); setCarry(rA < rD + c); rA -= (rD + c); defaultSetFlags(rA); }
void XCM2CPU::ir_SBB_E() { XCM2_WORD c = bCarry(); setCarry(rA < rE + c); rA -= (rE + c); defaultSetFlags(rA); }
void XCM2CPU::ir_SBB_H() { XCM2_WORD c = bCarry(); setCarry(rA < rH + c); rA -= (rH + c); defaultSetFlags(rA); }
void XCM2CPU::ir_SBB_L() { XCM2_WORD c = bCarry(); setCarry(rA < rL + c); rA -= (rL + c); defaultSetFlags(rA); }
void XCM2CPU::ir_SBB_M()
{
	XCM2_WORD m = memory->load(rH, rL);
	XCM2_WORD c = bCarry();
	setCarry(rA < m + c);
	rA -= (m + c);
	defaultSetFlags(rA);
}

void XCM2CPU::ir_SBI()
{ 
	XCM2_WORD b = pcRead(); 
	XCM2_WORD c = bCarry();
	setCarry(rA < b + c);
	rA -= (b + c);
	defaultSetFlags(rA);
}

void XCM2CPU::ir_INR_A() { setCarry(~rA == 0); rA++; defaultSetFlags(rA); }
void XCM2CPU::ir_INR_B() { setCarry(~rB == 0); rB++; defaultSetFlags(rB); }
void XCM2CPU::ir_INR_C() { setCarry(~rC == 0); rC++; defaultSetFlags(rC); }
void XCM2CPU::ir_INR_D() { setCarry(~rD == 0); rD++; defaultSetFlags(rD); }
void XCM2CPU::ir_INR_E() { setCarry(~rE == 0); rE++; defaultSetFlags(rE); }
void XCM2CPU::ir_INR_H() { setCarry(~rH == 0); rH++; defaultSetFlags(rH); }
void XCM2CPU::ir_INR_L() { setCarry(~rL == 0); rL++; defaultSetFlags(rL); }
void XCM2CPU::ir_INR_M()
{
	XCM2_WORD m = memory->load(rH, rL);
	setCarry(~m == 0);
	m++;
	memory->store(rH, rL, m);
	defaultSetFlags(m);
}

void XCM2CPU::ir_DCR_A() { setCarry(rA == 0); rA--; defaultSetFlags(rA); }
void XCM2CPU::ir_DCR_B() { setCarry(rB == 0); rB--; defaultSetFlags(rB); }
void XCM2CPU::ir_DCR_C() { setCarry(rC == 0); rC--; defaultSetFlags(rC); }
void XCM2CPU::ir_DCR_D() { setCarry(rD == 0); rD--; defaultSetFlags(rD); }
void XCM2CPU::ir_DCR_E() { setCarry(rE == 0); rE--; defaultSetFlags(rE); }
void XCM2CPU::ir_DCR_H() { setCarry(rH == 0); rH--; defaultSetFlags(rH); }
void XCM2CPU::ir_DCR_L() { setCarry(rL == 0); rL--; defaultSetFlags(rL); }
void XCM2CPU::ir_DCR_M()
{
	XCM2_WORD m = memory->load(rH, rL);
	setCarry(m == 0);
	m--;
	memory->store(rH, rL, m);
	defaultSetFlags(m);
}

void XCM2CPU::ir_INX_BC() { XCM2_DWORD buffer = pairBC(); buffer++; rB = XCM2_HIGH_WORD(buffer); rC = XCM2_LOW_WORD(buffer); }
void XCM2CPU::ir_INX_DE() { XCM2_DWORD buffer =	pairDE(); buffer++; rD = XCM2_HIGH_WORD(buffer); rE = XCM2_LOW_WORD(buffer); }
void XCM2CPU::ir_INX_HL() { XCM2_DWORD buffer =	pairHL(); buffer++; rH = XCM2_HIGH_WORD(buffer); rL = XCM2_LOW_WORD(buffer); }
void XCM2CPU::ir_INX_SP() { SP++; }

void XCM2CPU::ir_DCX_BC() { XCM2_DWORD buffer = pairBC(); buffer--; rB = XCM2_HIGH_WORD(buffer); rC = XCM2_LOW_WORD(buffer); }
void XCM2CPU::ir_DCX_DE() { XCM2_DWORD buffer =	pairDE(); buffer--; rD = XCM2_HIGH_WORD(buffer); rE = XCM2_LOW_WORD(buffer); }
void XCM2CPU::ir_DCX_HL() { XCM2_DWORD buffer =	pairHL(); buffer--; rH = XCM2_HIGH_WORD(buffer); rL = XCM2_LOW_WORD(buffer); }
void XCM2CPU::ir_DCX_SP() { SP--; }

void XCM2CPU::ir_DAD_BC () { XCM2_DWORD buffer = pairHL(); buffer += pairBC(); rH = XCM2_HIGH_WORD(buffer); rL = XCM2_LOW_WORD(buffer); }
void XCM2CPU::ir_DAD_DE () { XCM2_DWORD buffer = pairHL(); buffer += pairDE(); rH = XCM2_HIGH_WORD(buffer); rL = XCM2_LOW_WORD(buffer); }
void XCM2CPU::ir_DAD_HL () { XCM2_DWORD buffer = pairHL(); buffer += pairHL(); rH = XCM2_HIGH_WORD(buffer); rL = XCM2_LOW_WORD(buffer); }
void XCM2CPU::ir_DAD_SP () { XCM2_DWORD buffer = pairHL(); buffer += SP; rH = XCM2_HIGH_WORD(buffer); rL = XCM2_LOW_WORD(buffer); }
void XCM2CPU::ir_DAA() { } // unimplemented

void XCM2CPU::ir_ANA_A() { rA &= rA; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ANA_B() { rA &= rB; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ANA_C() { rA &= rC; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ANA_D() { rA &= rD; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ANA_E() { rA &= rE; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ANA_H() { rA &= rH; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ANA_L() { rA &= rL; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ANA_M() { rA &= memory->load(rH, rL); setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ANI()   { rA &= pcRead(); setCarry(false); defaultSetFlags(rA); }

void XCM2CPU::ir_ORA_A() { rA |= rA; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ORA_B() { rA |= rB; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ORA_C() { rA |= rC; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ORA_D() { rA |= rD; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ORA_E() { rA |= rE; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ORA_H() { rA |= rH; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ORA_L() { rA |= rL; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ORA_M() { rA |= memory->load(rH, rL); setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_ORI  () { rA |= pcRead(); setCarry(false); defaultSetFlags(rA); }

void XCM2CPU::ir_XRA_A() { rA ^= rA; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_XRA_B() { rA ^= rB; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_XRA_C() { rA ^= rC; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_XRA_D() { rA ^= rD; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_XRA_E() { rA ^= rE; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_XRA_H() { rA ^= rH; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_XRA_L() { rA ^= rL; setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_XRA_M() { rA ^= memory->load(rH, rL); setCarry(false); defaultSetFlags(rA); }
void XCM2CPU::ir_XRI()	 { rA ^= pcRead(); setCarry(false); defaultSetFlags(rA); }

void XCM2CPU::ir_CMP_A() { compareSetFlags(rA, rA); }
void XCM2CPU::ir_CMP_B() { compareSetFlags(rA, rB); }
void XCM2CPU::ir_CMP_C() { compareSetFlags(rA, rC); }
void XCM2CPU::ir_CMP_D() { compareSetFlags(rA, rD); }
void XCM2CPU::ir_CMP_E() { compareSetFlags(rA, rE); }
void XCM2CPU::ir_CMP_H() { compareSetFlags(rA, rH); }
void XCM2CPU::ir_CMP_L() { compareSetFlags(rA, rL); }
void XCM2CPU::ir_CMP_M() { compareSetFlags(rA, memory->load(rH, rL)); }
void XCM2CPU::ir_CPI()   { compareSetFlags(rA, pcRead()); }

void XCM2CPU::ir_RLC() 
{
	setCarry((rA & XCM2_HIGHBIT_MASK) > 0);
	rA = (rA << 1) | ((rA & XCM2_HIGHBIT_MASK) >> (XCM2_WORD_BITS-1));
}
void XCM2CPU::ir_RRC() 
{
	setCarry((rA & XCM2_LOWBIT_MASK) > 0); 
	rA = (rA >> 1) | ((rA & XCM2_LOWBIT_MASK) << (XCM2_WORD_BITS-1));
}
void XCM2CPU::ir_RAL()
{
	bool c = (rA & XCM2_HIGHBIT_MASK) > 0;
	rA = (rA << 1) | (XCM2_WORD)fCarry();
	setCarry(c);
}
void XCM2CPU::ir_RAR() 
{
	bool c = (rA & XCM2_LOWBIT_MASK) > 0;
	rA = (rA >> 1) | ((XCM2_WORD)fCarry() << (XCM2_WORD_BITS-1));
	setCarry(c);
}
void XCM2CPU::ir_CMA() { rA = ~rA; }
void XCM2CPU::ir_CMC() { setCarry(!fCarry()); }
void XCM2CPU::ir_STC() { setCarry(true); }
void XCM2CPU::ir_RTC() { setCarry(false); }

void XCM2CPU::ir_JMP() { PC = pcReadDouble(); }
void XCM2CPU::ir_JNZ() { XCM2_RAM_ADDR buffer = pcReadDouble(); if (!fZero())   PC = buffer; }
void XCM2CPU::ir_JZ	() { XCM2_RAM_ADDR buffer = pcReadDouble(); if (fZero())    PC = buffer; }
void XCM2CPU::ir_JNC() { XCM2_RAM_ADDR buffer = pcReadDouble(); if (!fCarry())  PC = buffer; }
void XCM2CPU::ir_JC	() { XCM2_RAM_ADDR buffer = pcReadDouble(); if (fCarry())   PC = buffer; }
void XCM2CPU::ir_JPO() { XCM2_RAM_ADDR buffer = pcReadDouble(); if (!fParity()) PC = buffer; }
void XCM2CPU::ir_JPE() { XCM2_RAM_ADDR buffer = pcReadDouble(); if (fParity())  PC = buffer; }
void XCM2CPU::ir_JP	() { XCM2_RAM_ADDR buffer = pcReadDouble(); if (!fSign())   PC = buffer; }
void XCM2CPU::ir_JM	() { XCM2_RAM_ADDR buffer = pcReadDouble(); if (fSign())    PC = buffer; }

void XCM2CPU::ir_CALL() { stackPush(PC+2); PC = pcReadDouble(); }
void XCM2CPU::ir_CNZ ()	{ XCM2_RAM_ADDR buffer = pcReadDouble(); if (!fZero())   { stackPush(PC+2); PC = buffer; } } 
void XCM2CPU::ir_CZ	 ()	{ XCM2_RAM_ADDR buffer = pcReadDouble(); if (fZero())	 { stackPush(PC+2); PC = buffer; } }
void XCM2CPU::ir_CNC ()	{ XCM2_RAM_ADDR buffer = pcReadDouble(); if (!fCarry())  { stackPush(PC+2); PC = buffer; } }
void XCM2CPU::ir_CC	 ()	{ XCM2_RAM_ADDR buffer = pcReadDouble(); if (fCarry())   { stackPush(PC+2); PC = buffer; } }
void XCM2CPU::ir_CPO ()	{ XCM2_RAM_ADDR buffer = pcReadDouble(); if (!fParity()) { stackPush(PC+2); PC = buffer; } }
void XCM2CPU::ir_CPE ()	{ XCM2_RAM_ADDR buffer = pcReadDouble(); if (fParity())  { stackPush(PC+2); PC = buffer; } }
void XCM2CPU::ir_CP	 ()	{ XCM2_RAM_ADDR buffer = pcReadDouble(); if (!fSign())   { stackPush(PC+2); PC = buffer; } }
void XCM2CPU::ir_CM	 ()	{ XCM2_RAM_ADDR buffer = pcReadDouble(); if (fSign()) 	 { stackPush(PC+2); PC = buffer; } }

void XCM2CPU::ir_RET() { stackPop(PC); }
void XCM2CPU::ir_RNZ() { if (!fZero())   stackPop(PC); }
void XCM2CPU::ir_RZ	() { if (fZero())	 stackPop(PC); }
void XCM2CPU::ir_RNC() { if (!fCarry())  stackPop(PC); }
void XCM2CPU::ir_RC	() { if (fCarry())   stackPop(PC); }
void XCM2CPU::ir_RPO() { if (!fParity()) stackPop(PC); }
void XCM2CPU::ir_RPE() { if (fParity())  stackPop(PC); }
void XCM2CPU::ir_RP () { if (!fSign())   stackPop(PC); }
void XCM2CPU::ir_RM	() { if (fSign()) 	 stackPop(PC); }

void XCM2CPU::ir_PCHL() { PC = pairHL(); }

void XCM2CPU::ir_PUSH_AF() { stackPush(rA, rF); }
void XCM2CPU::ir_PUSH_BC() { stackPush(rB, rC); }
void XCM2CPU::ir_PUSH_DE() { stackPush(rD, rE); }
void XCM2CPU::ir_PUSH_HL() { stackPush(rH, rL); }
void XCM2CPU::ir_POP_AF	() { stackPop(rA, rF); }
void XCM2CPU::ir_POP_BC	() { stackPop(rB, rC); }
void XCM2CPU::ir_POP_DE	() { stackPop(rD, rE); }
void XCM2CPU::ir_POP_HL	() { stackPop(rH, rL); }

void XCM2CPU::ir_XTHL()
{
	XCM2_WORD th = rH;
	XCM2_WORD tl = rL;
	rH = memory->load(SP);
	rL = memory->load(SP+1);
	memory->store(SP, th);
	memory->store(SP+1, tl);
}

void XCM2CPU::ir_SPHL()	{ SP = pairHL(); }

void XCM2CPU::ir_HLSP() { rH = XCM2_HIGH_WORD(SP); rL = XCM2_LOW_WORD(SP); }

void XCM2CPU::ir_IIN()  
{
    IIODevice* device = devices[pcRead()];
    if (device)
        rA = device->out_contact_OUT();
}
void XCM2CPU::ir_IOUT()
{
    IIODevice* device = devices[pcRead()];
    if (device)
        device->in_contact_IN(rA);
}
void XCM2CPU::ir_HLT()  { halted = true; }
void XCM2CPU::ir_DI()   { interruptsEnabled = false; }
void XCM2CPU::ir_EI()   { interruptsEnabled = true; }

void XCM2CPU::ir_VFSA () { gpu->back()->store(pairHL(), rA); }
void XCM2CPU::ir_VFSAC() { gpu->back()->storePixel(pairHL(), rA);}
void XCM2CPU::ir_VFLA () { rA = gpu->back()->load(pairHL()); }
void XCM2CPU::ir_VFLAC() { rA = gpu->back()->loadPixel(pairHL()); }
void XCM2CPU::ir_VFCLR() { gpu->back()->clear(); }
void XCM2CPU::ir_VSSA () { gpu->sprite()->store(pairHL(), rA); }
void XCM2CPU::ir_VSSAC() { gpu->sprite()->storePixel(pairHL(), rA);}
void XCM2CPU::ir_VSLA () { rA = gpu->sprite()->load(pairHL()); }
void XCM2CPU::ir_VSLAC() { rA = gpu->sprite()->loadPixel(pairHL()); }
void XCM2CPU::ir_VPRE () { gpu->present(); }
void XCM2CPU::ir_VMODE() { gpu->switchMode((XCM2_VIDEO_MODE)pcRead()); }
void XCM2CPU::ir_VPAL () { gpu->readPallette(); }

void XCM2CPU::ir_VSS () {}
void XCM2CPU::ir_VSDQ() {}
void XCM2CPU::ir_VSDT() {}
void XCM2CPU::ir_VSDA() {}
void XCM2CPU::ir_VSBA() {}
void XCM2CPU::ir_VSBO() {}
void XCM2CPU::ir_VSBX() {}

void XCM2CPU::ir_IR250() {}
void XCM2CPU::ir_IR251() {}
void XCM2CPU::ir_IR252() {}
void XCM2CPU::ir_IR253() {}
void XCM2CPU::ir_IR254() {}
void XCM2CPU::ir_IR255() {}
