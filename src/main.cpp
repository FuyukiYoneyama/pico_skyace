#include <cstdio>

#include "config/board_config.h"
#include "game/game.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "platform/picocalc_audio.h"
#include "platform/picocalc_display.h"
#include "platform/picocalc_keyboard.h"

#ifndef PICO_SKYACE_VERSION_STRING
#define PICO_SKYACE_VERSION_STRING "0.0.0-dev"
#endif

int main() {
    const bool watchdog_reboot = watchdog_caused_reboot();

    const bool clock_ok = set_sys_clock_khz(skyace::board::kSysClockKhz, true);
    // life/synth と同じ手順: クロック変更の直後に PIO/LCD へ触りにいくのではなく、
    // 新しい clk_sys が完全に安定するまで猶予を置く。ここが「PIO とクロックの
    // 整合性」で唯一 life と違っていた点（life は set_sys_clock_khz の直後に
    // 100ms 待ってから display::init() を呼んでいる）。
    stdio_init_all();
    sleep_ms(100);

    std::printf("pico_skyace version %s\r\n", PICO_SKYACE_VERSION_STRING);
    std::printf("BUILD ID time=\"%s %s\"\r\n", __DATE__, __TIME__);
    // ハング/フリーズ切り分け用（general/11_TIMING.md §6）。前回起動が
    // ウォッチドッグ復帰なら、画面が固まって自動リセットされたことが分かる。
    std::printf("WATCHDOG_CAUSED_REBOOT=%d\r\n", watchdog_reboot ? 1 : 0);
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

    // 電源投入直後は、バックライト/キー入力を担う STM32 側コントローラが
    // まだ I2C に応答できるまで起き切っていないことがある。BOOTSEL 経由の
    // 再書き込みは RP2040 だけをリセットし STM32 は起きたままなので、
    // 「一度成功すればその後は毎回動く」という報告はこれで説明がつく。
    // 最初の書き込みが失敗したら少し待って数回リトライする。
    bool backlight_ok = skyace::keyboard::set_backlight(220);
    for (int retry = 1; !backlight_ok && retry <= 5; ++retry) {
        sleep_ms(50);
        backlight_ok = skyace::keyboard::set_backlight(220);
        std::printf("BACKLIGHT retry=%d ok=%d\r\n", retry, backlight_ok);
    }

    // ハング対策ウォッチドッグ（general/11_TIMING.md §6）。ゲームループが
    // 3 秒以内に watchdog_update() を呼べなければ自動リセットする。バグの
    // 根本解決ではなく、次回起動ログで「フリーズして自動復帰した」ことを
    // 判別するための保険。
    watchdog_enable(3000, true);

    skyace::game::run();
    return 0;
}
