#include "game/game.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "game/fixed_math.h"
#include "game/gfx.h"
#include "pico/stdlib.h"
#include "platform/picocalc_display.h"
#include "platform/picocalc_key_table.h"
#include "platform/picocalc_keyboard.h"

namespace skyace::game {
namespace {

namespace fm = skyace::fixmath;

// ---------------------------------------------------------------- 定数

constexpr int kFrameMs = 33;         // 約 30fps
constexpr int kFocal = 110;          // 内部解像度での焦点距離（px）
constexpr int kCx = 80;              // 画面中心
constexpr int kCy = 80;
constexpr int kReticleY = 72;        // 照準の画面 y

constexpr int kMaxEnemies = 5;
constexpr int kMaxMissiles = 4;
constexpr int kMaxExplosions = 6;

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

struct Input {
    bool down[256] = {};
    bool pressed[256] = {};

    void begin_frame() {
        std::memset(pressed, 0, sizeof(pressed));
    }

    void poll() {
        keyboard::KeyEvent ev;
        while (keyboard::read_event(&ev)) {
            const uint8_t key = keys::uppercase_ascii(ev.key);
            switch (ev.state) {
                case keyboard::KeyState::Pressed:
                    down[key] = true;
                    pressed[key] = true;
                    break;
                case keyboard::KeyState::Hold:
                    down[key] = true;
                    break;
                case keyboard::KeyState::Released:
                    down[key] = false;
                    break;
                default:
                    break;
            }
        }
    }
};

// ---------------------------------------------------------------- 状態

enum class Mode : uint8_t { Title, Play, GameOver };

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
    int lock_target;      // -1 = 無し
    int lock_timer;
    int dmg_flash;
};

enum class EnemyMode : uint8_t { Cruise, Turn, Attack, Evade };

struct Enemy {
    bool alive;
    int32_t x, y, z;
    uint16_t yaw_q8;
    int32_t speed_q8;
    int hp;
    EnemyMode mode;
    int8_t turn_dir;
    int16_t timer;
    int16_t fire_cd;
    // 直近フレームの投影キャッシュ（HUD・照準判定用）
    bool vis;
    int sx, sy;
    int32_t zc;
    int pixw;
};

struct Missile {
    bool active;
    int32_t x, y, z;
    int32_t vx, vy, vz;   // Q8 m/s
    int target;
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
Player g_pl;
Enemy g_en[kMaxEnemies];
Missile g_ms[kMaxMissiles];
Explosion g_ex[kMaxExplosions];

uint32_t g_frame = 0;
int g_wave = 0;
int g_score = 0;
int g_banner_timer = 0;
int g_msg_timer = 0;
char g_msg[24] = "";
bool g_crashed = false;

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

uint16_t enemy_palette(char c) {
    switch (c) {
        case 'X': return gfx::rgb(70, 66, 78);
        case 'R': return gfx::rgb(255, 60, 60);
        default: return gfx::rgb(70, 66, 78);
    }
}

// ---------------------------------------------------------------- 投影

struct Proj {
    bool ok;
    int sx, sy;      // ロール適用済みスクリーン座標
    int32_t zc;      // カメラ空間の奥行き（Q8 m）
};

void apply_roll(int sx0, int sy0, int* sx, int* sy) {
    const int32_t dx = sx0 - kCx;
    const int32_t dy = sy0 - g_hy;
    *sx = kCx + static_cast<int>((dx * g_cosr - dy * g_sinr) >> 12);
    *sy = g_hy + static_cast<int>((dx * g_sinr + dy * g_cosr) >> 12);
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

    if (zc < 3 * kQ8) {
        p.ok = false;
        return p;
    }
    const int sx0 = kCx + static_cast<int>(
        (static_cast<int64_t>(xc) * kFocal) / zc);
    const int sy0 = kCy - static_cast<int>(
        (static_cast<int64_t>(yc) * kFocal) / zc);
    apply_roll(sx0, sy0, &p.sx, &p.sy);
    p.zc = zc;
    p.ok = (p.sx > -80 && p.sx < 240 && p.sy > -80 && p.sy < 240);
    return p;
}

// ---------------------------------------------------------------- スポーン

void spawn_explosion(int32_t x, int32_t y, int32_t z) {
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
        e.speed_q8 = fm::rnd_range(60, 90) * kQ8;
        e.hp = 100;
        e.mode = EnemyMode::Cruise;
        e.turn_dir = (fm::rnd() & 1) ? 1 : -1;
        e.timer = static_cast<int16_t>(fm::rnd_range(40, 120));
        e.fire_cd = 30;
        e.vis = false;
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
    g_pl = Player{};
    g_pl.x = 0;
    g_pl.y = 800 * kQ8;
    g_pl.z = 0;
    g_pl.yaw_q8 = 0;
    g_pl.speed_q8 = 90 * kQ8;
    g_pl.hp = 100;
    g_pl.missiles = 8;
    g_pl.lock_target = -1;
    g_score = 0;
    g_crashed = false;
    g_msg_timer = 0;
    start_wave(1);
}

void set_msg(const char* s) {
    std::snprintf(g_msg, sizeof(g_msg), "%s", s);
    g_msg_timer = 50;
}

// ---------------------------------------------------------------- 更新

void fire_missile() {
    if (g_pl.missiles <= 0) {
        return;
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
        const bool locked = g_pl.lock_target >= 0 && g_pl.lock_timer >= 20 &&
                            g_en[g_pl.lock_target].alive;
        m.target = locked ? g_pl.lock_target : -1;
        m.life = 150;  // 5 秒
        m.trail_n = 0;
        m.trail_head = 0;
        --g_pl.missiles;
        return;
    }
}

void update_player() {
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
    if (g_in.down['X']) {
        g_pl.speed_q8 += 80;
    } else if (g_in.down['C']) {
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
        g_crashed = true;
        const int32_t fx = fm::sin_q12(yaw);
        const int32_t fz = fm::cos_q12(yaw);
        spawn_explosion(g_pl.x + ((40 * kQ8 * fx) >> 12),
                        6 * kQ8,
                        g_pl.z + ((40 * kQ8 * fz) >> 12));
        return;
    }

    // 機銃
    if (g_pl.gun_cd > 0) {
        --g_pl.gun_cd;
    }
    if (g_pl.gun_flash > 0) {
        --g_pl.gun_flash;
    }
    if (g_in.down['Z'] && g_pl.gun_cd == 0) {
        g_pl.gun_cd = 3;
        g_pl.gun_flash = 3;
        for (int i = 0; i < kMaxEnemies; ++i) {
            Enemy& e = g_en[i];
            if (!e.alive || !e.vis) {
                continue;
            }
            if (e.zc < 700 * kQ8 &&
                e.sx > kCx - 10 && e.sx < kCx + 10 &&
                e.sy > kReticleY - 10 && e.sy < kReticleY + 10) {
                e.hp -= 9;
                if (e.hp <= 0) {
                    e.alive = false;
                    spawn_explosion(e.x, e.y, e.z);
                    g_score += 100;
                    set_msg("ENEMY DOWN +100");
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
        if (g_pl.lock_target == best) {
            if (g_pl.lock_timer < 60) {
                ++g_pl.lock_timer;
            }
        } else {
            g_pl.lock_target = best;
            g_pl.lock_timer = 0;
        }
    } else {
        g_pl.lock_target = -1;
        g_pl.lock_timer = 0;
    }

    // ミサイル発射
    if (g_in.pressed[keys::Space]) {
        fire_missile();
    }

    if (g_pl.dmg_flash > 0) {
        --g_pl.dmg_flash;
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
    for (const auto& m : g_ms) {
        if (m.active && m.target == enemy_idx) {
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

    const int64_t dx = g_pl.x - e.x;
    const int64_t dz = g_pl.z - e.z;
    const uint32_t dist_m = fm::isqrt64(
        static_cast<uint64_t>(dx * dx + dz * dz)) >> 8;

    // ミサイルに狙われたら回避
    if (e.mode != EnemyMode::Evade && missile_targets(idx)) {
        e.mode = EnemyMode::Evade;
        e.turn_dir = (fm::rnd() & 1) ? 1 : -1;
        e.timer = 70;
    }

    if (--e.timer <= 0) {
        // モード遷移
        const uint32_t r = fm::rnd() % 100;
        if (dist_m > 2500) {
            e.mode = EnemyMode::Attack;  // 離れすぎ → 追跡して戻る
            e.timer = 90;
        } else if (r < 35 && dist_m < 900) {
            e.mode = EnemyMode::Attack;
            e.timer = static_cast<int16_t>(fm::rnd_range(60, 120));
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
            steer_enemy_towards(e, bearing, 190);
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

    // 攻撃（プレイヤーが正面コーンに入っていれば射撃）
    if (e.fire_cd > 0) {
        --e.fire_cd;
    }
    if (e.mode == EnemyMode::Attack && dist_m < 550 && dist_m > 40) {
        const uint8_t bearing = fm::atan2_brad(
            static_cast<int32_t>(dx >> 8), static_cast<int32_t>(dz >> 8));
        const int8_t diff = static_cast<int8_t>(bearing - yaw);
        if (diff > -6 && diff < 6 && e.fire_cd == 0) {
            e.fire_cd = 26;
            if ((fm::rnd() % 10) < 6) {  // 命中率 60%
                g_pl.hp -= 7;
                g_pl.dmg_flash = 8;
                set_msg("TAKING FIRE!");
                if (g_pl.hp <= 0) {
                    g_pl.hp = 0;
                }
            }
        }
    }

    // 体当たり判定
    const int64_t dy = g_pl.y - e.y;
    const uint64_t d2 = static_cast<uint64_t>(dx * dx + dy * dy + dz * dz);
    const uint64_t rr = static_cast<uint64_t>(25 * kQ8) * (25 * kQ8);
    if (d2 < rr) {
        e.alive = false;
        spawn_explosion(e.x, e.y, e.z);
        g_pl.hp -= 30;
        g_pl.dmg_flash = 12;
        g_score += 50;
        set_msg("MIDAIR COLLISION!");
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

        // 誘導
        if (m.target >= 0 && g_en[m.target].alive) {
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
                spawn_explosion(t.x, t.y, t.z);
                g_score += 100;
                set_msg("ENEMY DOWN +100");
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
    update_player();
    for (int i = 0; i < kMaxEnemies; ++i) {
        update_enemy(i);
    }
    update_missiles();
    update_explosions();

    if (g_banner_timer > 0) {
        --g_banner_timer;
    }
    if (g_msg_timer > 0) {
        --g_msg_timer;
    }

    if (g_pl.hp <= 0) {
        g_mode = Mode::GameOver;
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
    }
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
        const Proj p = project(e.x, e.y, e.z);
        e.vis = p.ok;
        if (!p.ok) {
            continue;
        }
        e.sx = p.sx;
        e.sy = p.sy;
        e.zc = p.zc;
        e.pixw = static_cast<int>(
            (static_cast<int64_t>(14 * kFocal) * kQ8) / p.zc);
        items[n++] = {p.zc, 0, static_cast<uint8_t>(i), p.sx, p.sy};
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
                             scale_q8 > 640 ? 640 : scale_q8, enemy_palette);
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
        const int jx = static_cast<int>(fm::rnd() % 5) - 2;
        gfx::line(64, 132, kCx + jx, kReticleY + 2, kColYellow);
        gfx::line(96, 132, kCx + jx, kReticleY + 2, kColYellow);
    }

    // ロックオン枠
    if (g_pl.lock_target >= 0) {
        const Enemy& e = g_en[g_pl.lock_target];
        if (e.alive && e.vis) {
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
        }
    }

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
            // 100m = 1px、レーダー円に収める
            xr /= 100;
            zr /= 100;
            if (xr < -(rr - 2)) {
                xr = -(rr - 2);
            }
            if (xr > rr - 2) {
                xr = rr - 2;
            }
            if (zr < -(rr - 2)) {
                zr = -(rr - 2);
            }
            if (zr > rr - 2) {
                zr = rr - 2;
            }
            gfx::put_pixel(rx + xr, ry - zr, kColRed);
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
    gfx::art(kEnemyArt, kEnemyRows, ex, ey, 300, enemy_palette);

    gfx::art(kPlaneLevel, kPlaneRows, kCx, 118, 400, plane_palette);

    gfx::text(kCx - gfx::text_width("SKY ACE", 3) / 2, 20, "SKY ACE",
              kColWhite, 3);
    gfx::text(kCx - gfx::text_width("PICOCALC AIR COMBAT") / 2, 46,
              "PICOCALC AIR COMBAT", kColHudGreen);

    gfx::text(24, 64, "ARROWS : STEER", kColWhite);
    gfx::text(24, 72, "SPACE  : MISSILE", kColWhite);
    gfx::text(24, 80, "Z      : GUN", kColWhite);
    gfx::text(24, 88, "X / C  : THROTTLE", kColWhite);

    if (g_frame & 8) {
        gfx::text(kCx - gfx::text_width("PRESS ENTER", 2) / 2, 140,
                  "PRESS ENTER", kColYellow, 2);
    }
}

void render_gameover() {
    render_background();
    render_entities();
    render_hud();
    dim_framebuffer();

    char buf[24];
    gfx::text(kCx - gfx::text_width("GAME OVER", 2) / 2, 56, "GAME OVER",
              kColRed, 2);
    std::snprintf(buf, sizeof(buf), "SCORE %06d", g_score);
    gfx::text(kCx - gfx::text_width(buf) / 2, 80, buf, kColWhite);
    std::snprintf(buf, sizeof(buf), "WAVE %d", g_wave);
    gfx::text(kCx - gfx::text_width(buf) / 2, 90, buf, kColWhite);
    if (g_frame & 8) {
        gfx::text(kCx - gfx::text_width("PRESS ENTER") / 2, 116,
                  "PRESS ENTER", kColYellow);
    }
}

}  // namespace

void run() {
    fm::init();
    init_mirrored_art();
    g_pl.y = 600 * kQ8;

    absolute_time_t next_frame = make_timeout_time_ms(kFrameMs);
    while (true) {
        g_in.begin_frame();
        g_in.poll();

        switch (g_mode) {
            case Mode::Title:
                fm::rnd();  // タイトルで回して種を進める
                if (g_in.pressed[keys::Enter]) {
                    reset_game();
                    g_mode = Mode::Play;
                }
                render_title();
                break;
            case Mode::Play:
                if (g_in.pressed[keys::Escape]) {
                    g_mode = Mode::Title;
                    break;
                }
                update_play();
                if (g_mode == Mode::Play) {
                    render_play();
                } else {
                    render_gameover();
                }
                break;
            case Mode::GameOver:
                update_missiles();
                update_explosions();
                for (int i = 0; i < kMaxEnemies; ++i) {
                    update_enemy(i);
                }
                if (g_in.pressed[keys::Enter] || g_in.pressed[keys::Escape]) {
                    g_mode = Mode::Title;
                }
                render_gameover();
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
