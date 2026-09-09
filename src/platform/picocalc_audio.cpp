#include "platform/picocalc_audio.h"

#include "config/board_config.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/time.h"

namespace skyace::audio {
namespace {

// PWM は固定 wrap の高速キャリアにして、サンプルごとに duty（=波形の瞬時値）を
// タイマー割り込みで書き換える「PWM=簡易DAC」方式にする。
// 250MHz sysclk / wrap 1023 は約244kHzキャリア、耳には聞こえない帯域。
constexpr uint16_t kPwmWrap = 1023;
constexpr uint32_t kSampleRateHz = 8000;

uint g_slice = 0;
uint g_chan_l = 0;
uint g_chan_r = 0;
repeating_timer_t g_timer;

uint16_t g_lfsr = 0xace1u;

// 16bit Galois LFSR によるホワイトノイズ（タップ: x^16+x^14+x^13+x^11+1 相当）。
inline int32_t next_noise() {
    const uint16_t bit = static_cast<uint16_t>(
        ((g_lfsr >> 0) ^ (g_lfsr >> 2) ^ (g_lfsr >> 3) ^ (g_lfsr >> 5)) & 1u);
    g_lfsr = static_cast<uint16_t>((g_lfsr >> 1) | (bit << 15));
    return static_cast<int32_t>(g_lfsr & 0xff) - 128;
}

// --- エンジン（連続音）: スロットルに応じてピッチと音量が変わる矩形波に
// ノイズを薄く混ぜ、旋回・ピッチ操作中は負荷に応じて唸りを強める ---
volatile uint8_t g_throttle = 0;
volatile uint8_t g_engine_maneuver = 0;
volatile int8_t g_engine_steering = 0;
uint32_t g_engine_phase = 0;

// --- BGM: 4音程チャンネル（リード/アルペジオ/コード/ベース）+ ドラム。
// チャンネルごとに音色パラメータ（振幅・デューティ比・エンベロープ・
// ビブラート）を変えることで、同一波形4本の平板な鳴りを避ける。 ---
enum VoiceSlot : uint8_t {
    kVoiceLead = 0,
    kVoiceArp,
    kVoiceChord,
    kVoiceBass,
    kVoiceCount,
};

struct VoiceTimbre {
    int32_t amplitude;
    uint32_t duty;      // 矩形波の Hi 区間しきい値（0x8000=50%、0x4000=25%）
    uint8_t env_start;  // 音符頭の音量 (0..255)
    uint8_t env_end;    // 音符末尾の音量 (0..255)
    uint16_t pluck_ms;  // >0 ならこの時間で 0 まで減衰する撥弦系（env_* は無視）
    uint8_t vib_depth;  // ビブラート深さ（0=なし。~3 で約1%のピッチ揺れ）
};

// リード: 太い50%矩形波+ビブラートで歌わせる。アルペジオ: 25%パルスを
// 短いプラックで刻んでシーケンサ風に。コード(ブラス系スタブ): 25%パルスの
// 持続音を薄く。ベース: 50%矩形波で土台。
constexpr VoiceTimbre kTimbres[kVoiceCount] = {
    {48, 0x8000u, 235, 150, 0, 3},   // Lead
    {30, 0x4000u, 255, 0, 80, 0},    // Arp
    {24, 0x4000u, 200, 110, 0, 0},   // Chord
    {44, 0x8000u, 235, 120, 0, 0},   // Bass
};

struct MusicVoiceState {
    const MusicNote* notes = nullptr;
    volatile int count = 0;
    volatile int index = 0;
    volatile bool loop = false;
    volatile bool active = false;
    uint32_t samples_left = 0;
    uint32_t note_total = 0;
    uint32_t phase = 0;
};
MusicVoiceState g_voices[kVoiceCount];
uint16_t g_vib_phase = 0;  // リード用ビブラート LFO（約5.9Hz）

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
    v->note_total = (kSampleRateHz * n.duration_ms) / 1000u;
    if (v->note_total == 0) {
        v->note_total = 1;
    }
    v->samples_left = v->note_total;
    ++v->index;
}

int32_t music_voice_render(int slot) {
    MusicVoiceState* v = &g_voices[slot];
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
        const VoiceTimbre& tb = kTimbres[slot];
        uint32_t step = (static_cast<uint32_t>(n.freq_hz) << 16) / kSampleRateHz;
        if (tb.vib_depth > 0) {
            // 三角波 LFO でピッチを ±(vib_depth/256) 揺らす
            const uint16_t ph = g_vib_phase;
            const int32_t tri = (ph < 0x8000u)
                ? static_cast<int32_t>(ph) - 0x4000
                : 0xC000 - static_cast<int32_t>(ph);
            const int32_t adj = (tri * tb.vib_depth) >> 14;  // -depth..+depth
            step = static_cast<uint32_t>(
                static_cast<int32_t>(step) +
                (static_cast<int32_t>(step) * adj) / 256);
        }
        v->phase += step;
        const int32_t tone =
            ((v->phase & 0xffffu) < tb.duty) ? tb.amplitude : -tb.amplitude;
        const uint32_t elapsed = v->note_total - v->samples_left;
        uint32_t env;
        if (tb.pluck_ms > 0) {
            const uint32_t pluck_total = (kSampleRateHz * tb.pluck_ms) / 1000u;
            env = (elapsed >= pluck_total)
                ? 0u
                : (255u * (pluck_total - elapsed)) / pluck_total;
        } else {
            env = tb.env_start -
                  ((static_cast<uint32_t>(tb.env_start - tb.env_end) * elapsed) /
                   v->note_total);
        }
        sample = (tone * static_cast<int32_t>(env)) / 255;
    }
    --v->samples_left;
    return sample;
}

