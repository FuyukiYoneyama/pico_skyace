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
// 回路図（clockwork_Mainboard_V2.0）で確認済み: GP21 は PSRAM の RAM_SCK
// （LCDとは無関係）。LCD の CS は GP13 のみ。ただし general/lcd の実機確認済み
// サンプルもこの GP21 を出力・High に固定しており、無害なので踏襲する。
constexpr unsigned kLcdPinRamCs = 21;

// PSRAM CS を High に保持して SPI バス上で沈黙させる（PSRAM 自体は未使用）
constexpr unsigned kPsramPinCs = 20;

constexpr unsigned kKeyboardI2cSda = 6;
constexpr unsigned kKeyboardI2cScl = 7;
constexpr uint32_t kKeyboardI2cHz = 400 * 1000;
constexpr uint8_t kKeyboardI2cAddress = 0x1f;

// general/lcd/src/lcd_rgb565_pio.cpp（2026-07-04 実機動作確認済み）と同じ値。
// 250 MHz sysclk / (2 * kPioClkDiv) = 62.5 MHz 相当。
constexpr uint32_t kSysClockKhz = 250000;
constexpr float kLcdPioClkDiv = 2.0f;

}  // namespace skyace::board

#endif  // PICO_SKYACE_CONFIG_BOARD_CONFIG_H_
