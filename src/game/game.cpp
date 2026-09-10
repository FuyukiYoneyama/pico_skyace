#include "game/game.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "game/fixed_math.h"
#include "game/bgm_track.h"
#include "game/game_rules.h"
#include "game/gfx.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "platform/picocalc_audio.h"
#include "platform/picocalc_display.h"
#include "platform/picocalc_key_table.h"
#include "platform/picocalc_keyboard.h"
#include "platform/screenshot_capture.h"

#ifndef PICO_SKYACE_VERSION_STRING
#define PICO_SKYACE_VERSION_STRING "0.0.0-dev"
#endif

// エミュレーターの短時間シナリオだけがビルド時に上書きできるようにする。
// 製品ビルドでは未定義のため、仕様値の30秒を使う。
#ifndef PICO_SKYACE_TITLE_DEMO_DELAY_MS
#define PICO_SKYACE_TITLE_DEMO_DELAY_MS 30000u
#endif

namespace skyace::game {
namespace {

namespace fm = skyace::fixmath;
namespace rules = skyace::game_rules;

// ---------------------------------------------------------------- 定数

constexpr int kFrameMs = 33;         // 約 30fps
constexpr uint32_t kGameOverAutoReturnMs = 10000;
constexpr uint32_t kTitleDemoDelayMs = PICO_SKYACE_TITLE_DEMO_DELAY_MS;
constexpr uint32_t kDemoDurationMs = 30000;
constexpr uint64_t kGameOverAutoReturnUs =
    static_cast<uint64_t>(kGameOverAutoReturnMs) * 1000u;
constexpr uint64_t kTitleDemoDelayUs =
    static_cast<uint64_t>(kTitleDemoDelayMs) * 1000u;
constexpr uint64_t kDemoDurationUs =
    static_cast<uint64_t>(kDemoDurationMs) * 1000u;
constexpr int kFocal = 110;          // 内部解像度での焦点距離（px）
constexpr int kCx = 80;              // 画面中心
constexpr int kCy = 80;
constexpr int kReticleY = 72;        // 照準の画面 y

constexpr int kMaxEnemies = 5;
constexpr int kMaxMissiles = 4;
constexpr int kMaxExplosions = 6;
constexpr int kAttackWarningFrames = 18;  // 約0.6秒の予告

constexpr int32_t kQ8 = 256;

// 色
constexpr uint16_t kColHudGreen = gfx::rgb(60, 255, 90);
constexpr uint16_t kColHudDim = gfx::rgb(30, 120, 50);
constexpr uint16_t kColRed = gfx::rgb(255, 40, 40);
constexpr uint16_t kColYellow = gfx::rgb(255, 230, 60);
constexpr uint16_t kColWhite = gfx::rgb(255, 255, 255);
constexpr uint16_t kColOrange = gfx::rgb(255, 140, 30);

constexpr uint16_t kColSky0 = gfx::rgb(10, 32, 120);    // 天頂
constexpr uint16_t kColSky1 = gfx::rgb(28, 62, 158);
constexpr uint16_t kColSky2 = gfx::rgb(64, 108, 198);
constexpr uint16_t kColSky3 = gfx::rgb(120, 162, 226);  // 地平線近く
constexpr uint16_t kColHaze = gfx::rgb(188, 202, 224);
constexpr uint16_t kColGndA = gfx::rgb(38, 112, 44);
constexpr uint16_t kColGndB = gfx::rgb(50, 134, 56);
constexpr uint16_t kColGndFar = gfx::rgb(96, 138, 122);

// ---------------------------------------------------------------- 入力

struct Input : rules::InputState {
    void poll() {
        keyboard::KeyEvent ev;
        while (keyboard::read_event(&ev)) {
            const uint8_t key = keys::uppercase_ascii(ev.key);
            apply_event(key, static_cast<rules::InputEventState>(ev.state));
        }
    }
};

// ---------------------------------------------------------------- 状態

enum class Mode : uint8_t { Title, Play, Pause, GameOver, Demo };
using DeathReason = rules::DeathReason;
using RunResult = rules::RunResult;

struct Player {
    int32_t x, y, z;      // Q8 メートル（y = 高度）
    uint16_t yaw_q8;      // brad Q8（wrap）
    int16_t pitch_q8;     // brad Q8、± に制限
    int16_t roll;         // brad（見た目 + 旋回率）
    int32_t speed_q8;     // m/s Q8
    int hp;
    int missiles;
    int gun_cd;
    int gun_flash;
    int hit_flash;
    int lock_target;      // -1 = 無し
    uint32_t lock_generation;
    int lock_timer;
    int guide_target;     // -1 = 無し（ロックとは独立した案内対象）
    uint32_t guide_generation;
    int dmg_flash;
};

enum class EnemyMode : uint8_t { Cruise, Turn, Attack, Evade };
enum class EnemyType : uint8_t { Fighter, Bomber, Interceptor };

struct Enemy {
    bool alive;
    uint32_t generation;
    int32_t x, y, z;
    uint16_t yaw_q8;
    int32_t speed_q8;
    int hp;
    EnemyMode mode;
    EnemyType type;
    int8_t turn_dir;
    int16_t timer;
    int16_t fire_cd;
    int16_t warning_timer;
    // 直近フレームの投影キャッシュ（HUD・照準判定用）
    bool vis;
    int sx, sy;
    int32_t zc;
    int pixw;
};

// 敵タイプ別のパラメータ。Fighter が既存のバランス、Bomber は低速・高耐久・
// 回避しない（missileで狙いやすいがgunでは削りにくい大型機）、Interceptor は
// 高速・低耐久・高命中率（gunで落としやすいが被弾しやすい）という役割分担。
struct EnemyStats {
    int hp;
    int32_t speed_min, speed_max;  // m/s
    int score;
    int16_t fire_cd;    // 射撃後のクールダウン（フレーム）
    int hit_pct;        // 命中率 %
    int player_dmg;
    int collision_dmg;
    int32_t wingspan_m;  // 描画スケール計算用の見かけの機体サイズ
    bool can_evade;
    int attack_bias;    // モード抽選での Attack 優先度（%pt 加算）
};

EnemyStats enemy_stats(EnemyType type) {
    switch (type) {
        case EnemyType::Bomber:
            return {180, 40, 55, 200, 40, 45, 10, 45, 19, false, -20};
        case EnemyType::Interceptor:
            return {60, 100, 130, 150, 18, 70, 6, 25, 11, true, 25};
        case EnemyType::Fighter:
        default:
            return {100, 60, 90, 100, 26, 60, 7, 30, 14, true, 0};
    }
}

EnemyType pick_enemy_type(int wave) {
    if (wave <= 1) {
        return EnemyType::Fighter;
    }
    const uint32_t r = fm::rnd() % 100;
    if (wave == 2) {
        return r < 70 ? EnemyType::Fighter : EnemyType::Bomber;
    }
    if (r < 45) {
        return EnemyType::Fighter;
    }
    if (r < 75) {
        return EnemyType::Bomber;
    }
    return EnemyType::Interceptor;
}

struct Missile {
    bool active;
    int32_t x, y, z;
    int32_t vx, vy, vz;   // Q8 m/s
    int target;
    uint32_t target_generation;
    int16_t life;
    int trail_n;
    int trail_head;
    int32_t trail[6][3];
};

struct Explosion {
    bool active;
    int32_t x, y, z;
    int age;
};

Input g_in;
Mode g_mode = Mode::Title;
bool g_demo_mode = false;
Player g_pl;
Enemy g_en[kMaxEnemies];
Missile g_ms[kMaxMissiles];
Explosion g_ex[kMaxExplosions];

uint32_t g_frame = 0;
uint32_t g_play_tick = 0;
uint32_t g_demo_timer = 0;
uint32_t g_enemy_generation = 0;
int g_demo_attack_cooldown = 0;
int g_wave = 0;
int g_score = 0;
int g_kills = 0;
int g_banner_timer = 0;
int g_msg_timer = 0;
char g_msg[24] = "";
bool g_crashed = false;
DeathReason g_death_reason = DeathReason::None;
RunResult g_result{};

// モード遷移はフレーム数ではなくRP2040の仮想時刻で判定する。描画やLCD
// 転送が一時的に遅くなっても、製品仕様の30秒／10秒からずれないようにする。
uint64_t g_title_deadline_us = 0;
uint64_t g_demo_deadline_us = 0;
uint64_t g_gameover_deadline_us = 0;

// 描画用（フレーム毎に計算）
int g_hy = 80;            // 地平線の中心 y
int32_t g_slope_q8 = 0;   // 地平線の傾き
int32_t g_cosr = 4096;    // ロール回転
int32_t g_sinr = 0;

// ---------------------------------------------------------------- スプライト

const char* const kPlaneLevel[] = {
    "...............W...............",
    "...............W...............",
    "..............WWW..............",
    "..............WCW..............",
    "......R.......WCW.......R......",
    "......RR......WWW......RR......",
    ".W....RRW...WWWWWWW...WRR....W.",
    ".WWWWWWWWWWWWWWWWWWWWWWWWWWWWW.",
    ".W....RR..WWWOOOOOWWW..RR....W.",
    "..........WW.OYYYO.WW..........",
    ".............OOOOO.............",
    "..............OOO..............",
};
constexpr int kPlaneRows = 12;

// 左バンク（左翼が下がる）
const char* const kPlaneBankLeft[] = {
    "...............W...............",
    "...............W..........RR..",
    "..............WWW........RRW...",
    "..............WCW......WWWW....",
    "..............WCW....WWWW....W.",
    "..............WWWW..WWWW....WW.",
    "...........WWWWWWWWWWWW....WW..",
    ".........WWWWWWWWWWWW......W...",
    ".W....WWWWWWWOOOOOWW...........",
    ".WW.WWWW..WW.OYYYO.WW..........",
    ".RRWWW.......OOOOO.............",
    ".RR...........OOO..............",
};

const char* const kEnemyArt[] = {
    "......X......",
    "......X......",
    ".....XXX.....",
    ".XX.XXRXX.XX.",
    ".XXXXXXXXXXX.",
    "..X..XXX..X..",
    "......X......",
};
constexpr int kEnemyRows = 7;

// 右バンクは左バンクの左右反転（起動時に生成）
char g_bank_right_buf[kPlaneRows][40];
const char* g_bank_right[kPlaneRows];

void init_mirrored_art() {
    for (int r = 0; r < kPlaneRows; ++r) {
        const char* src = kPlaneBankLeft[r];
        const int len = static_cast<int>(std::strlen(src));
        for (int i = 0; i < len; ++i) {
            g_bank_right_buf[r][i] = src[len - 1 - i];
        }
        g_bank_right_buf[r][len] = '\0';
        g_bank_right[r] = g_bank_right_buf[r];
    }
}

uint16_t plane_palette(char c) {
    switch (c) {
        case 'W': return gfx::rgb(196, 200, 210);
        case 'C': return gfx::rgb(80, 200, 255);
        case 'R': return gfx::rgb(220, 40, 40);
        case 'O': return kColOrange;
        case 'Y': return kColYellow;
        default: return kColWhite;
    }
}

uint16_t enemy_palette_fighter(char c) {
    switch (c) {
        case 'X': return gfx::rgb(70, 66, 78);
        case 'R': return gfx::rgb(255, 60, 60);
        default: return gfx::rgb(70, 66, 78);
    }
}

uint16_t enemy_palette_bomber(char c) {
    switch (c) {
        case 'X': return gfx::rgb(74, 82, 58);
        case 'R': return gfx::rgb(255, 170, 40);
        default: return gfx::rgb(74, 82, 58);
    }
}

uint16_t enemy_palette_interceptor(char c) {
    switch (c) {
        case 'X': return gfx::rgb(60, 84, 100);
        case 'R': return gfx::rgb(80, 210, 255);
        default: return gfx::rgb(60, 84, 100);
    }
}

gfx::PaletteFn enemy_palette_fn(EnemyType type) {
    switch (type) {
        case EnemyType::Bomber: return enemy_palette_bomber;
        case EnemyType::Interceptor: return enemy_palette_interceptor;
        case EnemyType::Fighter:
        default: return enemy_palette_fighter;
    }
}

uint16_t enemy_radar_color(EnemyType type) {
    switch (type) {
        case EnemyType::Bomber: return kColOrange;
        case EnemyType::Interceptor: return gfx::rgb(80, 210, 255);
        case EnemyType::Fighter:
        default: return kColRed;
    }
}

// ---------------------------------------------------------------- 投影

struct Proj {
    bool ok;
    int sx, sy;      // ロール適用済みスクリーン座標
    int32_t zc;      // カメラ空間の奥行き（Q8 m）
    int32_t xc, yc;  // カメラ空間の左右・上下（Q8 m）
};

void apply_roll(int sx0, int sy0, int* sx, int* sy) {
    const int32_t dx = sx0 - kCx;
    const int32_t dy = sy0 - g_hy;
    *sx = kCx + static_cast<int>((dx * g_cosr - dy * g_sinr) >> 12);
    *sy = g_hy + static_cast<int>((dx * g_sinr + dy * g_cosr) >> 12);
}

void update_camera_transform() {
    const int8_t pitch = static_cast<int8_t>(g_pl.pitch_q8 >> 8);
    const int32_t tan_p = fm::tan_q12(pitch);
    g_hy = kCy + static_cast<int>((static_cast<int64_t>(kFocal) * tan_p) >> 12);
    if (g_hy < -400) {
        g_hy = -400;
    }
    if (g_hy > 560) {
        g_hy = 560;
    }

    const int32_t roll = -g_pl.roll;
    g_cosr = fm::cos_q12(static_cast<uint8_t>(roll & 0xff));
    g_sinr = fm::sin_q12(static_cast<uint8_t>(roll & 0xff));
    g_slope_q8 = fm::tan_q12(roll) >> 4;
    if (g_slope_q8 > 900) {
        g_slope_q8 = 900;
    }
    if (g_slope_q8 < -900) {
        g_slope_q8 = -900;
    }
}

Proj project(int32_t wx, int32_t wy, int32_t wz) {
    Proj p{};
    const int64_t dx = wx - g_pl.x;
    const int64_t dy = wy - g_pl.y;
    const int64_t dz = wz - g_pl.z;

    const uint8_t yaw = static_cast<uint8_t>(g_pl.yaw_q8 >> 8);
    const int32_t sy_ = fm::sin_q12(yaw);
    const int32_t cy_ = fm::cos_q12(yaw);
    const int32_t xc = static_cast<int32_t>((dx * cy_ - dz * sy_) >> 12);
    const int32_t z1 = static_cast<int32_t>((dx * sy_ + dz * cy_) >> 12);

    const int8_t pitch = static_cast<int8_t>(g_pl.pitch_q8 >> 8);
    const int32_t sp = fm::sin_q12(static_cast<uint8_t>(pitch));
    const int32_t cp = fm::cos_q12(static_cast<uint8_t>(pitch));
    const int32_t zc = static_cast<int32_t>(
        (static_cast<int64_t>(z1) * cp + dy * sp) >> 12);
    const int32_t yc = static_cast<int32_t>(
        (dy * cp - static_cast<int64_t>(z1) * sp) >> 12);

    p.zc = zc;
    p.xc = xc;
    p.yc = yc;

    if (zc < 3 * kQ8) {
        p.ok = false;
        return p;
    }
    const int sx0 = kCx + static_cast<int>(
        (static_cast<int64_t>(xc) * kFocal) / zc);
    const int sy0 = kCy - static_cast<int>(
        (static_cast<int64_t>(yc) * kFocal) / zc);
    apply_roll(sx0, sy0, &p.sx, &p.sy);
    p.ok = (p.sx > -80 && p.sx < 240 && p.sy > -80 && p.sy < 240);
    return p;
}

void play_game_sfx(audio::Sfx sfx) {
    // デモはタイトル曲を主役にするため、同じ戦闘処理を通っても効果音で
    // 曲を汚さない。実プレイでは従来どおり各SFXを鳴らす。
    if (!g_demo_mode) {
        audio::play_sfx(sfx);
    }
}

void refresh_enemy_projection() {
    for (auto& e : g_en) {
        if (!e.alive) {
            e.vis = false;
            e.pixw = 0;
            continue;
        }
        const Proj p = project(e.x, e.y, e.z);
        e.vis = p.ok;
        if (!p.ok) {
            e.pixw = 0;
            continue;
        }
        e.sx = p.sx;
        e.sy = p.sy;
        e.zc = p.zc;
        const int32_t wingspan_m = enemy_stats(e.type).wingspan_m;
        e.pixw = static_cast<int>(
            (static_cast<int64_t>(wingspan_m * kFocal) * kQ8) / p.zc);
    }
}

// ---------------------------------------------------------------- スポーン

void spawn_explosion(int32_t x, int32_t y, int32_t z) {
    play_game_sfx(audio::Sfx::Explosion);
    for (auto& e : g_ex) {
        if (!e.active) {
            e.active = true;
            e.x = x;
            e.y = y;
            e.z = z;
            e.age = 0;
            return;
        }
    }
}

void spawn_enemy() {
    for (auto& e : g_en) {
        if (e.alive) {
            continue;
        }
        const uint8_t bearing = static_cast<uint8_t>(fm::rnd() & 0xff);
        const int32_t dist = fm::rnd_range(900, 1400) * kQ8;
        e.alive = true;
        ++g_enemy_generation;
        if (g_enemy_generation == 0) {
            ++g_enemy_generation;
        }
        e.generation = g_enemy_generation;
        e.x = g_pl.x + static_cast<int32_t>(
            (static_cast<int64_t>(dist) * fm::sin_q12(bearing)) >> 12);
        e.z = g_pl.z + static_cast<int32_t>(
            (static_cast<int64_t>(dist) * fm::cos_q12(bearing)) >> 12);
        int32_t alt = g_pl.y + fm::rnd_range(-150, 250) * kQ8;
        if (alt < 120 * kQ8) {
            alt = 120 * kQ8;
        }
        if (alt > 2500 * kQ8) {
            alt = 2500 * kQ8;
        }
        e.y = alt;
        e.yaw_q8 = static_cast<uint16_t>(fm::rnd() & 0xffff);
        e.type = pick_enemy_type(g_wave);
        const EnemyStats stats = enemy_stats(e.type);
        e.speed_q8 = rules::scale_enemy_speed(
            fm::rnd_range(stats.speed_min, stats.speed_max) * kQ8,
            g_wave);
        e.hp = stats.hp;
        e.mode = EnemyMode::Cruise;
        e.turn_dir = (fm::rnd() & 1) ? 1 : -1;
        const rules::EnemyWaveTuning tuning = rules::enemy_wave_tuning(
            g_wave, stats.attack_bias, stats.fire_cd, stats.hit_pct);
        e.timer = static_cast<int16_t>(
            fm::rnd_range(tuning.spawn_timer_min, tuning.spawn_timer_max));
        e.fire_cd = 30;
        e.warning_timer = 0;
        e.vis = false;
        std::printf("SPAWN wave=%d type=%d\r\n", g_wave, static_cast<int>(e.type));
        return;
    }
}

int enemies_for_wave(int wave) {
    const int n = 1 + wave;
    return n > kMaxEnemies ? kMaxEnemies : n;
}

void start_wave(int wave) {
    g_wave = wave;
    const int n = enemies_for_wave(wave);
    const rules::EnemyWaveTuning tuning =
        rules::enemy_wave_tuning(wave, 0, 26, 60);
    std::printf("WAVE START wave=%d enemies=%d aggression=%d frame=%lu\r\n",
                wave, n, tuning.aggression_level,
                static_cast<unsigned long>(g_frame));
    for (int i = 0; i < n; ++i) {
        spawn_enemy();
    }
    g_pl.missiles += 4;
    if (g_pl.missiles > 16) {
        g_pl.missiles = 16;
    }
    if (wave > 1) {
        g_pl.hp += 15;
        if (g_pl.hp > 100) {
            g_pl.hp = 100;
        }
    }
    g_banner_timer = 60;
}

void reset_game() {
    std::memset(g_en, 0, sizeof(g_en));
    std::memset(g_ms, 0, sizeof(g_ms));
    std::memset(g_ex, 0, sizeof(g_ex));
    g_enemy_generation = 0;
    g_pl = Player{};
    g_pl.x = 0;
    g_pl.y = 800 * kQ8;
    g_pl.z = 0;
    g_pl.yaw_q8 = 0;
    g_pl.speed_q8 = 90 * kQ8;
    g_pl.hp = 100;
    g_pl.missiles = 8;
    g_pl.lock_target = -1;
    g_pl.lock_generation = 0;
    g_pl.guide_target = -1;
    g_pl.guide_generation = 0;
    g_play_tick = 0;
    g_score = 0;
    g_kills = 0;
    g_crashed = false;
    g_msg_timer = 0;
    g_death_reason = DeathReason::None;
    g_result = {};
    start_wave(1);
}

void set_msg(const char* s) {
    std::snprintf(g_msg, sizeof(g_msg), "%s", s);
    g_msg_timer = 50;
}

void apply_player_damage(int amount, DeathReason reason) {
    if (amount <= 0 || g_death_reason != DeathReason::None) {
        return;
    }
    const rules::DamageResult result = rules::apply_damage(
        g_pl.hp, g_death_reason, amount, reason);
    g_pl.hp = result.hp;
    g_death_reason = result.death_reason;
    g_pl.dmg_flash = 8;
}

uint8_t engine_throttle_for_speed(int32_t speed_q8) {
    int32_t throttle = ((speed_q8 - 45 * kQ8) * 255) / (105 * kQ8);
    if (throttle < 0) {
        throttle = 0;
    }
    if (throttle > 255) {
        throttle = 255;
    }
    return static_cast<uint8_t>(throttle);
}

uint8_t engine_maneuver_for_controls() {
    int maneuver = 0;
    const bool rolling = g_in.down[keys::Left] || g_in.down[keys::Right];
    const bool pitching = g_in.down[keys::Up] || g_in.down[keys::Down];
    if (rolling) {
        maneuver = 170;
    }
    if (pitching && maneuver < 130) {
        maneuver = 130;
    }

    const int roll_load = (g_pl.roll < 0 ? -g_pl.roll : g_pl.roll) * 220 / 44;
    const int pitch_brad = g_pl.pitch_q8 >> 8;
    const int pitch_load =
        (pitch_brad < 0 ? -pitch_brad : pitch_brad) * 200 / 24;
    if (maneuver < roll_load) {
        maneuver = roll_load;
    }
    if (maneuver < pitch_load) {
        maneuver = pitch_load;
    }
    if (maneuver > 255) {
        maneuver = 255;
    }
    return static_cast<uint8_t>(maneuver);
}

int8_t engine_steering_for_controls() {
    if (g_in.down[keys::Left]) {
        return -127;
    }
    if (g_in.down[keys::Right]) {
        return 127;
    }
    int steering = g_pl.roll * 127 / 44;
    if (steering < -127) {
        steering = -127;
    }
    if (steering > 127) {
        steering = 127;
    }
    return static_cast<int8_t>(steering);
}

void set_engine_for_controls() {
    if (g_demo_mode) {
        audio::set_engine(0);
        return;
    }
    int throttle = engine_throttle_for_speed(g_pl.speed_q8);
    // O/Lを押している間は速度の追従を待たず、音にも操作意図を先行反映する。
    if (g_in.down['O']) {
        throttle += 24;
    } else if (g_in.down['L']) {
        throttle -= 20;
    }
    if (throttle < 0) {
        throttle = 0;
    }
    if (throttle > 255) {
        throttle = 255;
    }
    audio::set_engine(static_cast<uint8_t>(throttle),
                      engine_maneuver_for_controls(),
                      engine_steering_for_controls());
}

void play_title_music() {
    audio::music_play(bgm::kLeadNotes, bgm::kLeadNotesCount,
                      bgm::kArpNotes, bgm::kArpNotesCount,
                      bgm::kChordNotes, bgm::kChordNotesCount,
                      bgm::kBassNotes, bgm::kBassNotesCount,
                      bgm::kTitleDrumNotes, bgm::kTitleDrumNotesCount, true);
}

void update_guidance_target();

void enter_play_from_reset(const char* transition) {
    g_demo_mode = false;
    reset_game();
    g_mode = Mode::Play;
    g_demo_timer = 0;
    g_title_deadline_us = 0;
    g_demo_deadline_us = 0;
    g_gameover_deadline_us = 0;
    audio::music_stop();
    audio::set_engine(0);
    std::printf("MODE %s frame=%lu time_us=%lu\r\n", transition,
                static_cast<unsigned long>(g_frame),
                static_cast<unsigned long>(time_us_64()));
    // 初期速度90m/sに対応したエンジン音を、最初の更新前から開始する。
    set_engine_for_controls();
    g_in.block_held();
    update_camera_transform();
    refresh_enemy_projection();
    update_guidance_target();
}

void enter_pause() {
    g_mode = Mode::Pause;
    audio::set_engine(0);
    g_in.block_held();
    std::printf("MODE Play->Pause frame=%lu\r\n",
                static_cast<unsigned long>(g_frame));
}

void resume_play() {
    g_mode = Mode::Play;
    g_gameover_deadline_us = 0;
    g_in.block_held();
    set_engine_for_controls();
    std::printf("MODE Pause->Play frame=%lu\r\n",
                static_cast<unsigned long>(g_frame));
}

void enter_demo() {
    g_demo_mode = true;
    // デモの出撃だけは毎回同じ乱数シードにする。敵の初期配置・AIの分岐は
    // 実プレイと同じだが、タイトル画面を何度見たかで展開が変わらない。
    fm::seed_rng(0xd3e0a11u);
    reset_game();
    g_mode = Mode::Demo;
    g_demo_timer = 0;
    g_title_deadline_us = 0;
    g_demo_deadline_us = time_us_64() + kDemoDurationUs;
    g_gameover_deadline_us = 0;
    g_demo_attack_cooldown = 0;
    // デモ用の自動操縦更新へ切り替える。タイトルから再生中の曲はそのまま
    // 継続し、攻撃の効果音だけを抑制する。
    audio::set_engine(0);
    g_in.block_held();
    std::printf("MODE Title->Demo frame=%lu time_us=%lu\r\n",
                static_cast<unsigned long>(g_frame),
                static_cast<unsigned long>(time_us_64()));
}

void enter_title(const char* transition) {
    g_demo_mode = false;
    g_mode = Mode::Title;
    g_demo_timer = 0;
    g_title_deadline_us = time_us_64() + kTitleDemoDelayUs;
    g_demo_deadline_us = 0;
    g_gameover_deadline_us = 0;
    audio::set_engine(0);
    g_in.block_held();
    std::printf("MODE %s frame=%lu time_us=%lu\r\n", transition,
                static_cast<unsigned long>(g_frame),
                static_cast<unsigned long>(time_us_64()));
    play_title_music();
}

void finalize_gameover() {
    if (g_mode == Mode::GameOver) {
        return;
    }
    if (g_death_reason == DeathReason::None) {
        // HP が外部要因で0になった場合も結果を未確定のままにしない。
        g_death_reason = DeathReason::ShotDown;
    }
    if (g_demo_mode) {
        // デモは結果画面を見せるモードではない。死亡した場合も、同じ
        // reset_game()/start_wave()を使って次の出撃を始め、デモ時間だけを
        // 継続する。敵AI・衝突・被弾の判定自体は実プレイと同じである。
        const DeathReason reason = g_death_reason;
        std::printf("DEMO RESET frame=%lu reason=%d\r\n",
                    static_cast<unsigned long>(g_frame),
                    static_cast<int>(reason));
        reset_game();
        g_mode = Mode::Demo;
        g_demo_mode = true;
        g_demo_attack_cooldown = 0;
        audio::set_engine(0);
        g_in.block_held();
        return;
    }
    g_demo_mode = false;
    g_result = {g_score, g_wave, g_kills, g_play_tick, g_death_reason};
    g_mode = Mode::GameOver;
    g_title_deadline_us = 0;
    g_demo_deadline_us = 0;
    g_gameover_deadline_us = time_us_64() + kGameOverAutoReturnUs;
    g_msg_timer = 0;
    g_banner_timer = 0;
    for (auto& e : g_en) {
        e.warning_timer = 0;
    }
    std::printf("MODE Play->GameOver frame=%lu time_us=%lu wave=%d score=%d ticks=%lu reason=%d\r\n",
                static_cast<unsigned long>(g_frame),
                static_cast<unsigned long>(time_us_64()), g_wave, g_score,
                static_cast<unsigned long>(g_play_tick),
                static_cast<int>(g_death_reason));
    audio::set_engine(0);
    play_title_music();
}

// ---------------------------------------------------------------- 更新

bool fire_missile() {
    if (g_pl.missiles <= 0) {
        set_msg("NO MISSILES");
        return false;
    }
    for (auto& m : g_ms) {
        if (m.active) {
            continue;
        }
        const uint8_t yaw = static_cast<uint8_t>(g_pl.yaw_q8 >> 8);
        const int8_t pitch = static_cast<int8_t>(g_pl.pitch_q8 >> 8);
        const int32_t sp = fm::sin_q12(static_cast<uint8_t>(pitch));
        const int32_t cp = fm::cos_q12(static_cast<uint8_t>(pitch));
        const int32_t fx = fm::mul_q12(fm::sin_q12(yaw), cp);
        const int32_t fz = fm::mul_q12(fm::cos_q12(yaw), cp);
        const int32_t v0 = g_pl.speed_q8 + 80 * kQ8;
        m.active = true;
        m.x = g_pl.x;
        m.y = g_pl.y - 2 * kQ8;
        m.z = g_pl.z;
        m.vx = static_cast<int32_t>((static_cast<int64_t>(v0) * fx) >> 12);
        m.vz = static_cast<int32_t>((static_cast<int64_t>(v0) * fz) >> 12);
        m.vy = static_cast<int32_t>((static_cast<int64_t>(v0) * sp) >> 12);
        const bool locked = g_pl.lock_target >= 0 &&
                            g_pl.lock_target < kMaxEnemies &&
                            g_pl.lock_timer >= 20 &&
                            rules::is_same_target(
                                g_pl.lock_target, g_pl.lock_generation,
                                g_pl.lock_target, g_en[g_pl.lock_target].alive,
                                g_en[g_pl.lock_target].generation);
        m.target = locked ? g_pl.lock_target : -1;
        m.target_generation = locked ? g_en[g_pl.lock_target].generation : 0;
        m.life = 150;  // 5 秒
        m.trail_n = 0;
        m.trail_head = 0;
        --g_pl.missiles;
        play_game_sfx(audio::Sfx::Missile);
        return true;
    }
    set_msg("MSL BUSY");
    return false;
}

void update_player_motion() {
    if (g_pl.dmg_flash > 0) {
        --g_pl.dmg_flash;
    }
    if (g_pl.hit_flash > 0) {
        --g_pl.hit_flash;
    }

    // ロール / ヨー
    int roll_target = 0;
    if (g_in.down[keys::Left]) {
        roll_target = -44;
    } else if (g_in.down[keys::Right]) {
        roll_target = 44;
    }
    if (g_pl.roll < roll_target) {
        g_pl.roll += 4;
        if (g_pl.roll > roll_target) {
            g_pl.roll = roll_target;
        }
    } else if (g_pl.roll > roll_target) {
        g_pl.roll -= 4;
        if (g_pl.roll < roll_target) {
            g_pl.roll = roll_target;
        }
    }
    g_pl.yaw_q8 = static_cast<uint16_t>(g_pl.yaw_q8 + g_pl.roll * 8);

    // ピッチ（上キー = 機首上げ）
    if (g_in.down[keys::Up]) {
        g_pl.pitch_q8 = static_cast<int16_t>(g_pl.pitch_q8 + 80);
    } else if (g_in.down[keys::Down]) {
        g_pl.pitch_q8 = static_cast<int16_t>(g_pl.pitch_q8 - 80);
    } else if (g_pl.pitch_q8 > 0) {
        g_pl.pitch_q8 = static_cast<int16_t>(g_pl.pitch_q8 - 30);
        if (g_pl.pitch_q8 < 0) {
            g_pl.pitch_q8 = 0;
        }
    } else if (g_pl.pitch_q8 < 0) {
        g_pl.pitch_q8 = static_cast<int16_t>(g_pl.pitch_q8 + 30);
        if (g_pl.pitch_q8 > 0) {
            g_pl.pitch_q8 = 0;
        }
    }
    if (g_pl.pitch_q8 > (24 << 8)) {
        g_pl.pitch_q8 = 24 << 8;
    }
    if (g_pl.pitch_q8 < -(24 << 8)) {
        g_pl.pitch_q8 = -(24 << 8);
    }

    // スロットル
    if (g_in.down['O']) {
        g_pl.speed_q8 += 80;
    } else if (g_in.down['L']) {
        g_pl.speed_q8 -= 96;
    } else if (g_pl.speed_q8 > 90 * kQ8) {
        g_pl.speed_q8 -= 8;
    } else if (g_pl.speed_q8 < 90 * kQ8) {
        g_pl.speed_q8 += 8;
    }
    if (g_pl.speed_q8 < 45 * kQ8) {
        g_pl.speed_q8 = 45 * kQ8;
    }
    if (g_pl.speed_q8 > 150 * kQ8) {
        g_pl.speed_q8 = 150 * kQ8;
    }

    // エンジン音: 速度を基準に、スロットル操作と操縦負荷も反映する。
    set_engine_for_controls();

    // 移動
    const uint8_t yaw = static_cast<uint8_t>(g_pl.yaw_q8 >> 8);
    const int8_t pitch = static_cast<int8_t>(g_pl.pitch_q8 >> 8);
    const int32_t sp = fm::sin_q12(static_cast<uint8_t>(pitch));
    const int32_t cp = fm::cos_q12(static_cast<uint8_t>(pitch));
    const int32_t hspeed = static_cast<int32_t>(
        (static_cast<int64_t>(g_pl.speed_q8) * cp) >> 12);
    const int32_t vspeed = static_cast<int32_t>(
        (static_cast<int64_t>(g_pl.speed_q8) * sp) >> 12);
    g_pl.x += static_cast<int32_t>(
        (static_cast<int64_t>(hspeed) * fm::sin_q12(yaw)) >> 12) / 30;
    g_pl.z += static_cast<int32_t>(
        (static_cast<int64_t>(hspeed) * fm::cos_q12(yaw)) >> 12) / 30;
    g_pl.y += vspeed / 30;
    if (g_pl.y > 4000 * kQ8) {
        g_pl.y = 4000 * kQ8;
    }

    // 墜落（爆発は 40m 前方に出してカメラから見えるようにする）
    if (g_pl.y < 4 * kQ8) {
        g_pl.hp = 0;
        g_death_reason = DeathReason::Crash;
        g_crashed = true;
        const int32_t fx = fm::sin_q12(yaw);
        const int32_t fz = fm::cos_q12(yaw);
        spawn_explosion(g_pl.x + ((40 * kQ8 * fx) >> 12),
                        6 * kQ8,
                        g_pl.z + ((40 * kQ8 * fz) >> 12));
        return;
    }
}

bool enemy_reference_alive(int idx, uint32_t generation) {
    if (idx < 0 || idx >= kMaxEnemies) {
        return false;
    }
    return rules::is_same_target(idx, generation, idx, g_en[idx].alive,
                                 g_en[idx].generation);
}

void update_guidance_target() {
    // ロック対象を最優先にする。ロックが外れても、案内対象が生存中なら
    // その機体を維持して、画面外へ出た瞬間に案内が飛び移らないようにする。
    if (enemy_reference_alive(g_pl.lock_target, g_pl.lock_generation)) {
        g_pl.guide_target = g_pl.lock_target;
        g_pl.guide_generation = g_pl.lock_generation;
        return;
    }
    if (enemy_reference_alive(g_pl.guide_target, g_pl.guide_generation)) {
        return;
    }

    int best = -1;
    uint64_t best_d2 = 0;
    for (int i = 0; i < kMaxEnemies; ++i) {
        const Enemy& e = g_en[i];
        if (!e.alive) {
            continue;
        }
        const int64_t dx = e.x - g_pl.x;
        const int64_t dy = e.y - g_pl.y;
        const int64_t dz = e.z - g_pl.z;
        const uint64_t d2 = static_cast<uint64_t>(dx * dx + dy * dy + dz * dz);
        if (best < 0 || d2 < best_d2) {
            best = i;
            best_d2 = d2;
        }
    }
    if (best >= 0) {
        g_pl.guide_target = best;
        g_pl.guide_generation = g_en[best].generation;
    } else {
        g_pl.guide_target = -1;
        g_pl.guide_generation = 0;
    }
}

void update_player_weapons() {
    // 機銃
    if (g_pl.gun_cd > 0) {
        --g_pl.gun_cd;
    }
    if (g_pl.gun_flash > 0) {
        --g_pl.gun_flash;
    }
    if (g_in.down[keys::Space] && g_pl.gun_cd == 0) {
        g_pl.gun_cd = 3;
        g_pl.gun_flash = 3;
        play_game_sfx(audio::Sfx::Gun);
        for (int i = 0; i < kMaxEnemies; ++i) {
            Enemy& e = g_en[i];
            if (!e.alive || !e.vis) {
                continue;
            }
            if (e.zc < 700 * kQ8 &&
                e.sx > kCx - 10 && e.sx < kCx + 10 &&
                e.sy > kReticleY - 10 && e.sy < kReticleY + 10) {
                e.hp -= 9;
                g_pl.hit_flash = 5;
                if (e.hp <= 0) {
                    e.alive = false;
                    ++g_kills;
                    spawn_explosion(e.x, e.y, e.z);
                    const int reward = enemy_stats(e.type).score;
                    g_score += reward;
                    char msg[24];
                    std::snprintf(msg, sizeof(msg), "ENEMY DOWN +%d", reward);
                    set_msg(msg);
                }
                break;
            }
        }
    }

    // ロックオン
    int best = -1;
    int32_t best_z = 0;
    for (int i = 0; i < kMaxEnemies; ++i) {
        const Enemy& e = g_en[i];
        if (!e.alive || !e.vis) {
            continue;
        }
        if (e.zc < 40 * kQ8 || e.zc > 1600 * kQ8) {
            continue;
        }
        if (e.sx < kCx - 26 || e.sx > kCx + 26 ||
            e.sy < kReticleY - 26 || e.sy > kReticleY + 26) {
            continue;
        }
        if (best < 0 || e.zc < best_z) {
            best = i;
            best_z = e.zc;
        }
    }
    if (best >= 0) {
        if (g_pl.lock_target == best &&
            g_pl.lock_generation == g_en[best].generation) {
            if (g_pl.lock_timer < 60) {
                ++g_pl.lock_timer;
                if (g_pl.lock_timer == 20) {
                    play_game_sfx(audio::Sfx::LockOn);
                }
            }
        } else {
            g_pl.lock_target = best;
            g_pl.lock_generation = g_en[best].generation;
            g_pl.lock_timer = 0;
        }
    } else {
        g_pl.lock_target = -1;
        g_pl.lock_generation = 0;
        g_pl.lock_timer = 0;
    }

    update_guidance_target();

    // ミサイル発射
    if (g_in.pressed['M']) {
        fire_missile();
    }

}

void steer_enemy_towards(Enemy& e, uint8_t desired, int max_step_q8) {
    const int8_t diff = static_cast<int8_t>(
        desired - static_cast<uint8_t>(e.yaw_q8 >> 8));
    int32_t step = diff * 32;  // brad差 → Q8 補正
    if (step > max_step_q8) {
        step = max_step_q8;
    }
    if (step < -max_step_q8) {
        step = -max_step_q8;
    }
    e.yaw_q8 = static_cast<uint16_t>(e.yaw_q8 + step);
}

bool missile_targets(int enemy_idx) {
    if (enemy_idx < 0 || enemy_idx >= kMaxEnemies || !g_en[enemy_idx].alive) {
        return false;
    }
    const uint32_t generation = g_en[enemy_idx].generation;
    for (const auto& m : g_ms) {
        if (m.active && rules::is_same_target(
                             m.target, m.target_generation, enemy_idx, true,
                             generation)) {
            return true;
        }
    }
    return false;
}

void update_enemy(int idx) {
    Enemy& e = g_en[idx];
    if (!e.alive) {
        return;
    }

    const EnemyStats stats = enemy_stats(e.type);
    const rules::EnemyWaveTuning tuning = rules::enemy_wave_tuning(
        g_wave, stats.attack_bias, stats.fire_cd, stats.hit_pct);
    const int64_t dx = g_pl.x - e.x;
    const int64_t dz = g_pl.z - e.z;
    const uint32_t dist_m = fm::isqrt64(
        static_cast<uint64_t>(dx * dx + dz * dz)) >> 8;

    // ミサイルに狙われたら回避（Bomber は回避運動をしない鈍重な機体）
    if (e.mode != EnemyMode::Evade && stats.can_evade && missile_targets(idx)) {
        e.mode = EnemyMode::Evade;
        e.turn_dir = (fm::rnd() & 1) ? 1 : -1;
        e.timer = 70;
    }

    if (--e.timer <= 0) {
        // モード遷移（type固有のattack_biasに加え、Wave後半ほど攻撃態勢へ
        // 入りやすくする。Interceptorは積極的、Bomberは相対的に消極的。）
        const uint32_t r = fm::rnd() % 100;
        const int attack_th = tuning.attack_chance_pct;
        const bool close_pressure =
            tuning.aggression_level >= 4 &&
            dist_m < static_cast<uint32_t>(tuning.fire_distance_m);
        if (dist_m > 2500) {
            e.mode = EnemyMode::Attack;  // 離れすぎ → 追跡して戻る
            e.timer = 90;
        } else if ((static_cast<int>(r) < attack_th || close_pressure) &&
                   dist_m < static_cast<uint32_t>(tuning.attack_distance_m)) {
            e.mode = EnemyMode::Attack;
            e.timer = static_cast<int16_t>(fm::rnd_range(
                tuning.attack_timer_min, tuning.attack_timer_max));
        } else if (r < 65) {
            e.mode = EnemyMode::Turn;
            e.turn_dir = (fm::rnd() & 1) ? 1 : -1;
            e.timer = static_cast<int16_t>(fm::rnd_range(30, 80));
        } else {
            e.mode = EnemyMode::Cruise;
            e.timer = static_cast<int16_t>(fm::rnd_range(40, 110));
        }
    }

    switch (e.mode) {
        case EnemyMode::Cruise:
            break;
        case EnemyMode::Turn:
            e.yaw_q8 = static_cast<uint16_t>(e.yaw_q8 + e.turn_dir * 180);
            break;
        case EnemyMode::Attack: {
            const uint8_t bearing = fm::atan2_brad(
                static_cast<int32_t>(dx >> 8), static_cast<int32_t>(dz >> 8));
            steer_enemy_towards(e, bearing, tuning.attack_turn_step_q8);
            break;
        }
        case EnemyMode::Evade:
            e.yaw_q8 = static_cast<uint16_t>(e.yaw_q8 + e.turn_dir * 300);
            break;
    }

    // 高度をゆっくりプレイヤーへ寄せる（回避中は離す）
    const int32_t dy_alt = g_pl.y - e.y;
    const int32_t climb = (e.mode == EnemyMode::Evade)
        ? ((e.turn_dir > 0) ? 18 * kQ8 : -18 * kQ8)
        : ((dy_alt > 0) ? 12 * kQ8 : -12 * kQ8);
    if (e.mode != EnemyMode::Evade && (dy_alt > -8 * kQ8 && dy_alt < 8 * kQ8)) {
        // 高度差が小さければ維持
    } else {
        e.y += climb / 30;
    }
    if (e.y < 80 * kQ8) {
        e.y = 80 * kQ8;
    }
    if (e.y > 3200 * kQ8) {
        e.y = 3200 * kQ8;
    }

    // 前進
    const uint8_t yaw = static_cast<uint8_t>(e.yaw_q8 >> 8);
    e.x += static_cast<int32_t>(
        (static_cast<int64_t>(e.speed_q8) * fm::sin_q12(yaw)) >> 12) / 30;
    e.z += static_cast<int32_t>(
        (static_cast<int64_t>(e.speed_q8) * fm::cos_q12(yaw)) >> 12) / 30;

    // 攻撃（プレイヤーが正面コーンに入っていれば、まず予告してから射撃）
    if (e.fire_cd > 0) {
        --e.fire_cd;
    }
    if (e.mode != EnemyMode::Attack) {
        e.warning_timer = 0;
    } else {
        const int64_t attack_dx = g_pl.x - e.x;
        const int64_t attack_dz = g_pl.z - e.z;
        const uint32_t attack_dist_m = fm::isqrt64(
            static_cast<uint64_t>(attack_dx * attack_dx +
                                  attack_dz * attack_dz)) >> 8;
        const int64_t attack_dy = g_pl.y - e.y;
        const uint8_t attack_bearing = fm::atan2_brad(
            static_cast<int32_t>(attack_dx >> 8),
            static_cast<int32_t>(attack_dz >> 8));
        const int8_t attack_diff = static_cast<int8_t>(attack_bearing - yaw);
        const bool attack_geometry =
            attack_dist_m < static_cast<uint32_t>(tuning.fire_distance_m) &&
            attack_dist_m > 40 &&
            attack_diff > -tuning.fire_cone_brad &&
            attack_diff < tuning.fire_cone_brad &&
            attack_dy > -tuning.fire_altitude_tolerance_m * kQ8 &&
            attack_dy < tuning.fire_altitude_tolerance_m * kQ8;

        if (!attack_geometry) {
            // 予告後に旋回・上昇などで射線を外せば、その攻撃を取り消す。
            e.warning_timer = 0;
        } else if (e.warning_timer > 0) {
            --e.warning_timer;
            if (e.warning_timer == 0 && e.fire_cd == 0) {
                e.fire_cd = static_cast<int16_t>(tuning.fire_cooldown_frames);
                if (static_cast<int>(fm::rnd() % 100) < tuning.hit_pct) {
                    apply_player_damage(stats.player_dmg, DeathReason::ShotDown);
                    play_game_sfx(audio::Sfx::Hit);
                    set_msg("TAKING FIRE!");
                    if (g_death_reason != DeathReason::None) {
                        return;
                    }
                }
            }
        } else if (e.fire_cd == 0) {
            e.warning_timer = kAttackWarningFrames;
        }
    }

    // 体当たり判定
    const int64_t collision_dx = g_pl.x - e.x;
    const int64_t dy = g_pl.y - e.y;
    const int64_t collision_dz = g_pl.z - e.z;
    const uint64_t d2 = static_cast<uint64_t>(
        collision_dx * collision_dx + dy * dy + collision_dz * collision_dz);
    const uint64_t rr = static_cast<uint64_t>(25 * kQ8) * (25 * kQ8);
    if (d2 < rr) {
        e.alive = false;
        spawn_explosion(e.x, e.y, e.z);
        g_score += stats.score / 2;
        set_msg("MIDAIR COLLISION!");
        apply_player_damage(stats.collision_dmg, DeathReason::Collision);
        g_pl.dmg_flash = 12;
    }
}

void update_missiles() {
    for (auto& m : g_ms) {
        if (!m.active) {
            continue;
        }
        if (--m.life <= 0) {
            m.active = false;
            continue;
        }

        // 誘導。敵スロットが再利用されても、世代が違う機体は追跡しない。
        if (m.target >= 0 && m.target < kMaxEnemies &&
            rules::is_same_target(
                m.target, m.target_generation, m.target, g_en[m.target].alive,
                g_en[m.target].generation)) {
            const Enemy& t = g_en[m.target];
            const int64_t dx = t.x - m.x;
            const int64_t dy = t.y - m.y;
            const int64_t dz = t.z - m.z;
            const uint32_t d = fm::isqrt64(
                static_cast<uint64_t>(dx * dx + dy * dy + dz * dz));
            if (d > 0) {
                const int32_t vmax = 300 * kQ8;
                const int32_t wx = static_cast<int32_t>(dx * vmax / d);
                const int32_t wy = static_cast<int32_t>(dy * vmax / d);
                const int32_t wz = static_cast<int32_t>(dz * vmax / d);
                m.vx += (wx - m.vx) >> 3;
                m.vy += (wy - m.vy) >> 3;
                m.vz += (wz - m.vz) >> 3;
            }
        } else if (m.target >= 0) {
            m.target = -1;
            m.target_generation = 0;
        }

        // 煙トレイル（2 フレームに 1 点）
        if ((g_frame & 1) == 0) {
            m.trail[m.trail_head][0] = m.x;
            m.trail[m.trail_head][1] = m.y;
            m.trail[m.trail_head][2] = m.z;
            m.trail_head = (m.trail_head + 1) % 6;
            if (m.trail_n < 6) {
                ++m.trail_n;
            }
        }

        m.x += m.vx / 30;
        m.y += m.vy / 30;
        m.z += m.vz / 30;
        if (m.y < 2 * kQ8) {
            m.active = false;
            spawn_explosion(m.x, 2 * kQ8, m.z);
            continue;
        }

        // 命中判定（無誘導弾も近接爆発する）
        const uint64_t rr = static_cast<uint64_t>(16 * kQ8) * (16 * kQ8);
        for (auto& t : g_en) {
            if (!t.alive) {
                continue;
            }
            const int64_t dx = t.x - m.x;
            const int64_t dy = t.y - m.y;
            const int64_t dz = t.z - m.z;
            const uint64_t d2 = static_cast<uint64_t>(dx * dx + dy * dy + dz * dz);
            if (d2 < rr) {
                m.active = false;
                t.alive = false;
                ++g_kills;
                spawn_explosion(t.x, t.y, t.z);
                const int reward = enemy_stats(t.type).score;
                g_score += reward;
                char msg[24];
                std::snprintf(msg, sizeof(msg), "ENEMY DOWN +%d", reward);
                set_msg(msg);
                break;
            }
        }
    }
}

void update_explosions() {
    for (auto& e : g_ex) {
        if (e.active && ++e.age > 26) {
            e.active = false;
        }
    }
}

void update_play() {
    ++g_play_tick;


    update_player_motion();
    if (g_death_reason != DeathReason::None) {
        finalize_gameover();
        return;
    }

    for (int i = 0; i < kMaxEnemies; ++i) {
        update_enemy(i);
        if (g_death_reason != DeathReason::None) {
            finalize_gameover();
            return;
        }
    }

    // 武器判定とHUDが同じフレームの姿勢・敵位置を使うように、敵の移動後に
    // カメラ変換と投影キャッシュを更新する。
    update_camera_transform();
    refresh_enemy_projection();
    update_player_weapons();
    if (g_death_reason != DeathReason::None) {
        finalize_gameover();
        return;
    }

    update_missiles();
    update_explosions();

    if (g_banner_timer > 0) {
        --g_banner_timer;
    }
    if (g_msg_timer > 0) {
        --g_msg_timer;
    }

    if (g_pl.hp <= 0 || g_death_reason != DeathReason::None) {
        finalize_gameover();
        return;
    }

    bool any_alive = false;
    for (const auto& e : g_en) {
        if (e.alive) {
            any_alive = true;
            break;
        }
    }
    if (!any_alive && g_banner_timer == 0) {
        g_score += 200;  // ウェーブクリアボーナス
        start_wave(g_wave + 1);
        // 新しいウェーブの敵も生成直後のフレームから同じ投影キャッシュを使う。
        update_camera_transform();
        refresh_enemy_projection();
    }
}

int abs_int(int value) {
    return value < 0 ? -value : value;
}

void clear_demo_controls() {
    // Title/Demo中に押されたキーで自動操縦が変わらないようにする。
    // Enter/Escは状態遷移を先に処理するため、ここでは触れない。
    const uint8_t controls[] = {
        keys::Left, keys::Right, keys::Up, keys::Down, keys::Space,
        static_cast<uint8_t>('M'), static_cast<uint8_t>('O'),
        static_cast<uint8_t>('L'),
    };
    for (const uint8_t key : controls) {
        g_in.down[key] = false;
        g_in.pressed[key] = false;
    }
}

int demo_target() {
    if (enemy_reference_alive(g_pl.lock_target, g_pl.lock_generation)) {
        return g_pl.lock_target;
    }
    if (enemy_reference_alive(g_pl.guide_target, g_pl.guide_generation)) {
        return g_pl.guide_target;
    }

    int best = -1;
    uint64_t best_d2 = 0;
    for (int i = 0; i < kMaxEnemies; ++i) {
        const Enemy& e = g_en[i];
        if (!e.alive) {
            continue;
        }
        const int64_t dx = e.x - g_pl.x;
        const int64_t dy = e.y - g_pl.y;
        const int64_t dz = e.z - g_pl.z;
        const uint64_t d2 = static_cast<uint64_t>(
            dx * dx + dy * dy + dz * dz);
        if (best < 0 || d2 < best_d2) {
            best = i;
            best_d2 = d2;
        }
    }
    if (best >= 0) {
        g_pl.guide_target = best;
        g_pl.guide_generation = g_en[best].generation;
    }
    return best;
}

void update_demo_autopilot() {
    clear_demo_controls();
    update_camera_transform();
    refresh_enemy_projection();
    update_guidance_target();

    const int target_idx = demo_target();
    if (target_idx < 0) {
        // ウェーブ切替の瞬間だけ目標がいない場合は、実機プレイと同じく
        // ゆっくり右へ索敵する。
        if ((g_demo_timer / 90u) & 1u) {
            g_in.down[keys::Left] = true;
        } else {
            g_in.down[keys::Right] = true;
        }
        return;
    }

    const Enemy& target = g_en[target_idx];
    const Proj p = project(target.x, target.y, target.z);
    const int horizontal_m = static_cast<int>(p.xc / kQ8);
    const int vertical_m = static_cast<int>(p.yc / kQ8);

    // 実プレイと同じロール→ヨー操作で目標を照準へ寄せる。背後の目標も
    // 同じ投影値から旋回方向を決めるため、固定された見せ物の軌道にはしない。
    if (target.vis && target.sx > kCx + 8) {
        g_in.down[keys::Right] = true;
    } else if (target.vis && target.sx < kCx - 8) {
        g_in.down[keys::Left] = true;
    } else if (!target.vis && horizontal_m > 55) {
        g_in.down[keys::Right] = true;
    } else if (!target.vis && horizontal_m < -55) {
        g_in.down[keys::Left] = true;
    } else if (p.zc <= 0) {
        // 真後ろは毎回同じ方向へ旋回して、画面内へ戻す。
        g_in.down[((target_idx + static_cast<int>(g_demo_timer / 90u)) & 1)
                      ? keys::Left : keys::Right] = true;
    }

    // 敵との高度差だけを追い、地面へ向かう入力は避ける。実プレイの
    // pitch制限・移動・地面衝突はそのまま適用される。
    if (target.vis && target.sy < kReticleY - 8 &&
        g_pl.y < 1800 * kQ8) {
        g_in.down[keys::Up] = true;
    } else if (target.vis && target.sy > kReticleY + 8 &&
               g_pl.y > 320 * kQ8) {
        g_in.down[keys::Down] = true;
    } else if (!target.vis && vertical_m > 80 && g_pl.y < 1800 * kQ8) {
        g_in.down[keys::Up] = true;
    } else if (!target.vis && vertical_m < -80 && g_pl.y > 320 * kQ8) {
        g_in.down[keys::Down] = true;
    }

    const bool in_gun_cone = target.vis && target.zc > 40 * kQ8 &&
                             target.zc < 700 * kQ8 &&
                             abs_int(target.sx - kCx) < 13 &&
                             abs_int(target.sy - kReticleY) < 13;
    g_in.down[keys::Space] = in_gun_cone;

    if (g_demo_attack_cooldown > 0) {
        --g_demo_attack_cooldown;
    }
    const bool cadence_attack = g_demo_timer >= 30 &&
                                ((g_demo_timer - 30u) % 120u) == 0u;
    if (g_pl.missiles > 0 && g_demo_attack_cooldown == 0 &&
        (g_pl.lock_timer >= 20 || cadence_attack)) {
        g_in.pressed['M'] = true;
        g_demo_attack_cooldown = 90;
    }

    // 長い追跡では加速し、近距離では自然に90m/sへ戻す。これもOキーを
    // 実際に押した時と同じ速度・エンジン制御を通る。
    if (p.zc > 950 * kQ8) {
        g_in.down['O'] = true;
    } else if (p.zc > 0 && p.zc < 260 * kQ8 &&
               (g_demo_timer % 180u) < 24u) {
        g_in.down['L'] = true;
    }
}

void update_demo() {
    // デモは専用の敵移動・撃墜演出を持たない。自動操縦が実際の入力状態を
    // 作り、通常プレイと同じ update_play()（敵AI、衝突、武器、Wave、得点）を
    // そのまま一フレーム進める。描画もrender_play()を共有する。
    update_demo_autopilot();
    update_play();
}

// ---------------------------------------------------------------- 背景描画

uint16_t sky_color(int d) {
    if (d < 8) {
        return kColSky3;
    }
    if (d < 24) {
        return kColSky2;
    }
    if (d < 52) {
        return kColSky1;
    }
    return kColSky0;
}

void render_background() {
    update_camera_transform();

    // 前進距離（地面ストライプのスクロール用）
    const uint8_t yaw = static_cast<uint8_t>(g_pl.yaw_q8 >> 8);
    const int32_t fwd_m = static_cast<int32_t>(
        ((static_cast<int64_t>(g_pl.x) * fm::sin_q12(yaw) +
          static_cast<int64_t>(g_pl.z) * fm::cos_q12(yaw)) >> 12) >> 8);
    int alt_m = static_cast<int>(g_pl.y >> 8);
    if (alt_m < 2) {
        alt_m = 2;
    }

    for (int y = 0; y < gfx::kHeight; ++y) {
        // 地平線 h(x) = hy + (x-80)*slope/256 と行 y の交点
        int sky_x0 = 0;
        int sky_x1 = 0;
        int gnd_x0 = 0;
        int gnd_x1 = 0;
        if (g_slope_q8 == 0) {
            if (y < g_hy) {
                sky_x0 = 0;
                sky_x1 = gfx::kWidth;
            } else {
                gnd_x0 = 0;
                gnd_x1 = gfx::kWidth;
            }
        } else {
            int xc = kCx + static_cast<int>(
                (static_cast<int64_t>(y - g_hy) << 8) / g_slope_q8);
            if (xc < 0) {
                xc = 0;
            }
            if (xc > gfx::kWidth) {
                xc = gfx::kWidth;
            }
            if (g_slope_q8 > 0) {
                // 右側ほど地平線が下（大きい y）→ 右側が空
                gnd_x0 = 0;
                gnd_x1 = xc;
                sky_x0 = xc;
                sky_x1 = gfx::kWidth;
            } else {
                sky_x0 = 0;
                sky_x1 = xc;
                gnd_x0 = xc;
                gnd_x1 = gfx::kWidth;
            }
        }

        if (g_slope_q8 == 0) {
            // ロール無し: 色帯の境界も水平のままで良い（従来通り行1本で塗れる）
            if (sky_x1 > sky_x0) {
                const int d = g_hy - y;
                gfx::hspan(y, sky_x0, sky_x1, d < 2 ? kColHaze : sky_color(d));
            }
            if (gnd_x1 > gnd_x0) {
                int v = y - g_hy;
                if (v < 1) {
                    v = 1;
                }
                uint16_t color;
                const int z_m = alt_m * kFocal / v;
                if (v < 3) {
                    color = kColHaze;
                } else if (z_m > 2600) {
                    color = kColGndFar;
                } else {
                    color = (((z_m + fwd_m) >> 6) & 1) ? kColGndA : kColGndB;
                }
                gfx::hspan(y, gnd_x0, gnd_x1, color);
            }
        } else {
            // ロールあり: 色帯の境界（sky_color の段差・地面遠近ストライプ）を
            // 傾いた地平線に合わせて回転させる。行 y・列 x ごとに、
            // 「ロールが無かったとした場合の地平線からの垂直距離」を
            // apply_roll の逆回転で求め、それを距離として使う
            // （水平な段差のまま斜めの地平線と重なる「くさび形」を防ぐ）。
            const int32_t py = y - g_hy;
            for (int x = sky_x0; x < sky_x1; ++x) {
                const int32_t px = x - kCx;
                // dy_world < 0 が地平線より上（空）。d は水平線からの
                // 正の距離にして従来の sky_color()/haze 判定と揃える。
                const int32_t dy_world = (-px * g_sinr + py * g_cosr) >> 12;
                const int d = static_cast<int>(-dy_world);
                gfx::put_pixel(x, y, d < 2 ? kColHaze : sky_color(d));
            }
            for (int x = gnd_x0; x < gnd_x1; ++x) {
                const int32_t px = x - kCx;
                int v = static_cast<int>((-px * g_sinr + py * g_cosr) >> 12);
                if (v < 1) {
                    v = 1;
                }
                uint16_t color;
                const int z_m = alt_m * kFocal / v;
                if (v < 3) {
                    color = kColHaze;
                } else if (z_m > 2600) {
                    color = kColGndFar;
                } else {
                    color = (((z_m + fwd_m) >> 6) & 1) ? kColGndA : kColGndB;
                }
                gfx::put_pixel(x, y, color);
            }
        }
    }

    // 太陽（方位固定 40 brad）
    const int8_t sun_d = static_cast<int8_t>(40 - yaw);
    if (sun_d > -45 && sun_d < 45) {
        const int sx0 = kCx + sun_d * 3;
        const int sy0 = g_hy - 46;
        int sx;
        int sy;
        apply_roll(sx0, sy0, &sx, &sy);
        if (sy > -8 && sy < gfx::kHeight + 8) {
            gfx::fill_circle(sx, sy, 6, gfx::rgb(255, 244, 190));
            gfx::fill_circle(sx, sy, 4, kColWhite);
        }
    }
}

// ---------------------------------------------------------------- エンティティ描画

struct DrawItem {
    int32_t zc;
    uint8_t kind;  // 0=enemy 1=missile 2=explosion
    uint8_t idx;
    int sx, sy;
};

void render_entities() {
    DrawItem items[kMaxEnemies + kMaxMissiles + kMaxExplosions];
    int n = 0;

    for (int i = 0; i < kMaxEnemies; ++i) {
        Enemy& e = g_en[i];
        if (!e.alive) {
            e.vis = false;
            continue;
        }
        if (!e.vis) {
            continue;
        }
        items[n++] = {e.zc, 0, static_cast<uint8_t>(i), e.sx, e.sy};
    }
    for (int i = 0; i < kMaxMissiles; ++i) {
        const Missile& m = g_ms[i];
        if (!m.active) {
            continue;
        }
        const Proj p = project(m.x, m.y, m.z);
        if (!p.ok) {
            continue;
        }
        items[n++] = {p.zc, 1, static_cast<uint8_t>(i), p.sx, p.sy};
    }
    for (int i = 0; i < kMaxExplosions; ++i) {
        const Explosion& e = g_ex[i];
        if (!e.active) {
            continue;
        }
        const Proj p = project(e.x, e.y, e.z);
        if (!p.ok) {
            continue;
        }
        items[n++] = {p.zc, 2, static_cast<uint8_t>(i), p.sx, p.sy};
    }

    // 遠い順に挿入ソート
    for (int i = 1; i < n; ++i) {
        const DrawItem key = items[i];
        int j = i - 1;
        while (j >= 0 && items[j].zc < key.zc) {
            items[j + 1] = items[j];
            --j;
        }
        items[j + 1] = key;
    }

    for (int i = 0; i < n; ++i) {
        const DrawItem& it = items[i];
        switch (it.kind) {
            case 0: {
                const Enemy& e = g_en[it.idx];
                if (e.pixw < 3) {
                    gfx::put_pixel(it.sx, it.sy, gfx::rgb(40, 40, 48));
                    gfx::put_pixel(it.sx + 1, it.sy, gfx::rgb(40, 40, 48));
                } else {
                    const int scale_q8 = e.pixw * 256 / 13;
                    gfx::art(kEnemyArt, kEnemyRows, it.sx, it.sy,
                             scale_q8 > 640 ? 640 : scale_q8,
                             enemy_palette_fn(e.type));
                }
                break;
            }
            case 1: {
                const Missile& m = g_ms[it.idx];
                // トレイル
                for (int t = 0; t < m.trail_n; ++t) {
                    const Proj tp = project(m.trail[t][0], m.trail[t][1],
                                            m.trail[t][2]);
                    if (tp.ok) {
                        gfx::put_pixel(tp.sx, tp.sy, gfx::rgb(200, 200, 200));
                    }
                }
                gfx::fill_rect(it.sx - 1, it.sy - 1, 2, 2, kColYellow);
                break;
            }
            case 2: {
                const Explosion& e = g_ex[it.idx];
                int r = 2 + e.age;
                const int32_t scale = static_cast<int32_t>(
                    (static_cast<int64_t>(40 * kFocal) * kQ8) / it.zc);
                r = r * (scale > 256 ? 256 : scale) / 256 + 2;
                if (r > 30) {
                    r = 30;
                }
                if (e.age < 8) {
                    gfx::fill_circle(it.sx, it.sy, r, kColWhite);
                    gfx::fill_circle(it.sx, it.sy, (r * 3) / 4, kColYellow);
                } else if (e.age < 18) {
                    gfx::fill_circle(it.sx, it.sy, r, kColOrange);
                    gfx::fill_circle(it.sx, it.sy, r / 2, kColYellow);
                } else {
                    gfx::fill_circle(it.sx, it.sy, r / 2,
                                     gfx::rgb(110, 100, 96));
                }
                break;
            }
        }
    }
}

void render_player_plane() {
    if (g_crashed) {
        return;
    }
    const int px = kCx - g_pl.roll / 4;
    const int py = 124 + static_cast<int>((g_frame >> 3) & 1);
    const char* const* art = kPlaneLevel;
    if (g_pl.roll < -12) {
        art = kPlaneBankLeft;
    } else if (g_pl.roll > 12) {
        art = g_bank_right;
    }
    gfx::art(art, kPlaneRows, px, py, 256, plane_palette);
    // エンジン光
    if (g_frame & 1) {
        gfx::fill_rect(px - 1, py + 7, 3, 2, kColOrange);
    }
}

int clamp_screen_edge(int value) {
    if (value < 8) {
        return 8;
    }
    if (value > gfx::kWidth - 9) {
        return gfx::kWidth - 9;
    }
    return value;
}

bool projection_screen_point(const Proj& p, int* sx, int* sy) {
    if (p.zc <= 0) {
        return false;
    }
    const int32_t depth = p.zc < 3 * kQ8 ? 3 * kQ8 : p.zc;
    const int sx0 = kCx + static_cast<int>(
        (static_cast<int64_t>(p.xc) * kFocal) / depth);
    const int sy0 = kCy - static_cast<int>(
        (static_cast<int64_t>(p.yc) * kFocal) / depth);
    apply_roll(sx0, sy0, sx, sy);
    return true;
}

bool edge_arrow_for_projection(const Proj& p, int* dir_x, int* dir_y,
                               int* arrow_x, int* arrow_y) {
    if (p.zc <= 0) {
        // 背後の目標は、旋回方向を迷わせないため右旋回を固定で案内する。
        *dir_x = 1;
        *dir_y = p.yc >= 0 ? 1 : -1;
    } else {
        int sx;
        int sy;
        if (!projection_screen_point(p, &sx, &sy)) {
            return false;
        }
        if (sx >= 0 && sx < gfx::kWidth && sy >= 0 && sy < gfx::kHeight) {
            return false;
        }
        *dir_x = sx - kCx;
        *dir_y = sy - kReticleY;
    }

    if (*dir_x == 0 && *dir_y == 0) {
        *dir_y = -1;
    }
    const int adx = *dir_x >= 0 ? *dir_x : -*dir_x;
    const int ady = *dir_y >= 0 ? *dir_y : -*dir_y;
    int ax;
    int ay;
    if (adx >= ady && adx > 0) {
        ax = *dir_x >= 0 ? gfx::kWidth - 9 : 8;
        ay = kReticleY + *dir_y * (ax > kCx ? ax - kCx : kCx - ax) / adx;
    } else {
        ay = *dir_y >= 0 ? gfx::kHeight - 9 : 8;
        ax = kCx + *dir_x * (ay > kReticleY ? ay - kReticleY
                                             : kReticleY - ay) / ady;
    }
    *arrow_x = clamp_screen_edge(ax);
    *arrow_y = clamp_screen_edge(ay);
    return true;
}

void draw_guidance_arrow(int x, int y, int dx, int dy, uint16_t color) {
    const int adx = dx >= 0 ? dx : -dx;
    const int ady = dy >= 0 ? dy : -dy;
    if (adx >= ady) {
        if (dx >= 0) {
            gfx::line(x - 5, y - 4, x, y, color);
            gfx::line(x - 5, y + 4, x, y, color);
        } else {
            gfx::line(x + 5, y - 4, x, y, color);
            gfx::line(x + 5, y + 4, x, y, color);
        }
        if (ady > 16) {
            const int marker_y = clamp_screen_edge(y + (dy < 0 ? -8 : 8));
            if (dy < 0) {
                gfx::line(x - 3, marker_y + 3, x, marker_y, color);
                gfx::line(x, marker_y, x + 3, marker_y + 3, color);
            } else {
                gfx::line(x - 3, marker_y - 3, x, marker_y, color);
                gfx::line(x, marker_y, x + 3, marker_y - 3, color);
            }
        }
        return;
    }

    if (dy >= 0) {
        gfx::line(x - 4, y - 5, x, y, color);
        gfx::line(x + 4, y - 5, x, y, color);
    } else {
        gfx::line(x - 4, y + 5, x, y, color);
        gfx::line(x + 4, y + 5, x, y, color);
    }
    if (adx > 16) {
        const int marker_x = clamp_screen_edge(x + (dx < 0 ? -8 : 8));
        if (dx < 0) {
            gfx::line(marker_x + 3, y - 3, marker_x, y, color);
            gfx::line(marker_x, y, marker_x + 3, y + 3, color);
        } else {
            gfx::line(marker_x - 3, y - 3, marker_x, y, color);
            gfx::line(marker_x, y, marker_x - 3, y + 3, color);
        }
    }
}

void render_guidance_arrow() {
    if (!enemy_reference_alive(g_pl.guide_target, g_pl.guide_generation)) {
        return;
    }
    const Enemy& target = g_en[g_pl.guide_target];
    const Proj p = project(target.x, target.y, target.z);
    int dir_x;
    int dir_y;
    int ax;
    int ay;
    if (edge_arrow_for_projection(p, &dir_x, &dir_y, &ax, &ay)) {
        draw_guidance_arrow(ax, ay, dir_x, dir_y, kColYellow);
    }
}

void render_attack_warning() {
    int warning_target = -1;
    uint64_t nearest_d2 = 0;
    for (int i = 0; i < kMaxEnemies; ++i) {
        const Enemy& e = g_en[i];
        if (!e.alive || e.warning_timer <= 0) {
            continue;
        }
        const int64_t dx = e.x - g_pl.x;
        const int64_t dy = e.y - g_pl.y;
        const int64_t dz = e.z - g_pl.z;
        const uint64_t d2 = static_cast<uint64_t>(dx * dx + dy * dy + dz * dz);
        if (warning_target < 0 || d2 < nearest_d2) {
            warning_target = i;
            nearest_d2 = d2;
        }
    }
    if (warning_target < 0) {
        return;
    }

    const Enemy& attacker = g_en[warning_target];
    gfx::text(kCx - gfx::text_width("INCOMING") / 2, 28, "INCOMING", kColRed);
    constexpr int kWarningBarW = 30;
    const int bar_x = kCx - kWarningBarW / 2;
    gfx::rect_outline(bar_x, 37, kWarningBarW + 2, 4, kColRed);
    const int fill_w = attacker.warning_timer * kWarningBarW /
                       kAttackWarningFrames;
    if (fill_w > 0) {
        gfx::fill_rect(bar_x + 1, 38, fill_w, 2, kColRed);
    }

    const Proj p = project(attacker.x, attacker.y, attacker.z);
    int sx;
    int sy;
    if (projection_screen_point(p, &sx, &sy) &&
        sx >= 0 && sx < gfx::kWidth && sy >= 0 && sy < gfx::kHeight) {
        int half = attacker.pixw / 2 + 4;
        if (half < 6) {
            half = 6;
        }
        if (half > 18) {
            half = 18;
        }
        if (g_frame & 2) {
            gfx::rect_outline(sx - half, sy - half, half * 2 + 1,
                              half * 2 + 1, kColRed);
        }
    } else {
        int dir_x;
        int dir_y;
        int ax;
        int ay;
        if (edge_arrow_for_projection(p, &dir_x, &dir_y, &ax, &ay)) {
            draw_guidance_arrow(ax, ay, dir_x, dir_y, kColRed);
        }
    }
}

// ---------------------------------------------------------------- HUD

void render_hud() {
    char buf[24];

    // 照準
    gfx::circle_outline(kCx, kReticleY, 7, kColHudGreen);
    gfx::put_pixel(kCx, kReticleY, kColHudGreen);
    gfx::line(kCx - 12, kReticleY, kCx - 8, kReticleY, kColHudGreen);
    gfx::line(kCx + 8, kReticleY, kCx + 12, kReticleY, kColHudGreen);
    gfx::line(kCx, kReticleY - 12, kCx, kReticleY - 8, kColHudGreen);
    gfx::line(kCx, kReticleY + 8, kCx, kReticleY + 12, kColHudGreen);

    // 機銃トレーサ
    if (g_pl.gun_flash > 0) {
        const int jx = static_cast<int>(fm::rnd_fx() % 5) - 2;
        gfx::line(64, 132, kCx + jx, kReticleY + 2, kColYellow);
        gfx::line(96, 132, kCx + jx, kReticleY + 2, kColYellow);
    }
    if (g_pl.hit_flash > 0) {
        const int d = 14;
        const int len = 4;
        gfx::line(kCx - d, kReticleY - d, kCx - d + len, kReticleY - d,
                  kColYellow);
        gfx::line(kCx - d, kReticleY - d, kCx - d, kReticleY - d + len,
                  kColYellow);
        gfx::line(kCx + d, kReticleY - d, kCx + d - len, kReticleY - d,
                  kColYellow);
        gfx::line(kCx + d, kReticleY - d, kCx + d, kReticleY - d + len,
                  kColYellow);
        gfx::line(kCx - d, kReticleY + d, kCx - d + len, kReticleY + d,
                  kColYellow);
        gfx::line(kCx - d, kReticleY + d, kCx - d, kReticleY + d - len,
                  kColYellow);
        gfx::line(kCx + d, kReticleY + d, kCx + d - len, kReticleY + d,
                  kColYellow);
        gfx::line(kCx + d, kReticleY + d, kCx + d, kReticleY + d - len,
                  kColYellow);
    }

    // ロックオン枠
    if (g_pl.lock_target >= 0 && g_pl.lock_target < kMaxEnemies) {
        const Enemy& e = g_en[g_pl.lock_target];
        if (e.alive && e.vis && e.generation == g_pl.lock_generation) {
            const bool locked = g_pl.lock_timer >= 20;
            const uint16_t c = locked ? kColRed : kColHudGreen;
            int half = e.pixw / 2 + 5;
            if (half < 7) {
                half = 7;
            }
            if (locked || (g_frame & 2)) {
                const int x0 = e.sx - half;
                const int y0 = e.sy - half;
                const int s = half / 2 + 2;
                // 四隅コーナー
                gfx::line(x0, y0, x0 + s, y0, c);
                gfx::line(x0, y0, x0, y0 + s, c);
                gfx::line(e.sx + half, y0, e.sx + half - s, y0, c);
                gfx::line(e.sx + half, y0, e.sx + half, y0 + s, c);
                gfx::line(x0, e.sy + half, x0 + s, e.sy + half, c);
                gfx::line(x0, e.sy + half, x0, e.sy + half - s, c);
                gfx::line(e.sx + half, e.sy + half, e.sx + half - s, e.sy + half, c);
                gfx::line(e.sx + half, e.sy + half, e.sx + half, e.sy + half - s, c);
            }
            if (locked) {
                gfx::text(e.sx - 11, e.sy - half - 9, "LOCK", kColRed);
            }
            // 距離表示
            std::snprintf(buf, sizeof(buf), "%ld", static_cast<long>(e.zc >> 8));
            gfx::text(e.sx + half + 2, e.sy - 3, buf, c);

            // ロック成立までの20フレームを照準下のゲージで示す。
            constexpr int kLockFrames = 20;
            constexpr int kGaugeW = 30;
            const int gauge_x = kCx - kGaugeW / 2;
            const int gauge_y = kReticleY + 16;
            gfx::rect_outline(gauge_x, gauge_y, kGaugeW + 2, 4, kColHudDim);
            int progress = g_pl.lock_timer;
            if (progress > kLockFrames) {
                progress = kLockFrames;
            }
            if (progress > 0) {
                const int fill_w = progress * kGaugeW / kLockFrames;
                gfx::fill_rect(gauge_x + 1, gauge_y + 1, fill_w, 2, c);
            }
        }
    }

    render_attack_warning();
    render_guidance_arrow();

    // レーダー（左下）
    const int rx = 21;
    const int ry = 137;
    const int rr = 16;
    gfx::fill_circle(rx, ry, rr, gfx::rgb(6, 26, 10));
    gfx::circle_outline(rx, ry, rr, kColHudDim);
    gfx::put_pixel(rx, ry, kColHudGreen);
    gfx::put_pixel(rx, ry - 1, kColHudGreen);
    {
        const uint8_t yaw = static_cast<uint8_t>(g_pl.yaw_q8 >> 8);
        const int32_t s = fm::sin_q12(yaw);
        const int32_t c = fm::cos_q12(yaw);
        for (const auto& e : g_en) {
            if (!e.alive) {
                continue;
            }
            const int64_t dx = e.x - g_pl.x;
            const int64_t dz = e.z - g_pl.z;
            int32_t xr = static_cast<int32_t>((dx * c - dz * s) >> 12) >> 8;
            int32_t zr = static_cast<int32_t>((dx * s + dz * c) >> 12) >> 8;
            // 100m = 1px。各軸ではなくベクトル全体を円の半径に収める。
            xr /= 100;
            zr /= 100;
            const int radar_limit = rr - 3;
            const int64_t radar_d2 =
                static_cast<int64_t>(xr) * xr +
                static_cast<int64_t>(zr) * zr;
            const int64_t radar_limit2 =
                static_cast<int64_t>(radar_limit) * radar_limit;
            if (radar_d2 > radar_limit2) {
                const uint32_t radar_dist =
                    fm::isqrt64(static_cast<uint64_t>(radar_d2));
                if (radar_dist > 0) {
                    xr = static_cast<int32_t>(
                        static_cast<int64_t>(xr) * radar_limit / radar_dist);
                    zr = static_cast<int32_t>(
                        static_cast<int64_t>(zr) * radar_limit / radar_dist);
                }
            }
            gfx::put_pixel(rx + xr, ry - zr, enemy_radar_color(e.type));
        }
    }

    // テキスト類
    std::snprintf(buf, sizeof(buf), "SCORE %06d", g_score);
    gfx::text(3, 3, buf, kColHudGreen);
    std::snprintf(buf, sizeof(buf), "WAVE %d", g_wave);
    gfx::text(160 - 3 - gfx::text_width(buf), 3, buf, kColHudGreen);

    const int spd_kmh = static_cast<int>(
        (static_cast<int64_t>(g_pl.speed_q8) * 36) / (10 * kQ8));
    std::snprintf(buf, sizeof(buf), "SPD %3d", spd_kmh);
    gfx::text(42, 146, buf, kColHudGreen);
    std::snprintf(buf, sizeof(buf), "ALT %4ld", static_cast<long>(g_pl.y >> 8));
    gfx::text(42, 153, buf, kColHudGreen);

    std::snprintf(buf, sizeof(buf), "MSL %2d", g_pl.missiles);
    gfx::text(118, 146, buf, kColYellow);
    // HP バー
    gfx::text(96, 153, "HP", kColHudGreen);
    gfx::rect_outline(110, 153, 44, 6, kColHudDim);
    const int hpw = g_pl.hp * 42 / 100;
    const uint16_t hpc = g_pl.hp > 50 ? kColHudGreen
                        : (g_pl.hp > 25 ? kColYellow : kColRed);
    if (hpw > 0) {
        gfx::fill_rect(111, 154, hpw, 4, hpc);
    }

    // 警告
    if (g_pl.y < 60 * kQ8 && (g_frame & 4)) {
        gfx::text(kCx - gfx::text_width("PULL UP") / 2, 96, "PULL UP", kColRed);
    }
    if (g_pl.dmg_flash > 0) {
        gfx::rect_outline(0, 0, 160, 160, kColRed);
        gfx::rect_outline(1, 1, 158, 158, kColRed);
    }
    if (g_msg_timer > 0) {
        gfx::text(kCx - gfx::text_width(g_msg) / 2, 104, g_msg, kColYellow);
    }
    if (g_banner_timer > 0 && (g_banner_timer & 4)) {
        char wbuf[16];
        std::snprintf(wbuf, sizeof(wbuf), "WAVE %d", g_wave);
        gfx::text(kCx - gfx::text_width(wbuf, 2) / 2, 44, wbuf, kColWhite, 2);
    }
}

// ---------------------------------------------------------------- 画面全体

void dim_framebuffer() {
    uint16_t* fb = gfx::fb();
    for (int i = 0; i < gfx::kWidth * gfx::kHeight; ++i) {
        fb[i] = (fb[i] >> 1) & 0x7bef;
    }
}

void render_play() {
    render_background();
    render_entities();
    render_player_plane();
    render_hud();
}

void render_title() {
    // 固定アングルの背景
    g_pl.pitch_q8 = 0;
    g_pl.roll = 0;
    g_pl.y = 600 * kQ8;
    g_pl.yaw_q8 = static_cast<uint16_t>((g_frame * 12) & 0xffff);
    render_background();

    // 飛び回る敵機シルエット
    const int ex = static_cast<int>((g_frame * 2) % 220) - 30;
    const int ey = 52 + static_cast<int>((g_frame / 5) % 9);
    gfx::art(kEnemyArt, kEnemyRows, ex, ey, 300, enemy_palette_fighter);

    gfx::art(kPlaneLevel, kPlaneRows, kCx, 118, 400, plane_palette);

    gfx::text(kCx - gfx::text_width("SKY ACE", 3) / 2, 20, "SKY ACE",
              kColWhite, 3);
    gfx::text(kCx - gfx::text_width("PICOCALC AIR COMBAT") / 2, 46,
              "PICOCALC AIR COMBAT", kColHudGreen);

    gfx::text(24, 64, "ARROWS : STEER", kColWhite);
    gfx::text(24, 72, "SPACE  : GUN", kColWhite);
    gfx::text(24, 80, "M      : MISSILE", kColWhite);
    gfx::text(24, 88, "O / L  : THROTTLE", kColWhite);

    char version[24];
    std::snprintf(version, sizeof(version), "VERSION %s",
                  PICO_SKYACE_VERSION_STRING);
    gfx::text(kCx - gfx::text_width(version) / 2, 98, version, kColHudGreen);

    if (g_frame & 8) {
        gfx::text(kCx - gfx::text_width("PRESS ENTER", 2) / 2, 140,
                  "PRESS ENTER", kColYellow, 2);
    }
}

void render_demo() {
    // 実プレイと同じ背景・敵・自機・照準・レーダー・警告・戦績を描画する。
    // デモであることは小さなラベルだけで示し、専用の固定演出は置かない。
    render_play();
    gfx::text(3, 18, "DEMO", kColYellow);
    gfx::text(3, 25, "AUTOPILOT", kColOrange);
}

void render_pause() {
    render_play();
    dim_framebuffer();
    gfx::text(kCx - gfx::text_width("PAUSED", 2) / 2, 54, "PAUSED",
              kColYellow, 2);
    gfx::text(kCx - gfx::text_width("P/ENTER: RESUME") / 2, 84,
              "P/ENTER: RESUME", kColWhite);
    gfx::text(kCx - gfx::text_width("ESC: TITLE") / 2, 94,
              "ESC: TITLE", kColWhite);
}

const char* death_reason_label(DeathReason reason) {
    switch (reason) {
        case DeathReason::Crash: return "CRASH";
        case DeathReason::Collision: return "COLLISION";
        case DeathReason::ShotDown: return "SHOT DOWN";
        case DeathReason::None:
        default: return "UNKNOWN";
    }
}

void render_gameover() {
    render_background();
    refresh_enemy_projection();
    render_entities();
    render_hud();
    dim_framebuffer();

    char buf[24];
    gfx::text(kCx - gfx::text_width("GAME OVER", 2) / 2, 56, "GAME OVER",
              kColRed, 2);
    std::snprintf(buf, sizeof(buf), "SCORE %06d", g_result.score);
    gfx::text(kCx - gfx::text_width(buf) / 2, 80, buf, kColWhite);
    std::snprintf(buf, sizeof(buf), "WAVE %d", g_result.wave);
    gfx::text(kCx - gfx::text_width(buf) / 2, 90, buf, kColWhite);
    std::snprintf(buf, sizeof(buf), "KILLS %d", g_result.kills);
    gfx::text(kCx - gfx::text_width(buf) / 2, 100, buf, kColWhite);
    const uint32_t total_seconds = g_result.play_ticks / 30;
    std::snprintf(buf, sizeof(buf), "TIME %02lu:%02lu",
                  static_cast<unsigned long>(total_seconds / 60),
                  static_cast<unsigned long>(total_seconds % 60));
    gfx::text(kCx - gfx::text_width(buf) / 2, 110, buf, kColWhite);
    std::snprintf(buf, sizeof(buf), "CAUSE %s",
                  death_reason_label(g_result.death_reason));
    gfx::text(kCx - gfx::text_width(buf) / 2, 120, buf, kColRed);
    if (g_frame & 8) {
        gfx::text(kCx - gfx::text_width("PRESS ENTER") / 2, 136,
                  "PRESS ENTER", kColYellow);
    }
}

}  // namespace

void run() {
    fm::init();
    init_mirrored_art();
    g_pl.y = 600 * kQ8;
    play_title_music();
    g_title_deadline_us = time_us_64() + kTitleDemoDelayUs;
    g_demo_deadline_us = 0;
    g_gameover_deadline_us = 0;

    absolute_time_t next_frame = make_timeout_time_ms(kFrameMs);
    while (true) {
        // フリーズ検知用ウォッチドッグ。
        watchdog_update();

        g_in.begin_frame();
        g_in.poll();

        // F5: 現在のフレームバッファを SD カードの /screenshots/ に BMP で
        // 保存する。SD書き込みの間はゲームループが止まるが、ユーザー要望通り
        // 許容する（同期処理でそれ以外を複雑にしない）。
        if (g_in.pressed[keys::F5]) {
            screenshot::capture(gfx::fb(), gfx::kWidth, gfx::kHeight);
            next_frame = make_timeout_time_ms(kFrameMs);
        }

        switch (g_mode) {
            case Mode::Title:
                if (g_in.pressed[keys::Enter]) {
                    enter_play_from_reset("Title->Play");
                    render_play();
                } else if (g_title_deadline_us != 0 &&
                           time_us_64() >= g_title_deadline_us) {
                    enter_demo();
                    update_demo();
                    render_demo();
                } else {
                    render_title();
                    // LCD転送が締切をまたいでも、次のフレームを丸ごと待たずに
                    // このフレームの完了時点でデモへ切り替える。
                    if (g_title_deadline_us != 0 &&
                        time_us_64() >= g_title_deadline_us) {
                        enter_demo();
                        update_demo();
                        render_demo();
                    }
                }
                break;
            case Mode::Play:
                if (g_in.pressed['P'] || g_in.pressed[keys::Escape]) {
                    enter_pause();
                    render_pause();
                    break;
                }
                update_play();
                if (g_mode == Mode::Play) {
                    render_play();
                } else {
                    render_gameover();
                }
                break;
            case Mode::Pause:
                if (g_in.pressed[keys::Escape]) {
                    enter_title("Pause->Title(esc)");
                    render_title();
                } else if (g_in.pressed['P'] || g_in.pressed[keys::Enter]) {
                    resume_play();
                    render_play();
                } else {
                    render_pause();
                }
                break;
            case Mode::Demo:
                if (g_in.pressed[keys::Enter]) {
                    enter_play_from_reset("Demo->Play");
                    render_play();
                } else if (g_in.pressed[keys::Escape]) {
                    enter_title("Demo->Title(esc)");
                    render_title();
                } else if (g_demo_deadline_us != 0 &&
                           time_us_64() >= g_demo_deadline_us) {
                    enter_title("Demo->Title(timeout)");
                    render_title();
                } else {
                    ++g_demo_timer;
                    update_demo();
                    render_demo();
                    // 実フレームの描画／転送中に締切を越えた場合も、次の
                    // フレームまで余計に待たず、直ちにタイトルへ戻す。
                    if (g_demo_deadline_us != 0 &&
                        time_us_64() >= g_demo_deadline_us) {
                        enter_title("Demo->Title(timeout)");
                        render_title();
                    }
                }
                break;
            case Mode::GameOver:
                update_explosions();
                if (g_in.pressed[keys::Enter]) {
                    enter_play_from_reset("GameOver->Play");
                    render_play();
                } else if (g_in.pressed[keys::Escape]) {
                    enter_title("GameOver->Title(esc)");
                    render_title();
                } else if (g_gameover_deadline_us != 0 &&
                           time_us_64() >= g_gameover_deadline_us) {
                    enter_title("GameOver->Title(timeout)");
                    render_title();
                } else {
                    render_gameover();
                    if (g_gameover_deadline_us != 0 &&
                        time_us_64() >= g_gameover_deadline_us) {
                        enter_title("GameOver->Title(timeout)");
                        render_title();
                    }
                }
                break;
        }

        display::present_scaled2x(gfx::fb());
        ++g_frame;

        sleep_until(next_frame);
        next_frame = delayed_by_ms(next_frame, kFrameMs);
        if (absolute_time_diff_us(next_frame, get_absolute_time()) > 200000) {
            // 大きく遅れたらリセット（デッドライン方式）
            next_frame = make_timeout_time_ms(kFrameMs);
        }
    }
}

}  // namespace skyace::game
