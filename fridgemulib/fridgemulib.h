#include <fridge.h>
#ifdef FRIDGE_POSIT16_SUPPORT
    #include <posit.h>
#endif

#ifndef FRIDGEMULIB_H
#define FRIDGEMULIB_H

typedef struct FRIDGE_SYSTEM FRIDGE_SYSTEM;

typedef enum FRIDGE_VIDEO_MODE
{
    FRIDGE_VIDEO_EGA = FRIDGE_GPU_VIDEO_MODE_EGA,
    FRIDGE_VIDEO_TEXT = FRIDGE_GPU_VIDEO_MODE_TEXT
} FRIDGE_VIDEO_MODE;

typedef enum FRIDGE_VIDEO_FRAME_INDEX
{
    FRIDGE_VIDEO_FRAME_0 = FRIDGE_GPU_VIDEO_FRAME_0,
    FRIDGE_VIDEO_FRAME_1 = FRIDGE_GPU_VIDEO_FRAME_1
} FRIDGE_VIDEO_FRAME_INDEX;

typedef enum FRIDGE_VIDEO_FRAME_SWAP_MODE
{
    FRIDGE_VIDEO_SWAP_NONE = FRIDGE_GPU_VIDEO_SWAP_NONE,           // leaves buffer indices unchanged
    FRIDGE_VIDEO_SWAP_AUTO = FRIDGE_GPU_VIDEO_SWAP_AUTO,           // alternates between (visible=0, active=1) and (visible=1, active=0)
    FRIDGE_VIDEO_SWAP_MANUAL_00 = FRIDGE_GPU_VIDEO_SWAP_MANUAL_00, // sets (visible=0, active=0)
    FRIDGE_VIDEO_SWAP_MANUAL_01 = FRIDGE_GPU_VIDEO_SWAP_MANUAL_01, // sets (visible=0, active=1)
    FRIDGE_VIDEO_SWAP_MANUAL_10 = FRIDGE_GPU_VIDEO_SWAP_MANUAL_10, // sets (visible=1, active=0)
    FRIDGE_VIDEO_SWAP_MANUAL_11 = FRIDGE_GPU_VIDEO_SWAP_MANUAL_11, // sets (visible=1, active=1)
} FRIDGE_VIDEO_FRAME_SWAP_MODE;

typedef struct FRIDGE_IRDATA
{
    FRIDGE_WORD ircode;
    FRIDGE_WORD arg0;
    FRIDGE_WORD arg1;
} FRIDGE_IRDATA;

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

typedef enum FRIDGE_GPU_SPRITE_MODE
{
    FRIDGE_GPU_SPRITE_INVISIBLE,
    FRIDGE_GPU_SPRITE_OPAQUE,
    FRIDGE_GPU_SPRITE_TRANSPARENT0,
    FRIDGE_GPU_SPRITE_ADDITIVE,
    FRIDGE_GPU_SPRITE_SUBTRACTIVE,
    FRIDGE_GPU_SPRITE_BITWISE_AND,
    FRIDGE_GPU_SPRITE_BITWISE_OR,
    FRIDGE_GPU_SPRITE_BITWISE_XOR
} FRIDGE_GPU_SPRITE_MODE;

typedef struct FRIDGE_GPU_SPRITE
{
    FRIDGE_RAM_ADDR data;
    FRIDGE_WORD position_x;
    FRIDGE_WORD position_y;
    FRIDGE_WORD size_x;
    FRIDGE_WORD size_y;
    FRIDGE_GPU_SPRITE_MODE mode;
} FRIDGE_GPU_SPRITE;

typedef enum FRIDGE_GPU_RLE_MODE
{
    FRIDGE_GPU_RLE_OPAQUE,
    FRIDGE_GPU_RLE_TRANSPARENT0,
    FRIDGE_GPU_RLE_DITHERED,
    FRIDGE_GPU_RLE_ADDITIVE
} FRIDGE_GPU_RLE_MODE;

typedef enum FRIDGE_GPU_RLE_DIRECTION
{
    FRIDGE_GPU_RLE_RIGHT = 0x10,
    FRIDGE_GPU_RLE_LEFT = 0xf0,
    FRIDGE_GPU_RLE_DOWN = 0x01,
    FRIDGE_GPU_RLE_UP = 0x0f,
    FRIDGE_GPU_RLE_DOWN_RIGHT = 0x11,
    FRIDGE_GPU_RLE_DOWN_LEFT = 0xf1,
    FRIDGE_GPU_RLE_UP_RIGHT = 0x1f,
    FRIDGE_GPU_RLE_UP_LEFT = 0xff
} FRIDGE_GPU_RLE_DIRECTION;

typedef struct FRIDGE_GPU_RLE_LINE
{
    FRIDGE_RAM_ADDR data;
    FRIDGE_WORD nominal_length;
} FRIDGE_GPU_RLE_LINE;

