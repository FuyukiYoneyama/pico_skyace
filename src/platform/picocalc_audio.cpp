#include "platform/picocalc_audio.h"

#include "config/board_config.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/time.h"

namespace skyace::audio {
namespace {

// PWM は固定 wrap の高速キャリアにして、サンプルごとに duty（=波形の瞬時値）を
// タイマー割り込みで書き換える「PWM=簡易DAC」方式にする。general/03_AUDIO_PWM.md
// の表で 250MHz sysclk / wrap 1023 は約244kHzキャリア、耳には聞こえない帯域。
constexpr uint16_t kPwmWrap = 1023;
constexpr uint32_t kSampleRateHz = 8000;

uint g_slice = 0;
uint g_chan_l = 0;
uint g_chan_r = 0;
repeating_timer_t g_timer;

// --- エンジン（連続音）: スロットルに応じてピッチと音量が変わる矩形波に
// ノイズを薄く混ぜて排気音っぽくする ---
volatile uint8_t g_throttle = 0;
uint32_t g_engine_phase = 0;

// --- BGM（音符列を順に再生するチャンネル）。ソプラノ・アルト・テノール・
// バスの4声を独立に進行させて同時にミックスする（同時発音数を増やすための
// 拡張）。 ---
struct MusicVoiceState {
    const MusicNote* notes = nullptr;
    volatile int count = 0;
    volatile int index = 0;
    volatile bool loop = false;
    volatile bool active = false;
    uint32_t samples_left = 0;
    uint32_t phase = 0;
    int32_t amplitude = 60;
};
MusicVoiceState g_voice_melody;
MusicVoiceState g_voice_alto;
MusicVoiceState g_voice_tenor;
MusicVoiceState g_voice_bass;

void music_voice_advance(MusicVoiceState* v) {
    if (!v->notes || v->count <= 0) {
        v->active = false;
        return;
    }
    if (v->index >= v->count) {
        if (v->loop) {
            v->index = 0;
        } else {
            v->active = false;
            return;
        }
    }
    const MusicNote& n = v->notes[v->index];
    v->samples_left = (kSampleRateHz * n.duration_ms) / 1000u;
    if (v->samples_left == 0) {
        v->samples_left = 1;
    }
    ++v->index;
}

// ピアノ風に、音符内で減衰させる（アタック直後が一番大きい）。
int32_t music_voice_render(MusicVoiceState* v) {
    if (!v->active) {
        return 0;
    }
    if (v->samples_left == 0) {
        music_voice_advance(v);
    }
    if (!v->active || v->samples_left == 0) {
        return 0;
    }
    const MusicNote& n = v->notes[v->index - 1];
    int32_t sample = 0;
    if (n.freq_hz > 0) {
        const uint32_t step = (static_cast<uint32_t>(n.freq_hz) << 16) / kSampleRateHz;
        v->phase += step;
        const bool high = (v->phase & 0xffffu) < 0x8000u;
        const int32_t tone = high ? v->amplitude : -v->amplitude;
        const uint32_t note_total =
            (kSampleRateHz * static_cast<uint32_t>(n.duration_ms)) / 1000u;
        const uint32_t env =
            note_total ? (v->samples_left * 200u) / note_total : 200u;
        sample = (tone * static_cast<int32_t>(env)) / 255;
    }
    --v->samples_left;
    return sample;
}

// --- 単発効果音: トーン(開始→終了周波数のスウィープ)とノイズを混ぜ、
// 線形エンベロープで減衰させる ---
struct SfxState {
    volatile bool active;
    uint32_t samples_left;
    uint32_t samples_total;
    uint32_t freq_start;
    uint32_t freq_end;
    uint32_t tone_phase;
    uint8_t noise_mix;   // 0..255: ノイズ成分の比率
    uint8_t volume;      // 0..255: 開始音量（エンベロープの初期値）
};
volatile SfxState g_sfx = {};

uint16_t g_lfsr = 0xace1u;

// 16bit Galois LFSR によるホワイトノイズ（タップ: x^16+x^14+x^13+x^11+1 相当）。
inline int32_t next_noise() {
    const uint16_t bit = static_cast<uint16_t>(
        ((g_lfsr >> 0) ^ (g_lfsr >> 2) ^ (g_lfsr >> 3) ^ (g_lfsr >> 5)) & 1u);
    g_lfsr = static_cast<uint16_t>((g_lfsr >> 1) | (bit << 15));
    return static_cast<int32_t>(g_lfsr & 0xff) - 128;
}

bool timer_callback(repeating_timer_t*) {
    int32_t mix = 0;

    const uint8_t throttle = g_throttle;
    if (throttle > 0) {
        const uint32_t freq = 50u + (static_cast<uint32_t>(throttle) * 140u) / 255u;
        const uint32_t step = (freq << 16) / kSampleRateHz;
        g_engine_phase += step;
        const bool high = (g_engine_phase & 0xffffu) < 0x8000u;
        const int32_t tone = high ? 40 : -40;
        const int32_t rumble = (next_noise() * 18) / 128;
        mix += (tone + rumble) * (20 + throttle / 3) / 64;
    }

    if (g_sfx.active) {
        const uint32_t total = g_sfx.samples_total;
        const uint32_t elapsed = total - g_sfx.samples_left;
        const int32_t delta = static_cast<int32_t>(g_sfx.freq_end) -
                              static_cast<int32_t>(g_sfx.freq_start);
        const uint32_t freq = static_cast<uint32_t>(
            static_cast<int32_t>(g_sfx.freq_start) +
            (delta * static_cast<int32_t>(elapsed)) / static_cast<int32_t>(total));
        const uint32_t step = (freq << 16) / kSampleRateHz;
        g_sfx.tone_phase += step;
        const bool high = (g_sfx.tone_phase & 0xffffu) < 0x8000u;
        const int32_t tone = high ? 100 : -100;
        const int32_t noise = next_noise();
        const uint8_t noise_mix = g_sfx.noise_mix;
        const int32_t sample =
            (tone * (255 - noise_mix) + noise * noise_mix) / 255;
        const uint32_t env = (g_sfx.samples_left * g_sfx.volume) / total;
        mix += (sample * static_cast<int32_t>(env)) / 255;

        --g_sfx.samples_left;
        if (g_sfx.samples_left == 0) {
            g_sfx.active = false;
        }
    }

    mix += music_voice_render(&g_voice_melody);
    mix += music_voice_render(&g_voice_alto);
    mix += music_voice_render(&g_voice_tenor);
    mix += music_voice_render(&g_voice_bass);

    if (mix > 127) {
        mix = 127;
    }
    if (mix < -128) {
        mix = -128;
    }
    const uint16_t level =
        static_cast<uint16_t>(((mix + 128) * kPwmWrap) / 255);
    pwm_set_chan_level(g_slice, g_chan_l, level);
    pwm_set_chan_level(g_slice, g_chan_r, level);
    return true;
}

void trigger(uint32_t freq_start, uint32_t freq_end, uint32_t ms,
             uint8_t noise_mix, uint8_t volume) {
    uint32_t samples_total = (kSampleRateHz * ms) / 1000u;
    if (samples_total == 0) {
        samples_total = 1;
    }
    const uint32_t save = save_and_disable_interrupts();
    g_sfx.freq_start = freq_start;
    g_sfx.freq_end = freq_end;
    g_sfx.tone_phase = 0;
    g_sfx.noise_mix = noise_mix;
    g_sfx.volume = volume;
    g_sfx.samples_total = samples_total;
    g_sfx.samples_left = samples_total;
    g_sfx.active = true;
    restore_interrupts(save);
}

}  // namespace

void init() {
    gpio_set_function(board::kAudioPwmLeft, GPIO_FUNC_PWM);
    gpio_set_function(board::kAudioPwmRight, GPIO_FUNC_PWM);
    g_slice = pwm_gpio_to_slice_num(board::kAudioPwmLeft);
    g_chan_l = pwm_gpio_to_channel(board::kAudioPwmLeft);
    g_chan_r = pwm_gpio_to_channel(board::kAudioPwmRight);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 1.0f);
    pwm_config_set_wrap(&config, kPwmWrap);
    pwm_init(g_slice, &config, true);
    pwm_set_chan_level(g_slice, g_chan_l, kPwmWrap / 2);
    pwm_set_chan_level(g_slice, g_chan_r, kPwmWrap / 2);

