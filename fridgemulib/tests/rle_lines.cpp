extern "C" {
#include "fridgemulib.h"
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_artifacts.h"

static void require(bool passed, const char* message)
{
    if (!passed)
    {
        fprintf(stderr, "%dx%d RLE: %s\n", FRIDGE_GPU_FRAME_EGA_WIDTH,
                FRIDGE_GPU_FRAME_EGA_HEIGHT, message);
        exit(1);
    }
}

static void save_screen(FRIDGE_SYSTEM* sys, const char* scene, const FRIDGE_WORD* frame)
{
    // Present a copy of the active buffer without changing the test's GPU state.
    FRIDGE_GPU capture = *sys->gpu;
    memcpy(FRIDGE_gpu_active_frame(&capture), frame, FRIDGE_GPU_FRAME_BUFFER_SIZE);
    capture.visible_frame = capture.active_frame;
    std::vector<unsigned char> rgb(FRIDGE_GPU_FRAME_EGA_WIDTH*FRIDGE_GPU_FRAME_EGA_HEIGHT*3);
    FRIDGE_gpu_render_ega_rgb8(&capture, rgb.data());
    char name[80];
    snprintf(name, sizeof(name), "rle-%dx%d-%s", FRIDGE_GPU_FRAME_EGA_WIDTH,
             FRIDGE_GPU_FRAME_EGA_HEIGHT, scene);
    require(save_test_screen(name, rgb.data(), FRIDGE_GPU_FRAME_EGA_WIDTH,
                            FRIDGE_GPU_FRAME_EGA_HEIGHT), "Could not save screen");
}

static void check_frame(FRIDGE_SYSTEM* sys, const FRIDGE_WORD* expected, const char* message)
{
    FRIDGE_WORD* actual = FRIDGE_gpu_active_frame(sys->gpu);
    if (memcmp(actual, expected, FRIDGE_GPU_FRAME_BUFFER_SIZE) == 0)
        return;
    fprintf(stderr, "Line %u, output length %u, direction 0x%02x, mode %u, position (%u,%u)\n",
            sys->cpu->rA, sys->cpu->rB, sys->cpu->rC, sys->cpu->rD,
            sys->cpu->rH, sys->cpu->rL);
    save_screen(sys, "failure-actual", actual);
    save_screen(sys, "failure-expected", expected);
    require(false, message);
}

static void execute(FRIDGE_SYSTEM* sys, FRIDGE_WORD opcode)
{
    FRIDGE_CPU* cpu = sys->cpu;
    const FRIDGE_WORD registers[] = {
        cpu->rA, cpu->rB, cpu->rC, cpu->rD, cpu->rE, cpu->rH, cpu->rL, cpu->rF
    };
    cpu->PC = FRIDGE_EXECUTABLE_OFFSET;
    cpu->ram[cpu->PC] = opcode;
    FRIDGE_sys_tick(sys);
    require(cpu->PC == FRIDGE_EXECUTABLE_OFFSET + 1, "Opcode was not dispatched");
    if (!FRIDGE_cpu_flag_PANIC(cpu))
    {
        const FRIDGE_WORD after[] = {
            cpu->rA, cpu->rB, cpu->rC, cpu->rD, cpu->rE, cpu->rH, cpu->rL, cpu->rF
        };
        require(memcmp(registers, after, sizeof(registers)) == 0,
                "Video instruction changed CPU registers or flags");
    }
}

static void setup(FRIDGE_SYSTEM* sys, unsigned slot, unsigned nominal, unsigned address)
{
    sys->cpu->rA = slot;
    sys->cpu->rB = nominal;
    sys->cpu->rH = FRIDGE_HIGH_WORD(address);
    sys->cpu->rL = FRIDGE_LOW_WORD(address);
    execute(sys, VRS);
}

static void test_setup(FRIDGE_SYSTEM* sys)
{
    for (unsigned i = 0; i < FRIDGE_GPU_MAX_RLE_LINES; ++i)
        require(sys->gpu->rle_line_list[i].nominal_length == 0,
                "Reset did not disable a line");
    setup(sys, 0, 17, 0x1234);
    setup(sys, 255, 255, 0xffff);
    require(sys->gpu->rle_line_list[0].data == 0x1234 &&
            sys->gpu->rle_line_list[0].nominal_length == 17,
            "VRS did not store the source address and length");
    require(sys->gpu->rle_line_list[255].data == 0xffff &&
            sys->gpu->rle_line_list[255].nominal_length == 255,
            "VRS did not support the full index range");
    setup(sys, 0, 0, 0);
    require(sys->gpu->rle_line_list[0].nominal_length == 0 &&
            sys->gpu->rle_line_list[255].nominal_length == 255,
            "Disabling one line affected another slot");
    FRIDGE_gpu_reset(sys->gpu);
    for (unsigned i = 0; i < FRIDGE_GPU_MAX_RLE_LINES; ++i)
        require(sys->gpu->rle_line_list[i].data == 0 &&
                sys->gpu->rle_line_list[i].nominal_length == 0,
                "Reset left stale line setup state");
}

static void draw(FRIDGE_SYSTEM* sys, unsigned slot, unsigned visible,
                 unsigned direction, unsigned mode, unsigned x, unsigned y)
{
    sys->cpu->rA = slot;
    sys->cpu->rB = visible;
    sys->cpu->rC = direction;
    sys->cpu->rD = mode;
    sys->cpu->rH = x;
    sys->cpu->rL = y;
    execute(sys, VRD);
}

// Reference uses direct multiplication/division, independently of both GPU accumulators.
static void reference_draw(FRIDGE_WORD* frame, const FRIDGE_WORD* source,
                           unsigned nominal, unsigned visible, unsigned direction,
                           unsigned mode, int start_x, int start_y)
{
    int dx = direction >> 4;
    int dy = direction & 15;
    if (dx >= 8) dx -= 16;
    if (dy >= 8) dy -= 16;
    int ax = abs(dx), ay = abs(dy);
    int sx = dx < 0 ? -1 : 1;
    int sy = dy < 0 ? -1 : 1;
    for (unsigned i = 0; i < visible; ++i)
    {
        int x = start_x, y = start_y;
        if (ax >= ay && ax > 0)
        {
            x += sx*(int)i;
            y += sy*((int)i*ay + ax/2)/ax;
        }
        else if (ay > 0)
        {
            y += sy*(int)i;
            x += sx*((int)i*ax + ay/2)/ay;
        }
        if (x < 0 || x >= FRIDGE_GPU_FRAME_EGA_WIDTH ||
            y < 0 || y >= FRIDGE_GPU_FRAME_EGA_HEIGHT)
            continue;
        unsigned color = source[i*nominal/visible];
        if (mode == FRIDGE_GPU_RLE_TRANSPARENT0 && color == 0)
            continue;
        if (mode == FRIDGE_GPU_RLE_DITHERED && (x+y)%2 != 0)
            continue;
        unsigned address = (y*FRIDGE_GPU_FRAME_EGA_WIDTH+x)/2;
        unsigned shift = x%2 ? 0 : 4;
        if (mode == FRIDGE_GPU_RLE_ADDITIVE)
            color = (color + ((frame[address] >> shift) & 15)) % 16;
        frame[address] = (frame[address] & ~(15 << shift)) | (color << shift);
    }
}

static void test_scaling(FRIDGE_SYSTEM* sys)
{
    FRIDGE_WORD source[255];
    FRIDGE_WORD expected[FRIDGE_GPU_FRAME_BUFFER_SIZE];
    FRIDGE_WORD* frame = FRIDGE_gpu_active_frame(sys->gpu);
    memset(FRIDGE_gpu_visible_frame(sys->gpu), 0xa5, FRIDGE_GPU_FRAME_BUFFER_SIZE);
    for (unsigned nominal = 1; nominal <= 255; ++nominal)
    {
        unsigned offset = 0, token = 0;
        while (offset < nominal)
        {
            unsigned run = (nominal + token*7) % 16 + 1;
            unsigned color = (token*3 + nominal) % 16;
            sys->gpu->sprite_mem[0x1000 + token++] = FRIDGE_GPU_WORD(color, run-1);
            for (unsigned i = 0; i < run && offset < nominal; ++i)
                source[offset++] = color;
        }
        setup(sys, 255, nominal, 0x1000);
        for (unsigned visible = 1; visible <= 255; ++visible)
        {
            memset(frame, 0xee, FRIDGE_GPU_FRAME_BUFFER_SIZE);
            memset(expected, 0xee, sizeof(expected));
            reference_draw(expected, source, nominal, visible,
                           FRIDGE_GPU_RLE_RIGHT, FRIDGE_GPU_RLE_OPAQUE, 1, 1);
            draw(sys, 255, visible, FRIDGE_GPU_RLE_RIGHT, FRIDGE_GPU_RLE_OPAQUE, 1, 1);
            require(!FRIDGE_cpu_flag_PANIC(sys->cpu), "Valid scaling caused a panic");
            check_frame(sys, expected, "Scaling disagreed with floor(output_index * N / L)");
        }
    }
    for (unsigned i = 0; i < FRIDGE_GPU_FRAME_BUFFER_SIZE; ++i)
        require(FRIDGE_gpu_visible_frame(sys->gpu)[i] == 0xa5,
                "VRD changed the visible framebuffer instead of the active one");
}

static void test_directions_and_modes(FRIDGE_SYSTEM* sys)
{
    const FRIDGE_WORD source[] = {0, 0, 3, 3, 3, 15, 1, 1, 9, 9, 9, 9, 7};
    const FRIDGE_WORD tokens[] = {0x01, 0x32, 0xf0, 0x11, 0x93, 0x70};
    FRIDGE_WORD expected[FRIDGE_GPU_FRAME_BUFFER_SIZE];
    FRIDGE_WORD* frame = FRIDGE_gpu_active_frame(sys->gpu);
    memcpy(sys->gpu->sprite_mem + 0x2000, tokens, sizeof(tokens));
    setup(sys, 42, sizeof(source), 0x2000);
    // Every signed-nibble vector, including stationary, steep, shallow and reversed.
    for (unsigned direction = 0; direction <= 255; ++direction)
        for (unsigned mode = 0; mode <= FRIDGE_GPU_RLE_ADDITIVE; ++mode)
        {
            memset(frame, 0xab, FRIDGE_GPU_FRAME_BUFFER_SIZE);
            memset(expected, 0xab, sizeof(expected));
            reference_draw(expected, source, sizeof(source), 31, direction, mode, 8, 9);
            draw(sys, 42, 31, direction, mode, 8, 9);
            require(!FRIDGE_cpu_flag_PANIC(sys->cpu), "Valid direction or mode caused a panic");
            check_frame(sys, expected,
                        "Slope, blending, dithering or clipping disagreed with reference");
        }
    // Clipping must preserve scaling phase, including when a line enters the frame.
    const unsigned starts[][3] = {
        {254, 1, FRIDGE_GPU_RLE_LEFT},
        {1, 254, FRIDGE_GPU_RLE_UP},
        {FRIDGE_GPU_FRAME_EGA_WIDTH-2, FRIDGE_GPU_FRAME_EGA_HEIGHT-2, 0x21}
    };
    for (const auto& start : starts)
    {
        memset(frame, 0xab, FRIDGE_GPU_FRAME_BUFFER_SIZE);
        memset(expected, 0xab, sizeof(expected));
        reference_draw(expected, source, sizeof(source), 100, start[2],
                       FRIDGE_GPU_RLE_DITHERED, start[0], start[1]);
        draw(sys, 42, 100, start[2], FRIDGE_GPU_RLE_DITHERED, start[0], start[1]);
        check_frame(sys, expected, "Clipping changed source sampling or checkerboard alignment");
    }
    // VRS definitions remain reusable, and VRD follows framebuffer selection.
    sys->gpu->active_frame = FRIDGE_VIDEO_FRAME_0;
    memset(sys->gpu->frame_a, 0xab, FRIDGE_GPU_FRAME_BUFFER_SIZE);
    memset(expected, 0xab, sizeof(expected));
    reference_draw(expected, source, sizeof(source), 31, FRIDGE_GPU_RLE_DOWN,
                   FRIDGE_GPU_RLE_OPAQUE, 5, 4);
    draw(sys, 42, 31, FRIDGE_GPU_RLE_DOWN, FRIDGE_GPU_RLE_OPAQUE, 5, 4);
    check_frame(sys, expected, "VRD did not follow the active framebuffer selection");
    sys->gpu->active_frame = FRIDGE_VIDEO_FRAME_1;
}

static void test_bounds(FRIDGE_SYSTEM* sys)
{
    FRIDGE_WORD* frame = FRIDGE_gpu_active_frame(sys->gpu);
    memset(frame, 0xee, FRIDGE_GPU_FRAME_BUFFER_SIZE);
    draw(sys, 12, 255, FRIDGE_GPU_RLE_RIGHT, FRIDGE_GPU_RLE_OPAQUE, 0, 0);
    setup(sys, 12, 0, 0xffff);
    draw(sys, 12, 255, FRIDGE_GPU_RLE_RIGHT, FRIDGE_GPU_RLE_OPAQUE, 0, 0);
    setup(sys, 12, 255, 0xffff);
    draw(sys, 12, 0, FRIDGE_GPU_RLE_RIGHT, FRIDGE_GPU_RLE_OPAQUE, 0, 0);
    require(!FRIDGE_cpu_flag_PANIC(sys->cpu), "Zero length caused a source read or panic");
    for (unsigned i = 0; i < FRIDGE_GPU_FRAME_BUFFER_SIZE; ++i)
        require(frame[i] == 0xee, "Disabled or zero-length line changed the frame");

    // A final 16-pixel token at 0xffff must not fetch another byte or wrap.
    sys->gpu->sprite_mem[0xffff] = 0xaf;
    setup(sys, 12, 16, 0xffff);
    draw(sys, 12, 16, FRIDGE_GPU_RLE_RIGHT, FRIDGE_GPU_RLE_OPAQUE, 0, 0);
    require(!FRIDGE_cpu_flag_PANIC(sys->cpu), "Last sprite-memory byte was rejected");
    for (unsigned i = 0; i < 8; ++i)
        require(frame[i] == 0xaa, "Run length 16 decoded incorrectly");
    require(frame[8] == 0xee, "Drawing exceeded the visible length");

    // Nominal length truncates a token even under maximum enlargement.
    setup(sys, 12, 1, 0xffff);
    draw(sys, 12, 255, 0, FRIDGE_GPU_RLE_ADDITIVE, 20, 0);
    require(!FRIDGE_cpu_flag_PANIC(sys->cpu), "Truncated token read past sprite memory");
    require(frame[10] == 0x4e, "Maximum enlargement emitted the wrong number of pixels");

    setup(sys, 12, 17, 0xffff);
    draw(sys, 12, 17, FRIDGE_GPU_RLE_RIGHT, FRIDGE_GPU_RLE_OPAQUE, 0, 0);
    require(FRIDGE_cpu_flag_PANIC(sys->cpu) && sys->cpu->state == FRIDGE_CPU_HALTED,
            "Exhausting sprite memory did not halt safely");
    require(frame[8] == 0xee, "Source cursor wrapped to address zero");
    FRIDGE_cpu_reset(sys->cpu);
    draw(sys, 12, 1, FRIDGE_GPU_RLE_RIGHT, 4, 0, 0);
    require(FRIDGE_cpu_flag_PANIC(sys->cpu), "Invalid blend mode was accepted");
    FRIDGE_cpu_reset(sys->cpu);
}

static void test_screens(FRIDGE_SYSTEM* sys)
{
    FRIDGE_cpu_reset(sys->cpu);
    FRIDGE_gpu_reset(sys->gpu);
    FRIDGE_WORD* frame = FRIDGE_gpu_active_frame(sys->gpu);
    FRIDGE_WORD expected[FRIDGE_GPU_FRAME_BUFFER_SIZE];
    FRIDGE_WORD source[64];
    for (unsigned i = 0; i < 64; ++i)
        source[i] = i/4;
    for (unsigned i = 0; i < 16; ++i)
        sys->gpu->sprite_mem[0x3000+i] = FRIDGE_GPU_WORD(i, 3);
    setup(sys, 0, 64, 0x3000);

    // Eight bands show the same source compressed, unchanged and stretched.
    memset(frame, 0x11, FRIDGE_GPU_FRAME_BUFFER_SIZE);
    memset(expected, 0x11, sizeof(expected));
    const unsigned lengths[] = {16, 32, 48, 64, 96, 128, 160, 192};
    for (unsigned band = 0; band < sizeof(lengths)/sizeof(lengths[0]); ++band)
        for (unsigned row = 0; row < 8; ++row)
        {
            unsigned y = 12 + band*16 + row;
            reference_draw(expected, source, 64, lengths[band], FRIDGE_GPU_RLE_RIGHT,
                           FRIDGE_GPU_RLE_OPAQUE, 8, y);
            draw(sys, 0, lengths[band], FRIDGE_GPU_RLE_RIGHT, FRIDGE_GPU_RLE_OPAQUE, 8, y);
        }
    check_frame(sys, expected, "Scaling screen disagreed with reference");
    save_screen(sys, "scaling", frame);

    // Rays include all octants and 1:1, 2:1, 3:1, 1:2 and 1:3 slopes.
    memset(frame, 0, FRIDGE_GPU_FRAME_BUFFER_SIZE);
    memset(expected, 0, sizeof(expected));
    const unsigned directions[] = {
        0x10, 0x31, 0x21, 0x11, 0x12, 0x13, 0x01,
        0xf3, 0xf2, 0xf1, 0xe1, 0xd1, 0xf0,
        0xdf, 0xef, 0xff, 0xfe, 0xfd, 0x0f,
        0x1d, 0x1e, 0x1f, 0x2f, 0x3f
    };
    setup(sys, 0, 16, 0x3100);
    for (unsigned i = 0; i < sizeof(directions)/sizeof(directions[0]); ++i)
    {
        unsigned color = i%15 + 1;
        sys->gpu->sprite_mem[0x3100] = FRIDGE_GPU_WORD(color, 15);
        FRIDGE_WORD ray[16];
        memset(ray, color, sizeof(ray));
        unsigned x = FRIDGE_GPU_FRAME_EGA_WIDTH/2, y = FRIDGE_GPU_FRAME_EGA_HEIGHT/2;
        reference_draw(expected, ray, 16, 60, directions[i], FRIDGE_GPU_RLE_OPAQUE, x, y);
        draw(sys, 0, 60, directions[i], FRIDGE_GPU_RLE_OPAQUE, x, y);
    }
    check_frame(sys, expected, "Direction screen disagreed with reference");
    save_screen(sys, "directions", frame);

    // Four bands apply the four modes over the same striped background.
    for (unsigned y = 0; y < FRIDGE_GPU_FRAME_EGA_HEIGHT; ++y)
        for (unsigned x = 0; x < FRIDGE_GPU_FRAME_EGA_WIDTH; x += 2)
            frame[(y*FRIDGE_GPU_FRAME_EGA_WIDTH+x)/2] = FRIDGE_GPU_WORD((x/8)%16, ((x+1)/8)%16);
    memcpy(expected, frame, sizeof(expected));
    setup(sys, 0, 64, 0x3000);
    for (unsigned mode = 0; mode <= FRIDGE_GPU_RLE_ADDITIVE; ++mode)
        for (unsigned row = 0; row < 24; ++row)
        {
            unsigned y = 12 + mode*36 + row;
            unsigned length = FRIDGE_GPU_FRAME_EGA_WIDTH - 16;
            reference_draw(expected, source, 64, length, FRIDGE_GPU_RLE_RIGHT, mode, 8, y);
            draw(sys, 0, length, FRIDGE_GPU_RLE_RIGHT, mode, 8, y);
        }
    check_frame(sys, expected, "Blending screen disagreed with reference");
    save_screen(sys, "blending", frame);
    require(!FRIDGE_cpu_flag_PANIC(sys->cpu), "Screen rendering caused a panic");
}

int main(void)
{
    FRIDGE_CPU cpu = {};
    FRIDGE_GPU gpu = {};
    FRIDGE_SYSTEM sys = {};
    sys.cpu = &cpu;
    sys.gpu = &gpu;
    FRIDGE_cpu_reset(&cpu);
    FRIDGE_gpu_reset(&gpu);
    test_setup(&sys);
    cpu.rE = 0x6a;
    cpu.rF = 0x95;
    test_scaling(&sys);
    test_directions_and_modes(&sys);
    test_bounds(&sys);
    test_screens(&sys);
    printf("%dx%d RLE lines: PASS\n", FRIDGE_GPU_FRAME_EGA_WIDTH,
            FRIDGE_GPU_FRAME_EGA_HEIGHT);
    return 0;
}
