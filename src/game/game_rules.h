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

// 敵スロット番号だけでなく、そのスロットの生成世代まで一致した時だけ
// 同じ標的として扱う。generation=0 は未割り当てとして無効にする。
bool is_same_target(int target_idx, uint32_t target_generation, int enemy_idx,
                    bool enemy_alive, uint32_t enemy_generation);

}  // namespace skyace::game_rules

#endif  // PICO_SKYACE_GAME_GAME_RULES_H_