typedef struct FRIDGE_GPU
{
    FRIDGE_WORD frame_a[FRIDGE_GPU_FRAME_BUFFER_SIZE];
    FRIDGE_WORD frame_b[FRIDGE_GPU_FRAME_BUFFER_SIZE];
    FRIDGE_VIDEO_MODE vmode;
    FRIDGE_VIDEO_FRAME_INDEX visible_frame;
    FRIDGE_VIDEO_FRAME_INDEX active_frame;
    FRIDGE_WORD frame_hor_offset;
    FRIDGE_WORD frame_ver_offset;
    FRIDGE_WORD palette[FRIDGE_GPU_PALETTE_SIZE];
    FRIDGE_WORD sprite_mem[FRIDGE_GPU_SPRITE_MEMORY_SIZE];
    FRIDGE_GPU_SPRITE sprite_list[FRIDGE_GPU_MAX_SPRITES];
    FRIDGE_GPU_RLE_LINE rle_line_list[FRIDGE_GPU_MAX_RLE_LINES];
    FRIDGE_SIZE_T frame_swaps; // VPRE instructions that changed the visible frame.

} FRIDGE_GPU;

void FRIDGE_gpu_reset                 (FRIDGE_GPU* gpu);
void FRIDGE_gpu_tick                  (FRIDGE_GPU* gpu);
FRIDGE_WORD* FRIDGE_gpu_visible_frame (FRIDGE_GPU* gpu);
FRIDGE_WORD* FRIDGE_gpu_active_frame  (FRIDGE_GPU* gpu);
FRIDGE_VIDEO_MODE FRIDGE_gpu_vmode    (FRIDGE_GPU* gpu);
void FRIDGE_gpu_render_ega_rgb8       (FRIDGE_GPU* gpu, unsigned char* pixels);
void FRIDGE_gpu_render_ega_rgb8_area  (FRIDGE_GPU* gpu, unsigned char* pixels,
                                       FRIDGE_WORD x, FRIDGE_WORD y, FRIDGE_WORD w, FRIDGE_WORD h, int pixelsRowOffset);
void FRIDGE_gpu_render_txt_rgb8       (FRIDGE_GPU* gpu, unsigned char* pixels, const FRIDGE_WORD* glyphBitmap);

// Advance both devices by elapsed milliseconds. A tick can deliver one timer IRQ.
void FRIDGE_sys_timer_tick(FRIDGE_SYSTEM* sys, FRIDGE_SIZE_T elapsed_ms);
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
    FRIDGE_WORD count;
    FRIDGE_WORD pending_interrupts; // The newest events whose interrupts have yet to be delivered.
} FRIDGE_KEYBOARD_CONTROLLER;

// OUT encodes the command in bits 7-4 and the argument in bits 3-0.
// NIB0 writes the least significant interval nibble.
typedef enum FRIDGE_TIMER_COMMAND
{
    FRIDGE_TIMER_NOP,
    FRIDGE_TIMER_RESET,
    FRIDGE_TIMER_SET_INTERVAL_NIB0,
    FRIDGE_TIMER_SET_INTERVAL_NIB1,
    FRIDGE_TIMER_SET_INTERVAL_NIB2,
    FRIDGE_TIMER_SET_INTERVAL_NIB3,
    FRIDGE_TIMER_GET_COUNTER_HIGH,
    FRIDGE_TIMER_GET_COUNTER_LOW,
} FRIDGE_TIMER_COMMAND;

// Zero-initialize each device to start with counting disabled.
typedef struct FRIDGE_TIMER
{
    FRIDGE_DWORD counter;
    FRIDGE_DWORD interval;
    FRIDGE_WORD read_high; // IN reads the selected byte of the current counter.
} FRIDGE_TIMER;

#ifdef FRIDGE_POSIT16_SUPPORT
    typedef struct FRIDGE_PAM16
    {
        Posit16 stack[FRIDGE_PAM16_STACK_SIZE];
        FRIDGE_WORD sp;
        Posit16Environment env;
    } FRIDGE_PAM16;

    void FRIDGE_pam16_reset(FRIDGE_SYSTEM* sys, unsigned char es);
    void FRIDGE_pam16_push(FRIDGE_SYSTEM* sys, Posit16 data);
    Posit16 FRIDGE_pam16_pop(FRIDGE_SYSTEM* sys);
    void FRIDGE_pam16_add(FRIDGE_SYSTEM* sys);
    void FRIDGE_pam16_sub(FRIDGE_SYSTEM* sys);
    void FRIDGE_pam16_mul(FRIDGE_SYSTEM* sys);
    void FRIDGE_pam16_div(FRIDGE_SYSTEM* sys);
    void FRIDGE_pam16_fmadd(FRIDGE_SYSTEM* sys);
    void FRIDGE_pam16_pack(FRIDGE_SYSTEM* sys);
    void FRIDGE_pam16_unpack(FRIDGE_SYSTEM* sys);
#endif

typedef struct FRIDGE_SYSTEM
{
    FRIDGE_CPU* cpu;
    FRIDGE_GPU* gpu;
    FRIDGE_ROM* rom;
    FRIDGE_KEYBOARD_CONTROLLER* kbrd;
    FRIDGE_TIMER* timer;
    FRIDGE_TIMER* stopwatch;
#ifdef FRIDGE_POSIT16_SUPPORT
    FRIDGE_PAM16* pam;
#endif
} FRIDGE_SYSTEM;



#endif
