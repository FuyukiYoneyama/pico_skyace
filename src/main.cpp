#include <cstdio>

#include "config/board_config.h"
#include "game/game.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "platform/picocalc_audio.h"
#include "platform/picocalc_display.h"
#include "platform/picocalc_keyboard.h"

#ifndef PICO_SKYACE_VERSION_STRING
#define PICO_SKYACE_VERSION_STRING "0.0.0-dev"
#endif

int main() {
    const bool clock_ok = set_sys_clock_khz(skyace::board::kSysClockKhz, true);
    // life/synth と同じ手順: クロック変更の直後に PIO/LCD へ触りにいくのではなく、
    // 新しい clk_sys が完全に安定するまで猶予を置く。ここが「PIO とクロックの
    // 整合性」で唯一 life と違っていた点（life は set_sys_clock_khz の直後に
    // 100ms 待ってから display::init() を呼んでいる）。
    stdio_init_all();
    sleep_ms(100);

    std::printf("pico_skyace version %s\r\n", PICO_SKYACE_VERSION_STRING);
    std::printf("BUILD ID time=\"%s %s\"\r\n", __DATE__, __TIME__);
    std::printf("set_sys_clock_khz(%lu) ok=%d sysclk=%lu kHz\r\n",
                static_cast<unsigned long>(skyace::board::kSysClockKhz),
                clock_ok,
                static_cast<unsigned long>(clock_get_hz(clk_sys) / 1000));

    skyace::display::init();
    skyace::keyboard::init();
    skyace::audio::init();

    // SD ローダー経由の起動では前のプログラムがバックライトを暗く/消して
    // いる場合があり、コードは正しく描画していても画面が真っ黒に見える。
    // 現在値をログに残した上で確実に明るい値へ強制する。
    const uint8_t prev_backlight = skyace::keyboard::read_backlight();
    std::printf("BACKLIGHT before=%u\r\n", prev_backlight);
    skyace::keyboard::set_backlight(220);

    skyace::game::run();
    return 0;
}
