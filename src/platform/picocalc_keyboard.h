#ifndef PICO_SKYACE_PLATFORM_PICOCALC_KEYBOARD_H_
#define PICO_SKYACE_PLATFORM_PICOCALC_KEYBOARD_H_

#include <cstdint>

namespace skyace::keyboard {

enum class KeyState : uint8_t {
    Idle = 0,
    Pressed = 1,
    Hold = 2,
    Released = 3,
};

struct KeyEvent {
    KeyState state = KeyState::Idle;
    uint8_t key = 0;
};

void init();
bool read_event(KeyEvent* event);

// バックライト明るさ 0..255。SD ローダー経由の起動では前のプログラムが
// 暗くした/消した値のまま渡ってくることがあるため、自前で明るさを保証する。
// 戻り値は I2C 書き込みが成功したか（電源投入直後は STM32 側キーボード
// コントローラがまだ I2C に応答できないことがあるため、呼び出し側で
// リトライする前提）。
bool set_backlight(uint8_t level);
uint8_t read_backlight();

}  // namespace skyace::keyboard

#endif  // PICO_SKYACE_PLATFORM_PICOCALC_KEYBOARD_H_
