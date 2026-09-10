#ifndef PICO_SKYACE_GAME_GAME_RULES_H_
#define PICO_SKYACE_GAME_GAME_RULES_H_

#include <cstdint>

namespace skyace::game_rules {

// 出撃結果に記録する敗因。None はまだ生存中を表す。
enum class DeathReason : uint8_t { None, ShotDown, Crash, Collision };

struct RunResult {
    int score;
    int wave;
    int kills;
    uint32_t play_ticks;
    DeathReason death_reason;
};

struct DamageResult {
    int hp;
    DeathReason death_reason;
};

// Waveごとの敵AI圧力。ゲーム本体から分離して、後半Waveほど追跡・射撃が
// 強くなる関係をホストテストで固定する。数値はゲーム内のメートル／フレーム
// と、角度のbrad単位を使う。
struct EnemyWaveTuning {
    int aggression_level;
    int attack_chance_pct;
    int attack_distance_m;
    int attack_turn_step_q8;
    int fire_cooldown_frames;
    int fire_distance_m;
    int fire_cone_brad;
    int fire_altitude_tolerance_m;
    int hit_pct;
    int spawn_timer_min;
    int spawn_timer_max;
    int attack_timer_min;
    int attack_timer_max;
};

enum class InputEventState : uint8_t { Pressed = 1, Hold = 2, Released = 3 };

// ハードウェアから切り離した入力ラッチ。状態遷移直後に保持中のキーを
// Released まで抑制するため、ホストからも同じイベント列を注入できる。
struct InputState {
    bool down[256] = {};
    bool pressed[256] = {};
    bool blocked_until_release[256] = {};

    void begin_frame();
    void apply_event(uint8_t key, InputEventState state);
    void block_held();
};

// 既に敗因が確定している場合は、後続イベントで上書きしない。
DamageResult apply_damage(int hp, DeathReason current_reason, int amount,
                          DeathReason cause);

// type固有のattack_bias / fire_cd / hit_pctを渡して、そのWaveの攻撃調整を
// 返す。Wave 1は従来値を基準にし、以降は最大8段階まで強める。
EnemyWaveTuning enemy_wave_tuning(int wave, int attack_bias, int fire_cd,
                                  int hit_pct);

// 敵の基礎速度にWave補正を適用する。戻り値はQ8 m/s。
int32_t scale_enemy_speed(int32_t speed_q8, int wave);

// 敵スロット番号だけでなく、そのスロットの生成世代まで一致した時だけ
// 同じ標的として扱う。generation=0 は未割り当てとして無効にする。
bool is_same_target(int target_idx, uint32_t target_generation, int enemy_idx,
                    bool enemy_alive, uint32_t enemy_generation);

}  // namespace skyace::game_rules

#endif  // PICO_SKYACE_GAME_GAME_RULES_H_
