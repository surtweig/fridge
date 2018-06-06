#pragma once

typedef enum XCM2_IRCODE {

	NOP,

	MOV_AB, MOV_AC, MOV_AD, MOV_AE, MOV_AH, MOV_AL, MOV_AM,
	MOV_BA, MOV_BC, MOV_BD, MOV_BE, MOV_BH, MOV_BL, MOV_BM,
	MOV_CA, MOV_CB, MOV_CD, MOV_CE, MOV_CH, MOV_CL, MOV_CM,
	MOV_DA, MOV_DB, MOV_DC, MOV_DE, MOV_DH, MOV_DL, MOV_DM,
	MOV_EA, MOV_EB, MOV_EC, MOV_ED, MOV_EH, MOV_EL, MOV_EM,
	MOV_HA, MOV_HB, MOV_HC,	MOV_HD,	MOV_HE,	MOV_HL, MOV_HM,
	MOV_LA,	MOV_LB,	MOV_LC,	MOV_LD,	MOV_LE,	MOV_LH, MOV_LM,
	MOV_MA, MOV_MB, MOV_MC, MOV_MD, MOV_ME, MOV_MH, MOV_ML,

	MVI_A, MVI_B, MVI_C, MVI_D, MVI_E, MVI_H, MVI_L,

	LXI_BC, LXI_DE, LXI_HL, LXI_SP,

	LDA,
	STA,
	LHLD,
	SHLD,

	LDAX_BC, LDAX_DE, LDAX_HL,
	STAX_BC, STAX_DE, STAX_HL,

	XCNG,

	ADD_A, ADD_B, ADD_C, ADD_D, ADD_E, ADD_H, ADD_L, ADD_M,
	ADI,
	ADC_A, ADC_B, ADC_C, ADC_D, ADC_E, ADC_H, ADC_L, ADC_M,
	ACI,

	SUB_A, SUB_B, SUB_C, SUB_D, SUB_E, SUB_H, SUB_L, SUB_M,
	SUI,
	SBB_A, SBB_B, SBB_C, SBB_D, SBB_E, SBB_H, SBB_L, SBB_M,
	SBI,

	INR_A, INR_B, INR_C, INR_D, INR_E, INR_H, INR_L, INR_M,
	DCR_A, DCR_B, DCR_C, DCR_D, DCR_E, DCR_H, DCR_L, DCR_M,

	INX_BC, INX_DE, INX_HL, INX_SP,
	DCX_BC, DCX_DE, DCX_HL, DCX_SP,

	DAD_BC, DAD_DE, DAD_HL, DAD_SP,

	//DAA,

	ANA_A, ANA_B, ANA_C, ANA_D, ANA_E, ANA_H, ANA_L, ANA_M,
	ANI,
	ORA_A, ORA_B, ORA_C, ORA_D, ORA_E, ORA_H, ORA_L, ORA_M,
	ORI,
	XRA_A, XRA_B, XRA_C, XRA_D, XRA_E, XRA_H, XRA_L, XRA_M,
	XRI,
	CMP_A, CMP_B, CMP_C, CMP_D, CMP_E, CMP_H, CMP_L, CMP_M,
	CPI,

	RLC,
	RRC,
	RAL,
	RAR,
	CMA,
	CMC,
	STC,
    RTC,

	JMP, JNZ, JZ, JNC, JC, JPO, JPE, JP, JM,
	CALL, CNZ, CZ, CNC, CC, CPO, CPE, CP, CM,
	RET, RNZ, RZ, RNC, RC, RPO, RPE, RP, RM,

	PCHL,

	PUSH_AF, PUSH_BC, PUSH_DE, PUSH_HL,
	POP_AF, POP_BC, POP_DE, POP_HL,

	XTHL, SPHL, HLSP,

	IIN, IOUT, HLT, EI, DI,

	// video memory instructions
	VFSA,  // store A as byte on the back buffer at address HL
	VFSAC, // store A as color on the back buffer at position HL
	VFLA,  // load to A a byte on the back buffer at address HL
	VFLAC, // load to A a color on the back buffer at position HL
	VFCLR, // clear (zero) the back buffer

	VSSA,  // store A as byte in sprite memory at address HL
	VSSAC, // store A as color in sprite memory at position HL
	VSLA,  // load to A a byte in sprite memory at address HL
	VSLAC, // load to A a color in sprite memory at position HL

	// video controller instructions
	VPRE,  // presents active buffer to the display
	VMODE, // switches video mode (0 for EGA and 1 for TEXT)
	VPAL,  // updates EGA pallette from the sprite memory at 0x0000 as 16 triples of RGB bytes

	// sprites
	VSS,  // set sprite (B = address, C = size, D = row length)
	VSDQ, // draw sprite opaque (HL = position)
	VSDT, // draw sprite transparent-0 (HL = position)
	VSDA, // draw sprite additive (HL = position)
	VSBA, // bitblit sprite AND (HL = position)
	VSBO, // bitblit sprite OR (HL = position)
	VSBX, // bitblit sprite XOR (HL = position)

    IR250,
    IR251,
    IR252,
    IR253,
    IR254,
    IR255
	
} XCM2_IRCODE;