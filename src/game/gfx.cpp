#include "game/gfx.h"

#include <cstring>

#include "game/font5x7.h"

namespace skyace::gfx {
namespace {

uint16_t g_fb[kWidth * kHeight];

inline bool row_valid(int y) {
    return y >= 0 && y < kHeight;
}

int art_row_len(const char* row) {
    return static_cast<int>(std::strlen(row));
}

}  // namespace

uint16_t* fb() {
    return g_fb;
}

void clear(uint16_t color) {
    for (int i = 0; i < kWidth * kHeight; ++i) {
        g_fb[i] = color;
    }
}

void hspan(int y, int x0, int x1, uint16_t color) {
    if (!row_valid(y)) {
        return;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 > kWidth) {
        x1 = kWidth;
    }
    uint16_t* p = g_fb + y * kWidth;
    for (int x = x0; x < x1; ++x) {
        p[x] = color;
    }
}

void fill_rect(int x, int y, int w, int h, uint16_t color) {
    for (int row = y; row < y + h; ++row) {
        hspan(row, x, x + w, color);
    }
}

void rect_outline(int x, int y, int w, int h, uint16_t color) {
    hspan(y, x, x + w, color);
    hspan(y + h - 1, x, x + w, color);
    for (int row = y + 1; row < y + h - 1; ++row) {
        put_pixel(x, row, color);
        put_pixel(x + w - 1, row, color);
    }
}

void put_pixel(int x, int y, uint16_t color) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return;
    }
    g_fb[y * kWidth + x] = color;
}

void line(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    const int adx = dx >= 0 ? dx : -dx;
    const int ady = dy >= 0 ? dy : -dy;
    const int sx = dx >= 0 ? 1 : -1;
    const int sy = dy >= 0 ? 1 : -1;
    int err = adx - ady;
    int x = x0;
    int y = y0;
    while (true) {
        put_pixel(x, y, color);
        if (x == x1 && y == y1) {
            break;
        }
        const int e2 = err * 2;
        if (e2 > -ady) {
            err -= ady;
            x += sx;
        }
        if (e2 < adx) {
            err += adx;
            y += sy;
        }
    }
}

void fill_circle(int cx, int cy, int r, uint16_t color) {
    for (int dy = -r; dy <= r; ++dy) {
        int hw = 0;
        const int rem = r * r - dy * dy;
        while ((hw + 1) * (hw + 1) <= rem) {
            ++hw;
        }
        hspan(cy + dy, cx - hw, cx + hw + 1, color);
    }
}

void circle_outline(int cx, int cy, int r, uint16_t color) {
    int x = r;
    int y = 0;
    int err = 0;
    while (x >= y) {
        put_pixel(cx + x, cy + y, color);
        put_pixel(cx + y, cy + x, color);
        put_pixel(cx - y, cy + x, color);
        put_pixel(cx - x, cy + y, color);
        put_pixel(cx - x, cy - y, color);
        put_pixel(cx - y, cy - x, color);
        put_pixel(cx + y, cy - x, color);
        put_pixel(cx + x, cy - y, color);
        ++y;
        if (err <= 0) {
            err += 2 * y + 1;
        }
        if (err > 0) {
            --x;
            err -= 2 * x + 1;
        }
    }
}

void text(int x, int y, const char* str, uint16_t color, int scale) {
    int pen = x;
    for (const char* p = str; *p != '\0'; ++p) {
        char c = *p;
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - ('a' - 'A'));
        }
        if (c < font::kFirstChar || c > font::kLastChar) {
            c = '?';
        }
        const uint8_t* glyph = font::kGlyphs[c - font::kFirstChar];
        for (int col = 0; col < 5; ++col) {
            const uint8_t bits = glyph[col];
            for (int row = 0; row < 7; ++row) {
                if (bits & (1u << row)) {
                    if (scale == 1) {
                        put_pixel(pen + col, y + row, color);
                    } else {
                        fill_rect(pen + col * scale, y + row * scale, scale, scale, color);
                    }
                }
            }
        }
        pen += 6 * scale;
    }
}

int text_width(const char* str, int scale) {
    return static_cast<int>(std::strlen(str)) * 6 * scale - scale;
}

int art_width(const char* const* rows, int n_rows) {
    int w = 0;
    for (int i = 0; i < n_rows; ++i) {
        const int len = art_row_len(rows[i]);
        if (len > w) {
            w = len;
        }
    }
    return w;
}

void art(const char* const* rows, int n_rows, int cx, int cy, int scale_q8,
         PaletteFn lookup) {
    if (scale_q8 <= 0) {
        return;
    }
    const int src_w = art_width(rows, n_rows);
    const int src_h = n_rows;
    int dst_w = (src_w * scale_q8) >> 8;
    int dst_h = (src_h * scale_q8) >> 8;
    if (dst_w < 1) {
        dst_w = 1;
    }
    if (dst_h < 1) {
        dst_h = 1;
    }
    const int x0 = cx - dst_w / 2;
    const int y0 = cy - dst_h / 2;

    for (int dy = 0; dy < dst_h; ++dy) {
        const int py = y0 + dy;
        if (!row_valid(py)) {
            continue;
        }
        int sy = dy * src_h / dst_h;
        if (sy >= src_h) {
            sy = src_h - 1;
        }
        const char* row = rows[sy];
        const int row_len = art_row_len(row);
        uint16_t* dst = g_fb + py * kWidth;
        for (int dx = 0; dx < dst_w; ++dx) {
            const int px = x0 + dx;
            if (px < 0 || px >= kWidth) {
                continue;
            }
            int sx = dx * src_w / dst_w;
            if (sx >= row_len) {
                continue;
            }
            const char c = row[sx];
            if (c == '.' || c == ' ') {
                continue;
            }
            dst[px] = lookup(c);
        }
    }
}

}  // namespace skyace::gfx
