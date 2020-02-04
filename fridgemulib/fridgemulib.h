#ifndef FRIDGEMULIB_H
#define FRIDGEMULIB_H

typedef unsigned char FRIDGE_WORD;
typedef unsigned short FRIDGE_DWORD;
typedef unsigned short FRIDGE_RAM_ADDR;
typedef unsigned short FRIDGE_ROM_ADDR;
typedef unsigned int FRIDGE_SIZE_T;
typedef struct FRIDGE_SYSTEM FRIDGE_SYSTEM;

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
#define FRIDGE_GPU_SPRITE_WIDTH 16
#define FRIDGE_GPU_SPRITE_HEIGHT 16
#define FRIDGE_GPU_SPRITE_SIZE 0x80 // 128 bytes (16x16 pixels)
#define FRIDGE_GPU_SPRITE_MEMORY_SIZE 0x10000 // bytes
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

#define FRIDGE_GRAM_WORD(LP, RP) (FRIDGE_WORD)( ((LP & 0x0f) << 4) | (RP & 0x0f) )
#define FRIDGE_GRAM_LEFT_PIXEL(W) (FRIDGE_WORD)( (W & 0xf0) >> 4 )
#define FRIDGE_GRAM_RIGHT_PIXEL(W) (FRIDGE_WORD)(W & 0x0f)

#define FRIDGE_FLAG_SIGN_MASK   0x80
#define FRIDGE_FLAG_ZERO_MASK   0x40
#define FRIDGE_FLAG_PANIC_MASK  0x20
#define FRIDGE_FLAG_AUX_MASK    0x10
#define FRIDGE_FLAG_PARITY_MASK 0x04
#define FRIDGE_FLAG_CARRY_MASK  0x01

#define FRIDGE_DEV_ROM_RESET_ID 0x01
#define FRIDGE_DEV_ROM_ID 0x02
#define FRIDGE_DEV_KEYBOARD_ID 0x03

typedef enum FRIDGE_VIDEO_MODE
{
    FRIDGE_VIDEO_EGA,
    FRIDGE_VIDEO_TEXT
} FRIDGE_VIDEO_MODE;

typedef enum FRIDGE_VIDEO_FRAME
{
    FRIDGE_VIDEO_FRAME_A,
    FRIDGE_VIDEO_FRAME_B
} FRIDGE_VIDEO_FRAME;

typedef struct FRIDGE_IRDATA
{
    FRIDGE_WORD ircode;
    FRIDGE_WORD arg0;
    FRIDGE_WORD arg1;
} FRIDGE_IRDATA;

typedef enum FRIDGE_IRCODE {

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
    VFCLR, // fill back buffer with byte A

    VSSA,  // store A as byte in sprite memory at address HL
    VSSAC, // store A as color in sprite memory at position HL
    VSLA,  // load to A a byte in sprite memory at address HL
    VSLAC, // load to A a color in sprite memory at position HL

    // video controller instructions
    VPRE,  // swaps back and visible buffers
    VMODE, // switches video mode (0 for EGA and 1 for TEXT)
    VPAL,  // updates EGA pallette from the sprite memory at HL address as 16 triples of RGB bytes

    // sprites
    VSS,  // set sprite (HL = address)
    VSDQ, // draw sprite opaque (HL = position)
    VSDT, // draw sprite transparent-0 (HL = position)
    VSBA, // bitblit sprite AND (HL = position)
    VSBO, // bitblit sprite OR (HL = position)
    VSBX, // bitblit sprite XOR (HL = position)
    VSRF, // read sprite from visible buffer (HL = position)

    IR250,
    IR251,
    IR252,
    IR253,
    IR254,
    IR255

} FRIDGE_IRCODE;

typedef enum FRIDGE_ROM_STATE
{
    FRIDGE_ROM_SELECT_MODE,
    FRIDGE_ROM_SEGLOW,
    FRIDGE_ROM_SEGHIGH,
    FRIDGE_ROM_OPERATE,
    FRIDGE_ROM_STREAMING,
} FRIDGE_ROM_STATE;

typedef enum FRIDGE_ROM_MODE
{
    FRIDGE_ROM_MODE_IDLE,
    FRIDGE_ROM_MODE_STORE,
    FRIDGE_ROM_MODE_LOAD
} FRIDGE_ROM_MODE;

typedef enum FRIDGE_CPU_STATE
{
    FRIDGE_CPU_ACTIVE,
    FRIDGE_CPU_HALTED
} FRIDGE_CPU_STATE;

typedef enum FRIDGE_CPU_INTERRUPTS_STATE
{
    FRIDGE_CPU_INTERRUPTS_ENABLED,
    FRIDGE_CPU_INTERRUPTS_READING_ARG0, // intermediate state for pcRead
    FRIDGE_CPU_INTERRUPTS_READING_ARG1, // intermediate state for pcRead
    FRIDGE_CPU_INTERRUPTS_DISABLED
} FRIDGE_CPU_INTERRUPTS_STATE;

typedef FRIDGE_WORD (*FRIDGE_INPUT_CALLBACK)(FRIDGE_SYSTEM*);
typedef void (*FRIDGE_OUTPUT_CALLBACK)(FRIDGE_SYSTEM*, FRIDGE_WORD);

