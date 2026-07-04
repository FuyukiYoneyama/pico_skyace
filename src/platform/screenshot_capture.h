#ifndef PICO_SKYACE_PLATFORM_SCREENSHOT_CAPTURE_H_
#define PICO_SKYACE_PLATFORM_SCREENSHOT_CAPTURE_H_

#include <cstdint>

// SD カードの /screenshots/ に、現在のゲーム内フレームバッファ(RGB565)を
// 24bit BMP として保存する。game/pico_rescue の diagnostics/screenshot_capture
// と違い、LCD からの読み出しはせず、既に手元にある内部フレームバッファ
// （160x160）をそのまま使う。呼び出し中は同期的にブロックする
// （ゲームループが一時停止するのは仕様として許容）。
namespace skyace::screenshot {

bool capture(const uint16_t* pixels, int width, int height);

}  // namespace skyace::screenshot

#endif  // PICO_SKYACE_PLATFORM_SCREENSHOT_CAPTURE_H_
