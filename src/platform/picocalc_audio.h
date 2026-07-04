#ifndef PICO_SKYACE_PLATFORM_PICOCALC_AUDIO_H_
#define PICO_SKYACE_PLATFORM_PICOCALC_AUDIO_H_

#include <cstdint>

// PWM (GP26/27) による疑似音源。8kHz のタイマー割り込みでサンプルごとに
// PWM デューティを書き換える方式（general/03_AUDIO_PWM.md の PWM=DAC 方式）。
// 単純な単一周波数の自走発振と違い、LFSR ノイズと矩形波トーンを混ぜて
// エンベロープで減衰させるため、実機で「ピー」ではなく「ジャッ」「ドッ」
// 「ゴォー」に近い音になる。ゲームループをブロックしない
// （タイマー割り込みが裏で鳴らし続けるので、毎フレーム呼ぶ関数は不要）。
namespace skyace::audio {

void init();

// BGM の音符列（周波数Hz / 長さms のペア）。freq_hz=0 は休符。
// アセットは実行時に MIDI 等を解釈せず、ビルド前に変換して同梱する
// （general/01_DISPLAY_LCD.md §7 のフォント/画像と同じ方針）。
struct MusicNote {
    uint16_t freq_hz;
    uint16_t duration_ms;
};
// BGM 再生。ゲームの効果音・エンジン音とは別の2チャンネル（旋律+低音）として
// ミックスする（pico_rescue の rescue_bgm.cpp と同じく2声構成）。
// bass が nullptr の場合は旋律のみを鳴らす。
void music_play(const MusicNote* melody, int melody_count,
                const MusicNote* bass, int bass_count, bool loop);
void music_stop();

// エンジン音（連続）。throttle=0 で停止、255 で最大。速度に応じて毎フレーム
// 呼ぶ。ピッチと音量の両方が throttle に追従する。
void set_engine(uint8_t throttle);

enum class Sfx : uint8_t {
    Gun,        // 機銃: 短いノイズ主体のクリック
    Missile,    // ミサイル発射: トーン+ノイズの下降スウィープ、やや長め
    Explosion,  // 爆発: ノイズ主体の低い唸り、長め
    Hit,        // 被弾: 短い低いバズ
    LockOn,     // ロックオン成立: 短いクリーンな上昇気味のビープ
};
// 単発効果音を鳴らす（1音同時、既存のSFXがあれば上書きする）。
void play_sfx(Sfx sfx);

}  // namespace skyace::audio

#endif  // PICO_SKYACE_PLATFORM_PICOCALC_AUDIO_H_