// --- ドラム: 打点ごとに短い一発音を合成する。duration_ms は次の打点までの
// 間隔で、音自体の長さ（length_ms）は音色ごとに固定。 ---
struct DrumTimbre {
    uint16_t length_ms;
    int32_t amplitude;
};
// index = DrumType（0=rest は未使用）
constexpr DrumTimbre kDrumTimbres[6] = {
    {0, 0},      // rest
    {72, 62},    // kick
    {105, 52},   // snare
    {24, 28},    // closed hat
    {180, 34},   // crash / open hat
    {84, 48},    // tom
};

struct DrumVoiceState {
    const DrumNote* notes = nullptr;
    volatile int count = 0;
    volatile int index = 0;
    volatile bool loop = false;
    volatile bool active = false;
    uint32_t seq_samples_left = 0;  // 次の打点までの残り
    uint8_t synth_type = 0;         // 今鳴っている一発音の種別
    uint32_t synth_left = 0;
    uint32_t synth_total = 0;
    uint32_t phase = 0;
};
DrumVoiceState g_drums;

void drum_advance() {
    if (!g_drums.notes || g_drums.count <= 0) {
        g_drums.active = false;
        return;
    }
    if (g_drums.index >= g_drums.count) {
        if (g_drums.loop) {
            g_drums.index = 0;
        } else {
            g_drums.active = false;
            return;
        }
    }
    const DrumNote& n = g_drums.notes[g_drums.index];
    uint32_t seq = (kSampleRateHz * n.duration_ms) / 1000u;
    if (seq == 0) {
        seq = 1;
    }
    g_drums.seq_samples_left = seq;
    if (n.type >= 1 && n.type <= 5) {
        g_drums.synth_type = n.type;
        g_drums.synth_total =
            (kSampleRateHz * kDrumTimbres[n.type].length_ms) / 1000u;
        g_drums.synth_left = g_drums.synth_total;
        g_drums.phase = 0;
    }
    ++g_drums.index;
}