    add_repeating_timer_us(-(1000000 / static_cast<int32_t>(kSampleRateHz)),
                           timer_callback, nullptr, &g_timer);
}

void set_engine(uint8_t throttle) {
    g_throttle = throttle;
}

void music_play(const MusicNote* melody, int melody_count,
                const MusicNote* alto, int alto_count,
                const MusicNote* tenor, int tenor_count,
                const MusicNote* bass, int bass_count, bool loop) {
    const uint32_t save = save_and_disable_interrupts();
    g_voice_melody = MusicVoiceState{};
    g_voice_melody.notes = melody;
    g_voice_melody.count = melody_count;
    g_voice_melody.loop = loop;
    g_voice_melody.amplitude = 55;
    g_voice_melody.active = (melody != nullptr && melody_count > 0);

    g_voice_alto = MusicVoiceState{};
    g_voice_alto.notes = alto;
    g_voice_alto.count = alto_count;
    g_voice_alto.loop = loop;
    g_voice_alto.amplitude = 38;
    g_voice_alto.active = (alto != nullptr && alto_count > 0);

    g_voice_tenor = MusicVoiceState{};
    g_voice_tenor.notes = tenor;
    g_voice_tenor.count = tenor_count;
    g_voice_tenor.loop = loop;
    g_voice_tenor.amplitude = 36;
    g_voice_tenor.active = (tenor != nullptr && tenor_count > 0);

    g_voice_bass = MusicVoiceState{};
    g_voice_bass.notes = bass;
    g_voice_bass.count = bass_count;
    g_voice_bass.loop = loop;
    g_voice_bass.amplitude = 46;
    g_voice_bass.active = (bass != nullptr && bass_count > 0);
    restore_interrupts(save);
}

void music_stop() {
    const uint32_t save = save_and_disable_interrupts();
    g_voice_melody.active = false;
    g_voice_alto.active = false;
    g_voice_tenor.active = false;
    g_voice_bass.active = false;
    restore_interrupts(save);
}

void play_sfx(Sfx sfx) {
    switch (sfx) {
        case Sfx::Gun:
            trigger(1200, 500, 40, 150, 220);
            break;
        case Sfx::Missile:
            trigger(700, 150, 350, 120, 220);
            break;
        case Sfx::Explosion:
            trigger(300, 50, 500, 230, 255);
            break;
        case Sfx::Hit:
            trigger(180, 90, 150, 200, 220);
            break;
        case Sfx::LockOn:
            trigger(1400, 2000, 90, 20, 200);
            break;
    }
}

}  // namespace skyace::audio
