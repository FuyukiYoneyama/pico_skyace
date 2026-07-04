#ifndef PICO_SKYACE_PLATFORM_PICOCALC_DISPLAY_H_
#define PICO_SKYACE_PLATFORM_PICOCALC_DISPLAY_H_

#include <cstdint>

namespace skyace::display {

constexpr int kPanelWidth = 320;
constexpr int kPanelHeight = 320;
constexpr int kFrameWidth = 160;   // 内部レンダリング解像度
constexpr int kFrameHeight = 160;

void init();
void clear_black();
// 160x160 RGB565 フレームバッファを 2 倍拡大して全画面へ転送する
void present_scaled2x(const uint16_t* frame);

}  // namespace skyace::display

#endif  // PICO_SKYACE_PLATFORM_PICOCALC_DISPLAY_H_