int32_t drum_render() {
    if (!g_drums.active) {
        return 0;
    }
    if (g_drums.seq_samples_left == 0) {
        drum_advance();
    }
    if (!g_drums.active) {
        return 0;
    }
    --g_drums.seq_samples_left;
    if (g_drums.synth_left == 0) {
        return 0;
    }
    const uint8_t type = g_drums.synth_type;
    const uint32_t total = g_drums.synth_total;
    const uint32_t elapsed = total - g_drums.synth_left;
    --g_drums.synth_left;
    const int32_t amp = kDrumTimbres[type].amplitude;
    const uint32_t env = (g_drums.synth_left * 255u) / total;
    int32_t sample = 0;
    switch (type) {
        case kDrumKick: {  // 150→40Hz の下降スウィープでドスッという胴鳴り
            const uint32_t freq = 150u - (110u * elapsed) / total;
            g_drums.phase += (freq << 16) / kSampleRateHz;
            sample = ((g_drums.phase & 0xffffu) < 0x8000u) ? amp : -amp;
            break;
        }
        case kDrumSnare: {  // 中域トーン3割+ノイズ7割
            g_drums.phase += (190u << 16) / kSampleRateHz;
            const int32_t tone =
                ((g_drums.phase & 0xffffu) < 0x8000u) ? amp : -amp;
            const int32_t noise = (next_noise() * amp) / 128;
            sample = (tone * 70 + noise * 185) / 255;
            break;
        }
        case kDrumHat: {  // ノイズに金属的な短いクリックを重ねる
            g_drums.phase += (2800u << 16) / kSampleRateHz;
            const int32_t metal =
                ((g_drums.phase & 0xffffu) < 0x4000u) ? amp : -amp;
            const int32_t noise = (next_noise() * amp) / 128;
            sample = (metal * 42 + noise * 213) / 255;
            break;
        }
        case kDrumCrash: {  // クラッシュ/オープンハットはノイズを長めに残す
            sample = (next_noise() * amp) / 128;
            break;
        }
        case kDrumTom: {  // 130→80Hz の短いスウィープ
            const uint32_t freq = 130u - (50u * elapsed) / total;
            g_drums.phase += (freq << 16) / kSampleRateHz;
            sample = ((g_drums.phase & 0xffffu) < 0x8000u) ? amp : -amp;
            break;
        }
        default:
            break;
    }
    return (sample * static_cast<int32_t>(env)) / 255;
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

bool timer_callback(repeating_timer_t*) {
    int32_t mix = 0;
    int32_t engine_mix = 0;

    const uint8_t throttle = g_throttle;
    const uint8_t maneuver = g_engine_maneuver;
    if (throttle > 0) {
        // 操作負荷を少しだけピッチへ加え、旋回中のエンジンの張りを出す。
        const uint32_t freq =
            50u + (static_cast<uint32_t>(throttle) * 140u) / 255u +
            (static_cast<uint32_t>(maneuver) * 28u) / 255u;
        const uint32_t step = (freq << 16) / kSampleRateHz;
        g_engine_phase += step;
        const bool high = (g_engine_phase & 0xffffu) < 0x8000u;
        const int32_t tone_amp = 32 + throttle / 32 + maneuver / 32;
        const int32_t tone = high ? tone_amp : -tone_amp;
        const int32_t rumble_amp = 10 + throttle / 20 + maneuver / 12;
        const int32_t rumble = (next_noise() * rumble_amp) / 128;
        const int32_t gain = 18 + throttle / 4 + maneuver / 16;
        engine_mix = (tone + rumble) * gain / 96;
        mix += engine_mix;
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

    g_vib_phase = static_cast<uint16_t>(g_vib_phase + 48);  // 約5.9Hz LFO
    mix += music_voice_render(kVoiceLead);
    mix += music_voice_render(kVoiceArp);
    mix += music_voice_render(kVoiceChord);
    mix += music_voice_render(kVoiceBass);
    mix += drum_render();

    // 旋回方向をエンジン成分だけに薄く反映し、BGM/SFXは中央に保つ。
    const int32_t steering = g_engine_steering;
    const int32_t pan = (steering * 56) / 127;
    const int32_t base_mix = mix - engine_mix;
    int32_t left_mix = base_mix + engine_mix * (256 - pan) / 256;
    int32_t right_mix = base_mix + engine_mix * (256 + pan) / 256;
    if (left_mix > 127) {
        left_mix = 127;
    }
    if (left_mix < -128) {
        left_mix = -128;
    }
    if (right_mix > 127) {
        right_mix = 127;
    }
    if (right_mix < -128) {
        right_mix = -128;
    }
    const uint16_t left_level =
        static_cast<uint16_t>(((left_mix + 128) * kPwmWrap) / 255);
    const uint16_t right_level =
        static_cast<uint16_t>(((right_mix + 128) * kPwmWrap) / 255);
    pwm_set_chan_level(g_slice, g_chan_l, left_level);
    pwm_set_chan_level(g_slice, g_chan_r, right_level);
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

void set_engine(uint8_t throttle, uint8_t maneuver, int8_t steering) {
    const uint32_t save = save_and_disable_interrupts();
    g_throttle = throttle;
    g_engine_maneuver = maneuver;
    g_engine_steering = steering;
    restore_interrupts(save);
}

void music_play(const MusicNote* lead, int lead_count,
                const MusicNote* arp, int arp_count,
                const MusicNote* chord, int chord_count,
                const MusicNote* bass, int bass_count,
                const DrumNote* drums, int drums_count, bool loop) {
    const MusicNote* parts[kVoiceCount] = {lead, arp, chord, bass};
    const int counts[kVoiceCount] = {lead_count, arp_count, chord_count,
                                     bass_count};
    const uint32_t save = save_and_disable_interrupts();
    for (int i = 0; i < kVoiceCount; ++i) {
        g_voices[i] = MusicVoiceState{};
        g_voices[i].notes = parts[i];
        g_voices[i].count = counts[i];
        g_voices[i].loop = loop;
        g_voices[i].active = (parts[i] != nullptr && counts[i] > 0);
    }
    g_drums = DrumVoiceState{};
    g_drums.notes = drums;
    g_drums.count = drums_count;
    g_drums.loop = loop;
    g_drums.active = (drums != nullptr && drums_count > 0);
    restore_interrupts(save);
}

void music_stop() {
    const uint32_t save = save_and_disable_interrupts();
    for (int i = 0; i < kVoiceCount; ++i) {
        g_voices[i].active = false;
    }
    g_drums.active = false;
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
