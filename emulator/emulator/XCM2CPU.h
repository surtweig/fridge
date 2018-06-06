#pragma once
#include "XCM2.h"
#include "XCM2RAM.h"
#include "XCM2IRCodes.h"

//#define XCM2CPU_DEBUG

class XCM2CPU;

typedef void(XCM2CPU::*IRPTR)();

#define XCM2_FLAG_SIGN_MASK   0x80
#define XCM2_FLAG_ZERO_MASK   0x40
#define XCM2_FLAG_AUX_MASK    0x10
#define XCM2_FLAG_PARITY_MASK 0x04
#define XCM2_FLAG_CARRY_MASK  0x01

struct XCM2CPU_DebugFrame
{
    XCM2_IRCODE ircode;
    XCM2_WORD rA;
    XCM2_WORD rB;
    XCM2_WORD rC;
    XCM2_WORD rD;
    XCM2_WORD rE;
    XCM2_WORD rH;
    XCM2_WORD rL;
    XCM2_RAM_ADDR SP;
    XCM2_RAM_ADDR PC;
    bool fSign;
    bool fZero;  
    bool fAux;   
    bool fParity;
    bool fCarry; 
};

class XCM2CPU : public IInterruptionListener
{
private:
	bool halted;        // HLT
	XCM2_RAM_ADDR PC;   // program counter
	XCM2_RAM_ADDR SP;   // stack pointer
	XCM2_WORD rA;       // registers
	XCM2_WORD rB;
	XCM2_WORD rC;
	XCM2_WORD rD;
	XCM2_WORD rE;
	XCM2_WORD rH;
	XCM2_WORD rL;
	XCM2_WORD rF;
	XCM2RAM* memory;
	IGraphicAdapter* gpu;
	IIODevice* devices[XCM2_MAX_IO_DEVICES];
	IRPTR demux[XCM2_MAX_INSTRUCTIONS];

    bool interruptsEnabled;
	XCM2_WORD inteArg0;
	XCM2_WORD inteArg1;
	bool inteReadArg0;

	inline bool fSign()   { return (rF & XCM2_FLAG_SIGN_MASK)   > 0; }
	inline bool fZero()   { return (rF & XCM2_FLAG_ZERO_MASK)   > 0; }
	inline bool fAux()    { return (rF & XCM2_FLAG_AUX_MASK)    > 0; }
	inline bool fParity() { return (rF & XCM2_FLAG_PARITY_MASK) > 0; }
	inline bool fCarry()  { return (rF & XCM2_FLAG_CARRY_MASK)  > 0; }        // state bits
	inline XCM2_DWORD pairBC() { return XCM2_DWORD_HL(rB, rC); }
	inline XCM2_DWORD pairDE() { return XCM2_DWORD_HL(rD, rE); }
	inline XCM2_DWORD pairHL() { return XCM2_DWORD_HL(rH, rL); }

	inline XCM2_WORD bSign()   { return (rF & XCM2_FLAG_SIGN_MASK)   > 0; }
	inline XCM2_WORD bZero()   { return (rF & XCM2_FLAG_ZERO_MASK)   > 0; }
	inline XCM2_WORD bAux()    { return (rF & XCM2_FLAG_AUX_MASK)    > 0; }
	inline XCM2_WORD bParity() { return (rF & XCM2_FLAG_PARITY_MASK) > 0; }
	inline XCM2_WORD bCarry()  { return (rF & XCM2_FLAG_CARRY_MASK)  > 0; }

public:
    XCM2CPU_DebugFrame currentDebugFrame;

	XCM2CPU(XCM2RAM* memory, IGraphicAdapter* gpu);
    inline bool Halted() { return halted; }
	void setDevice(int id, IIODevice* device);
	void reset();
	void in_contact_INTERRUPT(XCM2_Instruction ir) override;
	void tick();
	~XCM2CPU();

private:
	XCM2_WORD pcRead();
	XCM2_DWORD pcReadDouble();
	void stackPush(XCM2_WORD hi, XCM2_WORD lo);
	void stackPush(XCM2_DWORD v);
	void stackPop(XCM2_WORD &hi, XCM2_WORD &lo);
	void stackPop(XCM2_DWORD &v);
	void defaultSetFlags(XCM2_WORD result);
	void compareSetFlags(XCM2_WORD a, XCM2_WORD b);
	void setCarry(bool flag);

	void ir_NOP();

