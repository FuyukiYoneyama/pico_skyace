#include "game/game_rules.h"

#include <cstring>

namespace skyace::game_rules {

namespace {

int aggression_level_for_wave(int wave) {
    if (wave <= 1) {
        return 0;
    }
    const int level = wave - 1;
    return level > 8 ? 8 : level;
}

int floor_value(int value, int floor) {
    return value < floor ? floor : value;
}

}  // namespace

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

EnemyWaveTuning enemy_wave_tuning(int wave, int attack_bias, int fire_cd,
                                  int hit_pct) {
    const int level = aggression_level_for_wave(wave);
    int attack_chance = 35 + attack_bias + level * 8;
    if (attack_chance < 5) {
        attack_chance = 5;
    }
    if (attack_chance > 92) {
        attack_chance = 92;
    }

    int spawn_min = floor_value(40 - level * 2, 24);
    int spawn_max = floor_value(120 - level * 4, spawn_min + 20);
    int attack_min = floor_value(60 - level * 3, 36);
    int attack_max = floor_value(120 - level * 5, attack_min + 20);

    int tuned_fire_cd = floor_value(fire_cd - level * 2, 8);
    int tuned_hit_pct = hit_pct + level * 2;
    if (tuned_hit_pct > 85) {
        tuned_hit_pct = 85;
    }

    int turn_step = 190 + level * 18;
    if (turn_step > 320) {
        turn_step = 320;
    }

    return {
        level,
        attack_chance,
        900 + level * 100,
        turn_step,
        tuned_fire_cd,
        550 + level * 35,
        6 + level / 2,
        // 出現高度のばらつきがあっても、攻撃態勢へ入った機体が
        // そのまま射線を失い続けないようにする。
        220 + level * 8,
        tuned_hit_pct,
        spawn_min,
        spawn_max,
        attack_min,
        attack_max,
    };
}

bool should_begin_aggressive_attack(int living_enemies, int wave_enemies,
                                    uint32_t wave_elapsed_frames) {
    if (wave_enemies <= 0) {
        return false;
    }
    const bool half_or_fewer = living_enemies >= 0 &&
                               living_enemies * 2 <= wave_enemies;
    constexpr uint32_t kAggressiveAfterFrames = 20u * 30u;
    const bool time_limit_passed = wave_elapsed_frames > kAggressiveAfterFrames;
    return half_or_fewer || time_limit_passed;
}

int32_t scale_enemy_speed(int32_t speed_q8, int wave) {
    const int level = aggression_level_for_wave(wave);
    return static_cast<int32_t>(
        (static_cast<int64_t>(speed_q8) * (256 + level * 8)) / 256);
}

bool is_same_target(int target_idx, uint32_t target_generation, int enemy_idx,
                    bool enemy_alive, uint32_t enemy_generation) {
    return enemy_alive && target_idx == enemy_idx && target_generation != 0 &&
           target_generation == enemy_generation;
}

}  // namespace skyace::game_rules
