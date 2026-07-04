#ifndef PICO_SKYACE_PLATFORM_PICOCALC_KEY_TABLE_H_
#define PICO_SKYACE_PLATFORM_PICOCALC_KEY_TABLE_H_

#include <cstdint>

namespace skyace::keys {

constexpr uint8_t None = 0x00;
constexpr uint8_t JoyUp = 0x01;
constexpr uint8_t JoyDown = 0x02;
constexpr uint8_t JoyLeft = 0x03;
constexpr uint8_t JoyRight = 0x04;
constexpr uint8_t JoyCenter = 0x05;
constexpr uint8_t ButtonLeft1 = 0x06;
constexpr uint8_t ButtonRight1 = 0x07;
constexpr uint8_t Backspace = 0x08;
constexpr uint8_t Tab = 0x09;
constexpr uint8_t Enter = 0x0a;
constexpr uint8_t ButtonLeft2 = 0x11;
constexpr uint8_t ButtonRight2 = 0x12;
constexpr uint8_t Space = 0x20;
constexpr uint8_t F5 = 0x85;
constexpr uint8_t Escape = 0xb1;
constexpr uint8_t Left = 0xb4;
constexpr uint8_t Up = 0xb5;
constexpr uint8_t Down = 0xb6;
constexpr uint8_t Right = 0xb7;

inline uint8_t uppercase_ascii(uint8_t key) {
    if (key >= 'a' && key <= 'z') {
        return static_cast<uint8_t>(key - ('a' - 'A'));
    }
    return key;
}

}  // namespace skyace::keys

#endif  // PICO_SKYACE_PLATFORM_PICOCALC_KEY_TABLE_H_
