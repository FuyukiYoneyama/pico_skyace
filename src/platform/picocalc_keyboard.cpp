#include "platform/picocalc_keyboard.h"

#include <cstddef>
#include <cstdio>

#include "config/board_config.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

namespace skyace::keyboard {
namespace {

constexpr uint8_t kRegKey = 0x04;
constexpr uint8_t kRegFifo = 0x09;
constexpr uint8_t kRegBacklight = 0x05;
constexpr uint8_t kWriteMask = 0x80;

bool read_reg(uint8_t reg, uint8_t* data, std::size_t len) {
    const int written = i2c_write_blocking(i2c1, board::kKeyboardI2cAddress, &reg, 1, true);
    if (written != 1) {
        return false;
    }
    const int read = i2c_read_blocking(i2c1, board::kKeyboardI2cAddress, data, len, false);
    return read == static_cast<int>(len);
}

}  // namespace

void init() {
    i2c_init(i2c1, board::kKeyboardI2cHz);
    gpio_set_function(board::kKeyboardI2cSda, GPIO_FUNC_I2C);
    gpio_set_function(board::kKeyboardI2cScl, GPIO_FUNC_I2C);
    gpio_pull_up(board::kKeyboardI2cSda);
    gpio_pull_up(board::kKeyboardI2cScl);

    std::printf("KEYBOARD init ok i2c1 addr=0x%02x hz=%lu\n",
                board::kKeyboardI2cAddress,
                static_cast<unsigned long>(board::kKeyboardI2cHz));
}

bool read_event(KeyEvent* event) {
    if (event == nullptr) {
        return false;
    }

    uint8_t key_info[2] = {0, 0};
    if (!read_reg(kRegKey, key_info, sizeof(key_info))) {
        return false;
    }
    if ((key_info[0] & 0x1f) == 0) {
        return false;
    }

    uint8_t fifo_item[2] = {0, 0};
    if (!read_reg(kRegFifo, fifo_item, sizeof(fifo_item))) {
        return false;
    }

    event->state = static_cast<KeyState>(fifo_item[0]);
    event->key = fifo_item[1];
    return event->key != 0;
}

void set_backlight(uint8_t level) {
    uint8_t data[2] = {static_cast<uint8_t>(kRegBacklight | kWriteMask), level};
    const int written = i2c_write_blocking(i2c1, board::kKeyboardI2cAddress, data, 2, false);
    std::printf("BACKLIGHT set level=%u ok=%d\n", level, written == 2);
}

uint8_t read_backlight() {
    uint8_t d[2] = {0, 0};
    read_reg(kRegBacklight, d, sizeof(d));
    return d[1];
}

}  // namespace skyace::keyboard
