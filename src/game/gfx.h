#ifndef PICO_SKYACE_GAME_GFX_H_
#define PICO_SKYACE_GAME_GFX_H_

#include <cstdint>

// 160x160 内部フレームバッファへの描画プリミティブ。
// 座標は内部解像度（0..159）。クリッピングは各関数内で行う。

namespace skyace::gfx {

constexpr int kWidth = 160;
constexpr int kHeight = 160;

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
}

uint16_t* fb();

void clear(uint16_t color);
void hspan(int y, int x0, int x1, uint16_t color);  // [x0, x1) を塗る
void fill_rect(int x, int y, int w, int h, uint16_t color);
void rect_outline(int x, int y, int w, int h, uint16_t color);
void put_pixel(int x, int y, uint16_t color);
void line(int x0, int y0, int x1, int y1, uint16_t color);
void fill_circle(int cx, int cy, int r, uint16_t color);
void circle_outline(int cx, int cy, int r, uint16_t color);

// 5x7 フォント。scale は整数倍。小文字は大文字として描画。
void text(int x, int y, const char* str, uint16_t color, int scale = 1);
int text_width(const char* str, int scale = 1);

// ASCII アートスプライト描画。rows[i] は 1 行分の文字列（'.' と ' ' は透明）。
// lookup(c) が色を返す。scale_q8 = 256 で等倍。(cx, cy) はスプライト中心。
using PaletteFn = uint16_t (*)(char c);
void art(const char* const* rows, int n_rows, int cx, int cy, int scale_q8,
         PaletteFn lookup);
int art_width(const char* const* rows, int n_rows);

}  // namespace skyace::gfx

#endif  // PICO_SKYACE_GAME_GFX_H_