	void ir_MOV_AB();
	void ir_MOV_BA();
	void ir_MOV_CA();
	void ir_MOV_DA();
	void ir_MOV_EA();
	void ir_MOV_HA();
	void ir_MOV_LA();
	void ir_MOV_AC();
	void ir_MOV_BC();
	void ir_MOV_CB();
	void ir_MOV_DB();
	void ir_MOV_EB();
	void ir_MOV_HB();
	void ir_MOV_LB();
	void ir_MOV_AD();
	void ir_MOV_BD();
	void ir_MOV_CD();
	void ir_MOV_DC();
	void ir_MOV_EC();
	void ir_MOV_HC();
	void ir_MOV_LC();
	void ir_MOV_AE();
	void ir_MOV_BE();
	void ir_MOV_CE();
	void ir_MOV_DE();
	void ir_MOV_ED();
	void ir_MOV_HD();
	void ir_MOV_LD();
	void ir_MOV_AH();
	void ir_MOV_BH();
	void ir_MOV_CH();
	void ir_MOV_DH();
	void ir_MOV_EH();
	void ir_MOV_HE();
	void ir_MOV_LE();
	void ir_MOV_AL();
	void ir_MOV_BL();
	void ir_MOV_CL();
	void ir_MOV_DL();
	void ir_MOV_EL();
	void ir_MOV_HL();
	void ir_MOV_LH();
	void ir_MOV_AM();
	void ir_MOV_BM();
	void ir_MOV_CM();
	void ir_MOV_DM();
	void ir_MOV_EM();
	void ir_MOV_HM();
	void ir_MOV_LM();
	void ir_MOV_ML();
	void ir_MOV_MA();
	void ir_MOV_MB();
	void ir_MOV_MC();
	void ir_MOV_MD();
	void ir_MOV_ME();
	void ir_MOV_MH();

	void ir_MVI_A    (); 
	void ir_MVI_B	 ();
	void ir_MVI_C	 ();
	void ir_MVI_D	 ();
	void ir_MVI_E	 ();
	void ir_MVI_H	 ();
	void ir_MVI_L	 ();

	void ir_LXI_BC	 ();
	void ir_LXI_DE	 ();
	void ir_LXI_HL	 ();
	void ir_LXI_SP	 ();

	void ir_LDA		 ();
	void ir_STA		 ();
	void ir_LHLD	 ();
	void ir_SHLD	 ();

	void ir_LDAX_BC	 ();
	void ir_LDAX_DE	 ();
    void ir_LDAX_HL  ();
	void ir_STAX_BC	 ();
	void ir_STAX_DE	 ();
    void ir_STAX_HL  ();

	void ir_XCNG	 ();

	void ir_ADD_A	 ();
	void ir_ADD_B	 ();
	void ir_ADD_C	 ();
	void ir_ADD_D	 ();
	void ir_ADD_E	 ();
	void ir_ADD_H	 ();
	void ir_ADD_L	 ();
	void ir_ADD_M	 ();

	void ir_ADI		 ();

	void ir_ADC_A	 ();
	void ir_ADC_B	 ();
	void ir_ADC_C	 ();
	void ir_ADC_D	 ();
	void ir_ADC_E	 ();
	void ir_ADC_H	 ();
	void ir_ADC_L	 ();
	void ir_ADC_M	 ();

	void ir_ACI		 ();

	void ir_SUB_A	 ();
	void ir_SUB_B	 ();
	void ir_SUB_C	 ();
	void ir_SUB_D	 ();
	void ir_SUB_E	 ();
	void ir_SUB_H	 ();
	void ir_SUB_L	 ();
	void ir_SUB_M	 ();

	void ir_SUI		 ();

	void ir_SBB_A	 ();
	void ir_SBB_B	 ();
	void ir_SBB_C	 ();
	void ir_SBB_D	 ();
	void ir_SBB_E	 ();
	void ir_SBB_H	 ();
	void ir_SBB_L	 ();
	void ir_SBB_M	 ();

	void ir_SBI		 ();

	void ir_INR_A	 ();
	void ir_INR_B	 ();
	void ir_INR_C	 ();
	void ir_INR_D	 ();
	void ir_INR_E	 ();
	void ir_INR_H	 ();
	void ir_INR_L	 ();
	void ir_INR_M	 ();

	void ir_DCR_A	 ();
	void ir_DCR_B	 ();
	void ir_DCR_C	 ();
	void ir_DCR_D	 ();
	void ir_DCR_E	 ();
	void ir_DCR_H	 ();
	void ir_DCR_L	 ();
	void ir_DCR_M	 ();