typedef struct FRIDGE_CPU
{
    FRIDGE_CPU_STATE state; // HLT
    FRIDGE_RAM_ADDR PC;     // program counter
    FRIDGE_RAM_ADDR SP;     // stack pointer
    FRIDGE_WORD rA;         // registers
    FRIDGE_WORD rB;
    FRIDGE_WORD rC;
    FRIDGE_WORD rD;
    FRIDGE_WORD rE;
    FRIDGE_WORD rH;
    FRIDGE_WORD rL;
    FRIDGE_WORD rF;
    FRIDGE_WORD ram[FRIDGE_RAM_SIZE];

    FRIDGE_CPU_INTERRUPTS_STATE inte;
    FRIDGE_WORD inteArg0;
    FRIDGE_WORD inteArg1;
    //FRIDGE_WORD iobus[FRIDGE_MAX_IO_DEVICES];
    FRIDGE_INPUT_CALLBACK input_dev[FRIDGE_MAX_IO_DEVICES];
    FRIDGE_OUTPUT_CALLBACK output_dev[FRIDGE_MAX_IO_DEVICES];
    FRIDGE_WORD gpubus[FRIDGE_GPU_BUS_SIZE];

} FRIDGE_CPU;

void FRIDGE_cpu_reset              (FRIDGE_CPU* cpu);
void FRIDGE_sys_tick               (FRIDGE_SYSTEM* sys);
void FRIDGE_cpu_interrupt          (FRIDGE_SYSTEM* sys, FRIDGE_WORD ircode, FRIDGE_WORD arg0, FRIDGE_WORD arg1);
void FRIDGE_cpu_ram_read           (FRIDGE_CPU* cpu, FRIDGE_WORD* buffer, FRIDGE_RAM_ADDR position, FRIDGE_SIZE_T size);
void FRIDGE_cpu_ram_write          (FRIDGE_CPU* cpu, FRIDGE_WORD* buffer, FRIDGE_RAM_ADDR position, FRIDGE_SIZE_T size);
FRIDGE_WORD FRIDGE_cpu_flag_SIGN   (const FRIDGE_CPU* cpu);
FRIDGE_WORD FRIDGE_cpu_flag_ZERO   (const FRIDGE_CPU* cpu);
FRIDGE_WORD FRIDGE_cpu_flag_AUX    (const FRIDGE_CPU* cpu);
FRIDGE_WORD FRIDGE_cpu_flag_PANIC  (const FRIDGE_CPU* cpu);
FRIDGE_WORD FRIDGE_cpu_flag_PARITY (const FRIDGE_CPU* cpu);
FRIDGE_WORD FRIDGE_cpu_flag_CARRY  (const FRIDGE_CPU* cpu);
FRIDGE_DWORD FRIDGE_cpu_pair_BC    (const FRIDGE_CPU* cpu);
FRIDGE_DWORD FRIDGE_cpu_pair_DE    (const FRIDGE_CPU* cpu);
FRIDGE_DWORD FRIDGE_cpu_pair_HL    (const FRIDGE_CPU* cpu);

typedef struct FRIDGE_GPU
{
    FRIDGE_WORD sprite[FRIDGE_GPU_SPRITE_MEMORY_SIZE];
    FRIDGE_WORD frame_a[FRIDGE_GPU_FRAME_BUFFER_SIZE];
    FRIDGE_WORD frame_b[FRIDGE_GPU_FRAME_BUFFER_SIZE];
    FRIDGE_VIDEO_MODE vmode;
    FRIDGE_VIDEO_FRAME vframe;

} FRIDGE_GPU;

void FRIDGE_gpu_reset                 (FRIDGE_GPU* gpu);
void FRIDGE_gpu_tick                  (FRIDGE_GPU* gpu);
FRIDGE_WORD* FRIDGE_gpu_visible_frame (const FRIDGE_GPU* gpu);

void FRIDGE_sys_timer_tick(FRIDGE_SYSTEM* sys);
void FRIDGE_keyboard_press(FRIDGE_SYSTEM* sys, FRIDGE_WORD key);
void FRIDGE_keyboard_release(FRIDGE_SYSTEM* sys, FRIDGE_WORD key);

typedef struct FRIDGE_ROM_SEGMENT
{
    FRIDGE_WORD data[FRIDGE_ROM_SEGMENT_SIZE];
} FRIDGE_ROM_SEGMENT;

typedef struct FRIDGE_ROM
{
    FRIDGE_ROM_SEGMENT* segments;
    FRIDGE_SIZE_T segments_count;
    FRIDGE_ROM_STATE state;
    FRIDGE_ROM_ADDR active_segment;
    FRIDGE_ROM_MODE mode;
    FRIDGE_SIZE_T stream_position;
} FRIDGE_ROM;

typedef struct FRIDGE_KEYBOARD_CONTROLLER
{
    FRIDGE_WORD key_buffer[FRIDGE_KEYBOARD_BUFFER_SIZE];
    FRIDGE_WORD input_index;
    FRIDGE_WORD output_index;
} FRIDGE_KEYBOARD_CONTROLLER;

typedef struct FRIDGE_SYSTEM
{
    FRIDGE_CPU* cpu;
    FRIDGE_GPU* gpu;
    FRIDGE_ROM* rom;
    FRIDGE_KEYBOARD_CONTROLLER* kbrd;
} FRIDGE_SYSTEM;

#endif
