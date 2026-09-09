#include "game/fixed_math.h"
#include "game/game_rules.h"

#include <cstdio>

namespace rules = skyace::game_rules;
namespace fm = skyace::fixmath;

int g_failures = 0;

void expect(bool condition, const char* message) {
    if (condition) {
        return;
    }
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++g_failures;
}

void test_damage_latches_first_death_reason() {
    const rules::DamageResult partial = rules::apply_damage(
        20, rules::DeathReason::None, 7, rules::DeathReason::ShotDown);
    expect(partial.hp == 13, "non-lethal damage reduces HP");
    expect(partial.death_reason == rules::DeathReason::None,
           "non-lethal damage keeps reason unset");

    const rules::DamageResult lethal = rules::apply_damage(
        partial.hp, partial.death_reason, 20, rules::DeathReason::Collision);
    expect(lethal.hp == 0, "lethal damage clamps HP to zero");
    expect(lethal.death_reason == rules::DeathReason::Collision,
           "lethal event records its cause");

    const rules::DamageResult after_death = rules::apply_damage(
        lethal.hp, lethal.death_reason, 20, rules::DeathReason::ShotDown);
    expect(after_death.hp == 0, "post-death damage does not change HP");
    expect(after_death.death_reason == rules::DeathReason::Collision,
           "post-death damage cannot overwrite the first cause");
}

void test_target_generation() {
    expect(rules::is_same_target(2, 17, 2, true, 17),
           "matching slot and generation is a valid target");
    expect(!rules::is_same_target(2, 17, 2, true, 18),
           "reused slot with a new generation is not the old target");
    expect(!rules::is_same_target(2, 17, 2, false, 17),
           "dead target is invalid");
    expect(!rules::is_same_target(2, 0, 2, true, 0),
           "unassigned generation is invalid");
}

void test_input_hold_is_blocked_until_release() {
    rules::InputState input;
    constexpr uint8_t key = 0x42;

    input.apply_event(key, rules::InputEventState::Pressed);
    expect(input.down[key] && input.pressed[key],
           "pressed input is visible in the current frame");

    input.begin_frame();
    input.block_held();
    expect(!input.down[key] && !input.pressed[key],
           "held input is consumed when a mode transition occurs");

    input.apply_event(key, rules::InputEventState::Hold);
    expect(!input.down[key] && !input.pressed[key],
           "held input remains suppressed after the transition");
    input.apply_event(key, rules::InputEventState::Released);
    input.apply_event(key, rules::InputEventState::Pressed);
    expect(input.down[key] && input.pressed[key],
           "a new press is accepted after release");
}

void test_rng_reproducibility_and_separation() {
    fm::init();

    fm::seed_rng(0x13579bdfu);
    const uint32_t expected = fm::rnd();
    fm::seed_rng(0x13579bdfu);
    expect(expected == fm::rnd(), "gameplay RNG is reproducible from a seed");

    fm::seed_rng(0x2468ace0u);
    const uint32_t before_fx = fm::rnd();
    fm::seed_rng(0x2468ace0u);
    fm::seed_fx_rng(0xabcdef01u);
    (void)fm::rnd_fx();
    expect(before_fx == fm::rnd(),
           "FX RNG does not consume gameplay RNG state");

    fm::seed_rng(0u);
    const uint32_t zero_seed = fm::rnd();
    fm::seed_rng(0u);
    expect(zero_seed == fm::rnd(), "zero seed maps to a reproducible default");
}

int main() {
    test_damage_latches_first_death_reason();
    test_target_generation();
    test_input_hold_is_blocked_until_release();
    test_rng_reproducibility_and_separation();
    return g_failures == 0 ? 0 : 1;
}
