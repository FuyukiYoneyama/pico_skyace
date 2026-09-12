#ifndef PICO_SKYACE_CONFIG_BOARD_CONFIG_H_
#define PICO_SKYACE_CONFIG_BOARD_CONFIG_H_

#include <cstdint>

namespace skyace::board {

constexpr unsigned kLcdPinSck = 10;
constexpr unsigned kLcdPinMosi = 11;
constexpr unsigned kLcdPinMiso = 12;
constexpr unsigned kLcdPinCs = 13;
constexpr unsigned kLcdPinDc = 14;
constexpr unsigned kLcdPinRst = 15;
// PicoCalc 公式回路図で確認済み: GP21 は PSRAM の RAM_SCK
// （LCDとは無関係）。LCD の CS は GP13 のみ。ただし実機確認済みの参照実装も
// この GP21 を出力・High に固定しており、無害なので踏襲する。
constexpr unsigned kLcdPinRamCs = 21;

// PSRAM CS を High に保持して SPI バス上で沈黙させる（PSRAM 自体は未使用）
constexpr unsigned kPsramPinCs = 20;

constexpr unsigned kKeyboardI2cSda = 6;
constexpr unsigned kKeyboardI2cScl = 7;
constexpr uint32_t kKeyboardI2cHz = 400 * 1000;
constexpr uint8_t kKeyboardI2cAddress = 0x1f;

// PicoCalc 公式回路図準拠。左右とも実機で発音実績あり。
constexpr unsigned kAudioPwmLeft = 26;
constexpr unsigned kAudioPwmRight = 27;

// LCD は基板や電源再投入の状態によって高速 PIO 転送のマージンが変わる。
// 250 MHz sysclk では、実効 31.25 MHz 相当まで落として安定性を優先する。
// （SPI bit rate = sysclk / (2 * kLcdPioClkDiv)）
constexpr uint32_t kSysClockKhz = 250000;
constexpr float kLcdPioClkDiv = 4.0f;

}  // namespace skyace::board

#endif  // PICO_SKYACE_CONFIG_BOARD_CONFIG_H_
