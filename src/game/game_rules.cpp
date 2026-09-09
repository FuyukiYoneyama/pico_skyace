#include "game/game_rules.h"

#include <cstring>

namespace skyace::game_rules {

void InputState::begin_frame() {
    std::memset(pressed, 0, sizeof(pressed));
}

void InputState::apply_event(uint8_t key, InputEventState state) {
    if (blocked_until_release[key]) {
        if (state == InputEventState::Released) {
            blocked_until_release[key] = false;
        }
        down[key] = false;
        return;
    }
    switch (state) {
        case InputEventState::Pressed:
            down[key] = true;
            pressed[key] = true;
            break;
        case InputEventState::Hold:
            down[key] = true;
            break;
        case InputEventState::Released:
            down[key] = false;
            break;
        default:
            break;
    }
}

void InputState::block_held() {
    for (int i = 0; i < 256; ++i) {
        if (down[i]) {
            blocked_until_release[i] = true;
        }
        down[i] = false;
        pressed[i] = false;
    }
}

DamageResult apply_damage(int hp, DeathReason current_reason, int amount,
                          DeathReason cause) {
    if (current_reason != DeathReason::None || amount <= 0) {
        return {hp < 0 ? 0 : hp, current_reason};
    }

    hp -= amount;
    if (hp <= 0) {
        return {0, cause};
    }
    return {hp, DeathReason::None};
}

bool is_same_target(int target_idx, uint32_t target_generation, int enemy_idx,
                    bool enemy_alive, uint32_t enemy_generation) {
    return enemy_alive && target_idx == enemy_idx && target_generation != 0 &&
           target_generation == enemy_generation;
}

}  // namespace skyace::game_rules
