// このファイルは薄いアダプタに徹する。実際の LCD 制御は
// platform/lcd_rgb565_pio.cpp（general/lcd/src/lcd_rgb565_pio.cpp を
// 無改変でコピーしたもの。2026-07-04 実機動作確認済み）が行う。
// 独自の再実装をやめ、検証済みファイルをそのまま呼ぶことで、書き写しミスの
// 可能性を排除する。
#include "platform/picocalc_display.h"

#include <cstdio>

#include "platform/lcd_rgb565_pio.h"

namespace skyace::display {

void init() {
    lcd_rgb565_pio_init(false);  // DMA なし。general/lcd の lcd_pio_blocking_rgb565 と同じ
    std::printf("DISPLAY init ok (verified lcd_rgb565_pio driver, blocking mode)\n");
}

void clear_black() {
    lcd_rgb565_pio_fill_rect_blocking(0, 0, kPanelWidth, kPanelHeight, 0x0000);
}

// general/lcd の draw_quadrants_blocking() と全く同じ粒度・全く同じ呼び出し
// パターン（160x160 の 4 象限、各象限は 1 回の set_window の後に行ごとの
// write_blocking）で転送する。象限の並び・サイズ・呼び出し順序も同一。
void present_scaled2x(const uint16_t* frame) {
    static uint16_t line[160];
    for (int qy = 0; qy < 2; ++qy) {
        for (int qx = 0; qx < 2; ++qx) {
            lcd_rgb565_pio_set_window(qx * 160, qy * 160, 160, 160);
            for (int row = 0; row < 80; ++row) {
                const uint16_t* src =
                    frame + (qy * 80 + row) * kFrameWidth + qx * 80;
                for (int x = 0; x < 80; ++x) {
                    line[x * 2] = src[x];
                    line[x * 2 + 1] = src[x];
                }
                // 内部行を 2 回書いて縦 2 倍
                lcd_rgb565_pio_write_blocking(line, 160);
                lcd_rgb565_pio_write_blocking(line, 160);
            }
        }
    }
}

}  // namespace skyace::display
