#ifndef FRIDGE_H
#define FRIDGE_H

typedef unsigned char FRIDGE_WORD;
typedef unsigned short FRIDGE_DWORD;
typedef unsigned short FRIDGE_RAM_ADDR;
typedef unsigned short FRIDGE_ROM_ADDR;
typedef unsigned int FRIDGE_SIZE_T;

#define FRIDGE_ASCENDING_STACK

#define FRIDGE_RAM_SIZE 0x10000 // bytes
#define FRIDGE_MAX_DWORD 0xffff
#define FRIDGE_ROM_MAX_SEGMENTS 0x10000
#define FRIDGE_ROM_SEGMENT_SIZE 0x100 // bytes (16Mb maximum)
#define FRIDGE_WORD_BITS 8 // number of bits in a word
#define FRIDGE_MAX_IO_DEVICES 4
#define FRIDGE_MAX_INSTRUCTIONS 0x100
#define FRIDGE_GPU_BUS_SIZE 4
#define FRIDGE_GPU_FRAME_EGA_WIDTH 240
#define FRIDGE_GPU_FRAME_EGA_HEIGHT 160
#define FRIDGE_GPU_SPRITE_MEMORY_SIZE 0x10000 // bytes
#define FRIDGE_GPU_MAX_SPRITES 64
#define FRIDGE_GPU_MAX_SPRITES_PER_PIXEL 4
#define FRIDGE_GPU_PALETTE_SIZE 48 // 3*16
#define FRIDGE_GPU_FRAME_BUFFER_SIZE 19200 // FRIDGE_GPU_FRAME_EGA_WIDTH*FRIDGE_GPU_FRAME_EGA_HEIGHT >> 1; // 240x160x4 bits
#define FRIDGE_BOOT_SECTION_INDEX_ADDRESS 0x0003
#define FRIDGE_EXECUTABLE_OFFSET 0x0100
#define FRIDGE_IRQ_SYS_TIMER 0x0004
#define FRIDGE_IRQ_KEYBOARD_PRESS 0x0007
#define FRIDGE_IRQ_KEYBOARD_RELEASE 0x000a
#define FRIDGE_KEYBOARD_BUFFER_SIZE 32
#define FRIDGE_KEYBOARD_KEY_STATE_MASK 0x80
#define FRIDGE_KEYBOARD_KEY_CODE_MASK 0x7f

#define FRIDGE_DWORD_HL(H, L) (((FRIDGE_DWORD)H) << FRIDGE_WORD_BITS) | L // big endian
#define FRIDGE_HIGH_WORD(DW) (FRIDGE_WORD)(DW >> FRIDGE_WORD_BITS)
#define FRIDGE_LOW_WORD(DW) (FRIDGE_WORD)DW
#define FRIDGE_DWORD_TO_WORDS(DW) FRIDGE_HIGH_WORD(DW), FRIDGE_LOW_WORD(DW)
#define FRIDGE_HIGHBIT_MASK 0x80
#define FRIDGE_LOWBIT_MASK 0x01

#define FRIDGE_GPU_WORD(LP, RP) (FRIDGE_WORD)( ((LP & 0x0f) << 4) | (RP & 0x0f) )
#define FRIDGE_GPU_LEFT_PIXEL(W) (FRIDGE_WORD)( (W & 0xf0) >> 4 )
#define FRIDGE_GPU_RIGHT_PIXEL(W) (FRIDGE_WORD)(W & 0x0f)

#define FRIDGE_FLAG_SIGN_MASK   0x80
#define FRIDGE_FLAG_ZERO_MASK   0x40
#define FRIDGE_FLAG_PANIC_MASK  0x20
#define FRIDGE_FLAG_AUX_MASK    0x10
#define FRIDGE_FLAG_PARITY_MASK 0x04
#define FRIDGE_FLAG_CARRY_MASK  0x01

#define FRIDGE_DEV_ROM_RESET_ID 0x01
#define FRIDGE_DEV_ROM_ID 0x02
#define FRIDGE_DEV_KEYBOARD_ID 0x03

typedef enum FRIDGE_IRCODE {

    NOP,

    MOV_AB, MOV_AC, MOV_AD, MOV_AE, MOV_AH, MOV_AL, MOV_AM,
    MOV_BA, MOV_BC, MOV_BD, MOV_BE, MOV_BH, MOV_BL, MOV_BM,
    MOV_CA, MOV_CB, MOV_CD, MOV_CE, MOV_CH, MOV_CL, MOV_CM,
    MOV_DA, MOV_DB, MOV_DC, MOV_DE, MOV_DH, MOV_DL, MOV_DM,
    MOV_EA, MOV_EB, MOV_EC, MOV_ED, MOV_EH, MOV_EL, MOV_EM,
    MOV_HA, MOV_HB, MOV_HC, MOV_HD, MOV_HE, MOV_HL, MOV_HM,
    MOV_LA, MOV_LB, MOV_LC, MOV_LD, MOV_LE, MOV_LH, MOV_LM,
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

    IIN, IOUT, HLT, EI, DI, // 231 instructions

    // video controller instructions
    VPRE,  // swaps back and visible buffers and sets buffer offset (position HL)
    VMODE, // switches video mode (A = 0 for EGA and 1 for TEXT)
    VPAL,  // updates EGA pallette color (color A, rgb B, C, D)

    // back buffer instructions
    VFSA,  // store A as byte on the back buffer at address HL
    VFSI,  // store two bytes (arg0, arg1) on the back buffer at address HL,
           // then increase HL by 2
    VFSAC, // store A as color on the back buffer at position HL
    VFLA,  // load to A a byte on the back buffer at address HL
    VFLAC, // load to A a color on the back buffer at position HL
    VS2F,  // (address BC, address HL)
           // copies one byte (two pixels) from sprite memory at address HL
           // to backbuffer at address BC

    // sprite memory instructions
    VSSA,  // store A as byte in sprite memory at address HL
    VSSI,  // store two bytes (arg0, arg1) in sprite memory at address HL,
           // then increase HL by 2
    VSLA,  // load to A a byte in sprite memory at address HL


    // sprites
    VSS, // set sprite (index A, width B, height C, address HL)
    VSD, // draw sprite (index A, mode B, position HL)
         // mode:
         // 0 - invisible
         // 1 - opaque
         // 2 - transparent0
         // 3 - additive
         // 4 - subtractive
         // 5 - bitwise and
         // 6 - bitwise or
         // 7 - bitwise xor

    IR246,
    IR247,
    IR248,
    IR249,
    IR250,
    IR251,
    IR252,
    IR253,
    IR254,
    IR255

} FRIDGE_IRCODE;

const FRIDGE_WORD FRIDGE_gpu_default_palette[FRIDGE_GPU_PALETTE_SIZE] =
{
    0x00, 0x00, 0x00, // black
    0x00, 0x00, 0x80, // blue
    0x00, 0x80, 0x00, // green
    0x00, 0x80, 0x80, // cyan
    0x80, 0x00, 0x00, // red
    0x80, 0x00, 0x80, // magenta
    0x80, 0x40, 0x00, // brown
    0x80, 0x80, 0x80, // light gray
    0x40, 0x40, 0x40, // dark gray
    0x00, 0x40, 0xff, // bright blue
    0x40, 0xff, 0x40, // bright green
    0x40, 0xff, 0xff, // bright cyan
    0xff, 0x40, 0x40, // bright red
    0xff, 0x40, 0xff, // bright magenta
    0xff, 0xff, 0x40, // bright yellow
    0xff, 0xff, 0xff, // white
};

#endif
