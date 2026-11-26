extern "C" {
#include "fridgemulib.h"
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_artifacts.h"

static void require(int passed, const char* message)
{
    if (!passed)
    {
        fprintf(stderr, "%dx%d: %s\n", FRIDGE_GPU_FRAME_EGA_WIDTH,
                FRIDGE_GPU_FRAME_EGA_HEIGHT, message);
        exit(1);
    }
}

static void check_pixel(const unsigned char* rgb, size_t index,
                        const FRIDGE_GPU* gpu, unsigned color)
{
    require(memcmp(rgb + index*3, gpu->palette + color*3, 3) == 0,
            "Unexpected rendered pixel");
}

static unsigned pattern(unsigned x, unsigned y)
{
    return (x/2 + y) % 16;
}

static void save_screen(const char* scene, const unsigned char* rgb)
{
    char name[80];
    snprintf(name, sizeof(name), "video-%dx%d-%s", FRIDGE_GPU_FRAME_EGA_WIDTH,
             FRIDGE_GPU_FRAME_EGA_HEIGHT, scene);
    require(save_test_screen(name, rgb, FRIDGE_GPU_FRAME_EGA_WIDTH,
                            FRIDGE_GPU_FRAME_EGA_HEIGHT), "Could not save screen");
}

int main(void)
{
    const unsigned width = FRIDGE_GPU_FRAME_EGA_WIDTH;
    const unsigned height = FRIDGE_GPU_FRAME_EGA_HEIGHT;
    const size_t bytes = (size_t)width*height*3;
    FRIDGE_GPU* gpu = (FRIDGE_GPU*)calloc(1, sizeof(*gpu));
    unsigned char* rgb = (unsigned char*)malloc(bytes + 1);
    FRIDGE_WORD glyphs[FRIDGE_GPU_TEXT_GLYPH_WIDTH*256] = {0};
    require(gpu && rgb, "Allocation failed");
#if defined(FRIDGE_VIDEO_240X180)
    require(width == 240 && height == 180 && FRIDGE_GPU_FRAME_BUFFER_SIZE == 21600,
            "240x180 mode was not selected");
#elif defined(FRIDGE_VIDEO_210X160)
    require(width == 210 && height == 160 && FRIDGE_GPU_FRAME_BUFFER_SIZE == 16800,
            "210x160 mode was not selected");
#else
    require(width == 240 && height == 160 && FRIDGE_GPU_FRAME_BUFFER_SIZE == 19200,
            "Default mode must be 240x160");
#endif
    FRIDGE_gpu_reset(gpu);
    FRIDGE_WORD* frame = FRIDGE_gpu_visible_frame(gpu);

    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; x += 2)
            frame[(y*width+x)/2] = FRIDGE_GPU_WORD(pattern(x, y), pattern(x, y));
    memset(rgb, 0xa5, bytes + 1);
    FRIDGE_gpu_render_ega_rgb8(gpu, rgb);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x)
            check_pixel(rgb, y*width+x, gpu, pattern(x, y));
    require(rgb[bytes] == 0xa5, "Full-frame render overran output");
    save_screen("ega", rgb);

    // Scrolling wraps at the selected frame dimensions.
    gpu->frame_hor_offset = width-3;
    gpu->frame_ver_offset = 7;
    FRIDGE_gpu_render_ega_rgb8(gpu, rgb);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x)
            check_pixel(rgb, y*width+x, gpu,
                        pattern((x+width-3)%width, (y+7)%height));
    save_screen("scrolling", rgb);
    gpu->frame_hor_offset = gpu->frame_ver_offset = 0;

    // Read the bottom-right area while preserving output row padding.
    memset(rgb, 0xa5, bytes + 1);
    FRIDGE_gpu_render_ega_rgb8_area(gpu, rgb, width-4, height-2, 4, 2, 2);
    for (unsigned y = 0; y < 2; ++y)
    {
        for (unsigned x = 0; x < 4; ++x)
            check_pixel(rgb, y*6+x, gpu, pattern(width-4+x, height-2+y));
        for (unsigned i = 12; i < 18; ++i)
            require(rgb[y*18+i] == 0xa5, "Area render overwrote row padding");
    }
    require(rgb[36] == 0xa5, "Area render overran output");

    // A one-pixel sprite covers only its position.
    memset(frame, 0, FRIDGE_GPU_FRAME_BUFFER_SIZE);
    gpu->sprite_list[0].mode = FRIDGE_GPU_SPRITE_OPAQUE;
    gpu->sprite_list[0].position_x = 10;
    gpu->sprite_list[0].size_x = gpu->sprite_list[0].size_y = 1;
    gpu->sprite_mem[1] = 1;
    FRIDGE_gpu_render_ega_rgb8(gpu, rgb);
    for (unsigned x = 0; x < width; ++x)
        check_pixel(rgb, x, gpu, x == 10 ? 1 : 0);
    save_screen("sprite", rgb);

    // At x=10 the transparent-zero sprite still occupies the first slot.
    // Descriptor 63 participates as soon as the first sprite's span ends.
    gpu->sprite_list[0].mode = FRIDGE_GPU_SPRITE_TRANSPARENT0;
    gpu->sprite_mem[1] = 0;
    for (unsigned id = 1; id <= 3; ++id)
    {
        gpu->sprite_list[id] = gpu->sprite_list[0];
        gpu->sprite_list[id].mode = FRIDGE_GPU_SPRITE_OPAQUE;
        gpu->sprite_list[id].size_x = 3;
    }
    FRIDGE_GPU_SPRITE* last = &gpu->sprite_list[FRIDGE_GPU_MAX_SPRITES - 1];
    *last = gpu->sprite_list[1];
    last->size_x = 5;
    gpu->sprite_mem[2] = 1;
    gpu->sprite_mem[3] = 2;
    FRIDGE_gpu_render_ega_rgb8_area(gpu, rgb, 9, 0, 3, 1, 0);
    check_pixel(rgb, 0, gpu, 0);
    check_pixel(rgb, 1, gpu, 1);
    check_pixel(rgb, 2, gpu, 2);

    // Check sprite-local coordinate wrapping and viewport scrolling together.
    FRIDGE_gpu_reset(gpu);
    last->mode = FRIDGE_GPU_SPRITE_OPAQUE;
    last->position_x = last->position_y = 255;
    last->size_x = last->size_y = 2;
    gpu->sprite_mem[3] = 2;
    gpu->frame_hor_offset = width - 1;
    gpu->frame_ver_offset = height - 1;
    FRIDGE_gpu_render_ega_rgb8_area(gpu, rgb, 0, 0, 2, 2, 0);
    check_pixel(rgb, 0, gpu, 0);
    check_pixel(rgb, 1, gpu, 0);
    check_pixel(rgb, 2, gpu, 0);
    check_pixel(rgb, 3, gpu, 2);
    gpu->frame_hor_offset = gpu->frame_ver_offset = 0;

    // All complete cells render; partial cells use palette zero, not next-row data.
    memset(frame, 0x11, FRIDGE_GPU_FRAME_BUFFER_SIZE);
    memset(glyphs + 'A'*FRIDGE_GPU_TEXT_GLYPH_WIDTH, 0xff, FRIDGE_GPU_TEXT_GLYPH_WIDTH);
    for (unsigned i = 0; i < FRIDGE_GPU_TEXT_FRAME_WIDTH*FRIDGE_GPU_TEXT_FRAME_HEIGHT; ++i)
    {
        frame[i*2] = 'A';
        frame[i*2+1] = 1;
    }
    memset(rgb, 0xa5, bytes + 1);
    FRIDGE_gpu_render_txt_rgb8(gpu, rgb, glyphs);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x)
            check_pixel(rgb, y*width+x, gpu,
                        x < FRIDGE_GPU_TEXT_FRAME_WIDTH*FRIDGE_GPU_TEXT_GLYPH_WIDTH &&
                        y < FRIDGE_GPU_TEXT_FRAME_HEIGHT*FRIDGE_GPU_TEXT_GLYPH_HEIGHT ? 1 : 0);
    require(rgb[bytes] == 0xa5, "Text render overran output");
    save_screen("text", rgb);
    free(rgb);
    free(gpu);
    printf("%ux%u EGA, scrolling, area, sprites and text: PASS\n", width, height);
    return 0;
}
