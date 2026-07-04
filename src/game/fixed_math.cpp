#include "game/fixed_math.h"

#include <cmath>

namespace skyace::fixmath {
namespace {

int32_t g_sin_table[256];
uint32_t g_rng_state = 0x12345678u;

}  // namespace

void init() {
    for (int i = 0; i < 256; ++i) {
        const float rad = static_cast<float>(i) * 6.28318530718f / 256.0f;
        g_sin_table[i] = static_cast<int32_t>(lroundf(sinf(rad) * 4096.0f));
    }
}

int32_t sin_q12(uint8_t brad) {
    return g_sin_table[brad];
}

int32_t cos_q12(uint8_t brad) {
    return g_sin_table[static_cast<uint8_t>(brad + 64)];
}

int32_t tan_q12(int32_t brad_signed) {
    const uint8_t a = static_cast<uint8_t>(brad_signed & 0xff);
    const int32_t c = cos_q12(a);
    if (c > -64 && c < 64) {
        return (brad_signed >= 0) ? (64 << 12) : -(64 << 12);
    }
    return static_cast<int32_t>((static_cast<int64_t>(sin_q12(a)) << 12) / c);
}

uint8_t atan2_brad(int32_t x, int32_t z) {
    const float rad = atan2f(static_cast<float>(x), static_cast<float>(z));
    int32_t brad = static_cast<int32_t>(lroundf(rad * 256.0f / 6.28318530718f));
    return static_cast<uint8_t>(brad & 0xff);
}

uint32_t isqrt64(uint64_t v) {
    if (v == 0) {
        return 0;
    }
    uint64_t x = v;
    uint64_t guess = 1ull << ((64 - __builtin_clzll(v)) / 2 + 1);
    for (int i = 0; i < 8; ++i) {
        guess = (guess + x / guess) >> 1;
    }
    while (guess * guess > v) {
        --guess;
    }
    while ((guess + 1) * (guess + 1) <= v) {
        ++guess;
    }
    return static_cast<uint32_t>(guess);
}

uint32_t rnd() {
    uint32_t s = g_rng_state;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    g_rng_state = s;
    return s;
}

}  // namespace skyace::fixmath
