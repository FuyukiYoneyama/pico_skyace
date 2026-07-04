#ifndef PICO_SKYACE_GAME_FIXED_MATH_H_
#define PICO_SKYACE_GAME_FIXED_MATH_H_

#include <cstdint>

// 角度は brad（256 で一周）。sin/cos は Q12（4096 = 1.0）。
// 位置・速度は Q8（256 = 1.0 メートル）。

namespace skyace::fixmath {

void init();  // 起動時に一度呼ぶ（sin テーブル生成）

int32_t sin_q12(uint8_t brad);
int32_t cos_q12(uint8_t brad);
// tan を Q12 で返す（|角| < 64 brad の範囲で使うこと）
int32_t tan_q12(int32_t brad_signed);

// atan2 を brad で返す（0..255、+z 前方 / +x 右 のとき atan2_brad(dx, dz)）
uint8_t atan2_brad(int32_t x, int32_t z);

// Q12 同士の乗算
inline int32_t mul_q12(int32_t a, int32_t b) {
    return static_cast<int32_t>((static_cast<int64_t>(a) * b) >> 12);
}

uint32_t isqrt64(uint64_t v);

// xorshift 乱数
uint32_t rnd();
inline int32_t rnd_range(int32_t lo, int32_t hi) {
    return lo + static_cast<int32_t>(rnd() % static_cast<uint32_t>(hi - lo + 1));
}

}  // namespace skyace::fixmath

#endif  // PICO_SKYACE_GAME_FIXED_MATH_H_
