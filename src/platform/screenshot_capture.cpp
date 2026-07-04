#include "platform/screenshot_capture.h"

#include <cstdio>
#include <cstring>

#include "ff.h"

namespace skyace::screenshot {
namespace {

constexpr uint32_t kBmpHeaderSize = 54;
constexpr const char* kScreenshotDir = "0:/screenshots";

bool g_busy = false;
FATFS g_fs{};
uint8_t g_row[512 * 3] = {};  // 160px 幅の実運用では十分な余裕を持たせる

void put_u16(uint8_t* dst, uint16_t value) {
    dst[0] = static_cast<uint8_t>(value);
    dst[1] = static_cast<uint8_t>(value >> 8);
}

void put_u32(uint8_t* dst, uint32_t value) {
    dst[0] = static_cast<uint8_t>(value);
    dst[1] = static_cast<uint8_t>(value >> 8);
    dst[2] = static_cast<uint8_t>(value >> 16);
    dst[3] = static_cast<uint8_t>(value >> 24);
}

bool file_exists(const char* path) {
    FILINFO info{};
    return f_stat(path, &info) == FR_OK;
}

bool choose_path(char* out_path, size_t out_size) {
    for (int index = 1; index <= 9999; ++index) {
        std::snprintf(out_path, out_size, "0:/screenshots/skyace_%04d.BMP", index);
        if (!file_exists(out_path)) {
            return true;
        }
    }
    return false;
}

bool write_exact(FIL* file, const void* data, UINT size) {
    UINT written = 0;
    const FRESULT result = f_write(file, data, size, &written);
    return result == FR_OK && written == size;
}

void make_bmp_header(uint8_t header[kBmpHeaderSize], int width, int height,
                     uint32_t row_bytes) {
    const uint32_t image_bytes = row_bytes * static_cast<uint32_t>(height);
    const uint32_t file_bytes = kBmpHeaderSize + image_bytes;
    std::memset(header, 0, kBmpHeaderSize);
    header[0] = 'B';
    header[1] = 'M';
    put_u32(&header[2], file_bytes);
    put_u32(&header[10], kBmpHeaderSize);
    put_u32(&header[14], 40);
    put_u32(&header[18], static_cast<uint32_t>(width));
    put_u32(&header[22], static_cast<uint32_t>(-height));  // top-down
    put_u16(&header[26], 1);
    put_u16(&header[28], 24);
    put_u32(&header[34], image_bytes);
}

void convert_row_rgb565_to_bgr24(const uint16_t* pixels, int width) {
    for (int x = 0; x < width; ++x) {
        const uint16_t rgb = pixels[x];
        const uint8_t red = static_cast<uint8_t>(((rgb >> 11) & 0x1f) << 3);
        const uint8_t green = static_cast<uint8_t>(((rgb >> 5) & 0x3f) << 2);
        const uint8_t blue = static_cast<uint8_t>((rgb & 0x1f) << 3);
        const int offset = x * 3;
        g_row[offset + 0] = blue;
        g_row[offset + 1] = green;
        g_row[offset + 2] = red;
    }
}

}  // namespace

bool capture(const uint16_t* pixels, int width, int height) {
    if (g_busy || pixels == nullptr || width <= 0 || height <= 0) {
        std::printf("SCREENSHOT error stage=busy_or_args\n");
        return false;
    }
    g_busy = true;

    FRESULT result = f_mount(&g_fs, "0:", 1);
    if (result != FR_OK) {
        std::printf("SCREENSHOT error stage=mount fr=%u\n", static_cast<unsigned>(result));
        g_busy = false;
        return false;
    }

    result = f_mkdir(kScreenshotDir);
    if (result != FR_OK && result != FR_EXIST) {
        std::printf("SCREENSHOT error stage=mkdir fr=%u\n", static_cast<unsigned>(result));
        g_busy = false;
        return false;
    }

    char path[40] = {};
    if (!choose_path(path, sizeof(path))) {
        std::printf("SCREENSHOT error stage=path\n");
        g_busy = false;
        return false;
    }

    FIL file{};
    result = f_open(&file, path, FA_CREATE_NEW | FA_WRITE);
    if (result != FR_OK) {
        std::printf("SCREENSHOT error stage=open fr=%u path=%s\n",
                    static_cast<unsigned>(result), path);
        g_busy = false;
        return false;
    }

    std::printf("SCREENSHOT begin path=%s\n", path);
    const uint32_t row_bytes = static_cast<uint32_t>(width) * 3u;
    uint8_t header[kBmpHeaderSize] = {};
    make_bmp_header(header, width, height, row_bytes);
    bool ok = write_exact(&file, header, sizeof(header));

    for (int y = 0; ok && y < height; ++y) {
        convert_row_rgb565_to_bgr24(pixels + static_cast<size_t>(y) * width, width);
        ok = write_exact(&file, g_row, row_bytes);
    }

    result = f_sync(&file);
    if (ok && result != FR_OK) {
        ok = false;
        std::printf("SCREENSHOT error stage=sync fr=%u\n", static_cast<unsigned>(result));
    }

    result = f_close(&file);
    if (ok && result != FR_OK) {
        ok = false;
        std::printf("SCREENSHOT error stage=close fr=%u\n", static_cast<unsigned>(result));
    }

    if (!ok) {
        f_unlink(path);
        g_busy = false;
        return false;
    }

    std::printf("SCREENSHOT done status=ok path=%s\n", path);
    g_busy = false;
    return true;
}

}  // namespace skyace::screenshot
