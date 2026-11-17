#ifndef FRIDGE_TEST_ARTIFACTS_H
#define FRIDGE_TEST_ARTIFACTS_H

#include <errno.h>
#include <stdio.h>
#include <string>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifndef FRIDGE_TEST_ARTIFACT_DIR
#define FRIDGE_TEST_ARTIFACT_DIR "artifacts"
#endif

static void artifact_le32(unsigned char* bytes, unsigned value)
{
    for (unsigned i = 0; i < 4; ++i)
        bytes[i] = (value >> (8*i)) & 255;
}

// A 24-bit BMP needs no image-library dependency. Input is tightly packed RGB8.
static bool save_test_screen(const char* name, const unsigned char* rgb,
                             unsigned width, unsigned height)
{
#ifdef _WIN32
    int result = _mkdir(FRIDGE_TEST_ARTIFACT_DIR);
#else
    int result = mkdir(FRIDGE_TEST_ARTIFACT_DIR, 0777);
#endif
    if (result != 0 && errno != EEXIST)
    {
        perror(FRIDGE_TEST_ARTIFACT_DIR);
        return false;
    }
    std::string path = std::string(FRIDGE_TEST_ARTIFACT_DIR) + "/" + name + ".bmp";
    FILE* file = fopen(path.c_str(), "wb");
    if (!file)
    {
        perror(path.c_str());
        return false;
    }
    const unsigned stride = (width*3 + 3) & ~3u;
    unsigned char header[54] = {'B', 'M'};
    artifact_le32(header + 2, sizeof(header) + stride*height);
    header[10] = sizeof(header);
    header[14] = 40;
    artifact_le32(header + 18, width);
    artifact_le32(header + 22, height);
    header[26] = 1;
    header[28] = 24;
    artifact_le32(header + 34, stride*height);
    bool saved = fwrite(header, 1, sizeof(header), file) == sizeof(header);
    std::vector<unsigned char> row(stride, 0);
    for (unsigned y = height; y > 0 && saved; --y)
    {
        for (unsigned x = 0; x < width; ++x)
        {
            const unsigned char* pixel = rgb + ((y-1)*width + x)*3;
            row[x*3] = pixel[2];
            row[x*3+1] = pixel[1];
            row[x*3+2] = pixel[0];
        }
        saved = fwrite(row.data(), 1, stride, file) == stride;
    }
    if (fclose(file) != 0)
        saved = false;
    if (saved)
        printf("Screen artifact: %s\n", path.c_str());
    else
        fprintf(stderr, "Could not write screen artifact: %s\n", path.c_str());
    return saved;
}

#endif
