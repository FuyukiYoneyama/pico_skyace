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
#ifndef PICO_SKYACE_BUILD_ID
#define PICO_SKYACE_BUILD_ID "unknown"
#endif

namespace {

// 電源投入時は、RP2040がUARTを初期化してからCH340とホスト側COMポートの
// 再認識が完了するまで時間差がある。UARTにはホストの接続状態を問い合わせる
// 手段がないため、最初の識別行を送る前に短い整定時間を設ける。
constexpr uint32_t kBootUartSettleMs = 500;

// Keep the binary identity on one machine-readable line.  The line is emitted
// before any peripheral initialization and is flushed explicitly so a boot log
// can identify the firmware even when a later initialization step fails.
void log_boot_identity(const char* phase, bool watchdog_reboot, bool clock_ok) {
    std::printf(
        "PICO_SKYACE_BOOT app=pico_skyace version=%s build=\"%s\" "
        "WATCHDOG_CAUSED_REBOOT=%d clock_ok=%d sysclk_target_khz=%lu "
        "sysclk_actual_khz=%lu uart_baud=115200 phase=%s\r\n",
        PICO_SKYACE_VERSION_STRING,
        PICO_SKYACE_BUILD_ID,
        watchdog_reboot ? 1 : 0,
        clock_ok ? 1 : 0,
        static_cast<unsigned long>(skyace::board::kSysClockKhz),
        static_cast<unsigned long>(clock_get_hz(clk_sys) / 1000),
        phase);
    stdio_flush();
}

}  // namespace

int main() {
    const bool watchdog_reboot = watchdog_caused_reboot();

    const bool clock_ok = set_sys_clock_khz(skyace::board::kSysClockKhz, true);
    // クロック変更の直後に PIO/LCD へ触りにいくのではなく、新しい clk_sys が
    // 完全に安定するまで猶予を置く（set_sys_clock_khz の直後に 100ms 待って
    // から display::init() を呼ぶ）。
    stdio_init_all();
    // 電源起動時のCH340/COM列挙を待ってから、取得対象の先頭行を送る。
    // UF2ローダー経由ではCOMが既に列挙済みだが、電源再投入ではこの待ちが必要。
    sleep_ms(kBootUartSettleMs);
    log_boot_identity("stdio", watchdog_reboot, clock_ok);
    sleep_ms(100);

    std::printf("pico_skyace version %s\r\n", PICO_SKYACE_VERSION_STRING);
    std::printf("BUILD ID id=\"%s\"\r\n", PICO_SKYACE_BUILD_ID);
    // ハング/フリーズ切り分け用。前回起動がウォッチドッグ復帰なら、
    // 画面が固まって自動リセットされたことが分かる。
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

    // ハング対策ウォッチドッグ。ゲームループが
    // 3 秒以内に watchdog_update() を呼べなければ自動リセットする。バグの
    // 根本解決ではなく、次回起動ログで「フリーズして自動復帰した」ことを
    // 判別するための保険。
    watchdog_enable(3000, true);

    skyace::game::run();
    return 0;
}