	void ir_INX_BC	 ();
	void ir_INX_DE	 ();
	void ir_INX_HL	 ();
	void ir_INX_SP	 ();

	void ir_DCX_BC	 ();
	void ir_DCX_DE	 ();
	void ir_DCX_HL	 ();
	void ir_DCX_SP	 ();

	void ir_DAD_BC	 ();
	void ir_DAD_DE	 ();
	void ir_DAD_HL	 ();
	void ir_DAD_SP	 ();
	void ir_DAA		 ();

	void ir_ANA_A	 ();
	void ir_ANA_B	 ();
	void ir_ANA_C	 ();
	void ir_ANA_D	 ();
	void ir_ANA_E	 ();
	void ir_ANA_H	 ();
	void ir_ANA_L	 ();
	void ir_ANA_M	 ();

	void ir_ANI		 ();

	void ir_ORA_A	 ();
	void ir_ORA_B	 ();
	void ir_ORA_C	 ();
	void ir_ORA_D	 ();
	void ir_ORA_E	 ();
	void ir_ORA_H	 ();
	void ir_ORA_L	 ();
	void ir_ORA_M	 ();

	void ir_ORI		 ();

	void ir_XRA_A	 ();
	void ir_XRA_B	 ();
	void ir_XRA_C	 ();
	void ir_XRA_D	 ();
	void ir_XRA_E	 ();
	void ir_XRA_H	 ();
	void ir_XRA_L	 ();
	void ir_XRA_M	 ();

	void ir_XRI		 ();

	void ir_CMP_A	 ();
	void ir_CMP_B	 ();
	void ir_CMP_C	 ();
	void ir_CMP_D	 ();
	void ir_CMP_E	 ();
	void ir_CMP_H	 ();
	void ir_CMP_L	 ();
	void ir_CMP_M	 ();

	void ir_CPI		 ();

	void ir_RLC		 ();
	void ir_RRC		 ();
	void ir_RAL		 ();
	void ir_RAR		 ();
	void ir_CMA		 ();
	void ir_CMC		 ();
	void ir_STC		 ();
    void ir_RTC		 ();

	void ir_JMP		 ();
	void ir_JNZ		 ();
	void ir_JZ		 ();
	void ir_JNC		 ();
	void ir_JC		 ();
	void ir_JPO		 ();
	void ir_JPE		 ();
	void ir_JP		 ();
	void ir_JM		 ();

	void ir_CALL	 ();
	void ir_CNZ		 ();
	void ir_CZ		 ();
	void ir_CNC		 ();
	void ir_CC		 ();
	void ir_CPO		 ();
	void ir_CPE		 ();
	void ir_CP		 ();
	void ir_CM		 ();

	void ir_RET		 ();
	void ir_RNZ		 ();
	void ir_RZ		 ();
	void ir_RNC		 ();
	void ir_RC		 ();
	void ir_RPO		 ();
	void ir_RPE		 ();
	void ir_RP		 ();
	void ir_RM		 ();

	void ir_PCHL	 ();

	void ir_PUSH_AF	 ();
	void ir_PUSH_BC	 ();
	void ir_PUSH_DE	 ();
	void ir_PUSH_HL	 ();
	void ir_POP_AF	 ();
	void ir_POP_BC	 ();
	void ir_POP_DE	 ();
	void ir_POP_HL	 ();

	void ir_XTHL	 ();
	void ir_SPHL	 ();
    void ir_HLSP     ();

	void ir_IIN		 ();
	void ir_IOUT	 ();
	void ir_HLT		 ();
    void ir_EI       ();
    void ir_DI       ();

	void ir_VFSA	 ();
	void ir_VFSAC 	 ();
	void ir_VFLA  	 ();
	void ir_VFLAC 	 ();
	void ir_VFCLR    ();
	void ir_VSSA  	 ();
	void ir_VSSAC 	 ();
	void ir_VSLA  	 ();
	void ir_VSLAC 	 ();
	void ir_VPRE 	 ();
	void ir_VMODE 	 ();
	void ir_VPAL  	 ();

    void ir_VSS      ();
    void ir_VSDQ     ();
    void ir_VSDT     ();
    void ir_VSDA     ();
    void ir_VSBA     ();
    void ir_VSBO     ();
    void ir_VSBX     ();

    void ir_IR250    ();
    void ir_IR251    ();
    void ir_IR252    ();
    void ir_IR253    ();
    void ir_IR254    ();
    void ir_IR255    ();
	// 256
};

