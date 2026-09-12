#ifndef PICO_SKYACE_PLATFORM_PICOCALC_AUDIO_H_
#define PICO_SKYACE_PLATFORM_PICOCALC_AUDIO_H_

#include <cstdint>

// PWM (GP26/27) による疑似音源。8kHz のタイマー割り込みでサンプルごとに
// PWM デューティを書き換える「PWM=簡易DAC」方式。
// 単純な単一周波数の自走発振と違い、LFSR ノイズと矩形波トーンを混ぜて
// エンベロープで減衰させるため、実機で「ピー」ではなく「ジャッ」「ドッ」
// 「ゴォー」に近い音になる。ゲームループをブロックしない
// （タイマー割り込みが裏で鳴らし続けるので、毎フレーム呼ぶ関数は不要）。
namespace skyace::audio {

void init();

// BGM の音符列（周波数Hz / 長さms のペア）。freq_hz=0 は休符。
// アセットは実行時に MIDI 等を解釈せず、ビルド前に変換して同梱する方式
// （フォント/画像アセットと同じ方針）。
struct MusicNote {
    uint16_t freq_hz;
    uint16_t duration_ms;
};

// リズムセクションの打点列。type は下記 DrumType、duration_ms は
// 次の打点までの間隔（音自体の長さは音色ごとにエンジン側で固定）。
struct DrumNote {
    uint8_t type;
    uint16_t duration_ms;
};
enum DrumType : uint8_t {
    kDrumRest = 0,   // 休符（間隔のみ進める）
    kDrumKick = 1,   // キック: 150→40Hz の下降スウィープ
    kDrumSnare = 2,  // スネア: 中域トーン+ノイズ
    kDrumHat = 3,    // ハイハット: ごく短いノイズ
    kDrumCrash = 4,  // クラッシュ/オープンハット: 長めのノイズ
    kDrumTom = 5,    // タム: 130→80Hz の短いスウィープ
};

// BGM 再生。効果音・エンジン音とは別の5チャンネル構成:
//   lead  : 主旋律（50%矩形波・ビブラート付き）
//   arp   : アルペジオ（25%パルス・短いプラック減衰）
//   chord : 和音スタブ（25%パルス・持続系）
//   bass  : ベース（50%矩形波）
//   drums : 合成ドラム（キック/スネア/ハット/クラッシュ/タム）
// チャンネルごとに音色（デューティ比・エンベロープ・ビブラート）が違うため、
// 全部同じ矩形波だった旧構成よりバンドらしい鳴りになる。
// nullptr のチャンネルは鳴らさない。
void music_play(const MusicNote* lead, int lead_count,
                const MusicNote* arp, int arp_count,
                const MusicNote* chord, int chord_count,
                const MusicNote* bass, int bass_count,
                const DrumNote* drums, int drums_count, bool loop);
void music_stop();

// エンジン音（連続）。throttle=0 で停止、255 で最大。毎フレーム呼ぶ。
// maneuver は旋回・ピッチ操作による負荷 (0..255)、steering は左右旋回の
// 方向 (-127=左、127=右) で、ピッチ・ノイズ・左右定位をわずかに変化させる。
void set_engine(uint8_t throttle, uint8_t maneuver = 0,
                int8_t steering = 0);

enum class Sfx : uint8_t {
    Gun,        // 機銃: 短いノイズ主体のクリック
    Missile,    // ミサイル発射: トーン+ノイズの下降スウィープ、やや長め
    Explosion,  // 爆発: ノイズ主体の低い唸り、長め
    Hit,        // 被弾: 短い低いバズ
    LockOn,     // ロックオン成立: 短いクリーンな上昇気味のビープ
    EnemyApproach,   // 敵接近: 音程のある電子ブザー
    EnemyGunWarning, // 敵機銃の発射準備: ロック中の連続高音
    WaveClear,  // 面クリア: 明るい上昇音の短いファンファーレ
};
// 単発効果音を鳴らす（1音同時）。重要度の低いSFXは、警告・被弾音の再生中は
// 上書きしない。
void play_sfx(Sfx sfx);

}  // namespace skyace::audio

#endif  // PICO_SKYACE_PLATFORM_PICOCALC_AUDIO_H_
