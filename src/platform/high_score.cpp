#include "platform/high_score.h"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "ff.h"
#include "platform/sd/picocalc_sdcard.h"

namespace skyace::high_score {
namespace {

// 8.3形式のルートファイルを2つ使う。更新先が途中で壊れても、もう一方の
// 旧記録を残せるため、電源断でハイスコア全体を失わない。
constexpr const char* kSlotA = "0:/SKYHI_A.DAT";
constexpr const char* kSlotB = "0:/SKYHI_B.DAT";
constexpr uint8_t kMagic[4] = {'S', 'K', 'H', 'I'};
constexpr uint8_t kFormatVersion = 1;
constexpr size_t kRecordSize = 16;

FATFS g_fs{};
bool g_initialized = false;
int g_current_score = 0;

void put_u32(uint8_t* dst, uint32_t value) {
    dst[0] = static_cast<uint8_t>(value);
    dst[1] = static_cast<uint8_t>(value >> 8);
    dst[2] = static_cast<uint8_t>(value >> 16);
    dst[3] = static_cast<uint8_t>(value >> 24);
}

uint32_t get_u32(const uint8_t* src) {
    return static_cast<uint32_t>(src[0]) |
           (static_cast<uint32_t>(src[1]) << 8) |
           (static_cast<uint32_t>(src[2]) << 16) |
           (static_cast<uint32_t>(src[3]) << 24);
}

uint32_t checksum(const uint8_t* data, size_t size) {
    // 小さいレコードの誤読・途中書き込みを検出するFNV-1a。暗号用途では
    // なく、異常な記録を採用して現在のハイスコアを下げないために使う。
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

void encode(uint8_t record[kRecordSize], uint32_t score) {
    std::memset(record, 0, kRecordSize);
    std::memcpy(record, kMagic, sizeof(kMagic));
    record[4] = kFormatVersion;
    put_u32(&record[8], score);
    put_u32(&record[12], checksum(record, 12));
}

bool decode(const uint8_t record[kRecordSize], uint32_t* score) {
    if (std::memcmp(record, kMagic, sizeof(kMagic)) != 0 ||
        record[4] != kFormatVersion || get_u32(&record[12]) != checksum(record, 12)) {
        return false;
    }
    const uint32_t decoded = get_u32(&record[8]);
    if (decoded > static_cast<uint32_t>(INT_MAX)) {
        return false;
    }
    *score = decoded;
    return true;
}

bool mount_card() {
    if (!sdcard::is_present()) {
        std::printf("HIGHSCORE storage status=no_card\n");
        return false;
    }
    const FRESULT result = f_mount(&g_fs, "0:", 1);
    if (result != FR_OK) {
        std::printf("HIGHSCORE storage status=unavailable fr=%u\n",
                    static_cast<unsigned>(result));
        return false;
    }
    return true;
}

bool read_slot(const char* path, uint32_t* score) {
    FIL file{};
    const FRESULT open_result = f_open(&file, path, FA_READ);
    if (open_result != FR_OK) {
        return false;
    }

    uint8_t record[kRecordSize] = {};
    UINT read = 0;
    const FRESULT read_result = f_read(&file, record, sizeof(record), &read);
    const FRESULT close_result = f_close(&file);
    if (read_result != FR_OK || close_result != FR_OK || read != sizeof(record)) {
        return false;
    }
    return decode(record, score);
}

bool write_slot(const char* path, uint32_t score) {
    FIL file{};
    FRESULT result = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (result != FR_OK) {
        return false;
    }

    uint8_t record[kRecordSize] = {};
    encode(record, score);
    UINT written = 0;
    bool ok = f_write(&file, record, sizeof(record), &written) == FR_OK &&
              written == sizeof(record);
    if (ok) {
        ok = f_sync(&file) == FR_OK;
    }
    result = f_close(&file);
    return ok && result == FR_OK;
}

bool load_slots(uint32_t* score_a, bool* valid_a, uint32_t* score_b,
                bool* valid_b) {
    *valid_a = read_slot(kSlotA, score_a);
    *valid_b = read_slot(kSlotB, score_b);
    if (!*valid_a && !*valid_b) {
        return false;
    }
    uint32_t best = 0;
    if (*valid_a && *score_a > best) {
        best = *score_a;
    }
    if (*valid_b && *score_b > best) {
        best = *score_b;
    }
    g_current_score = static_cast<int>(best);
    return true;
}

}  // namespace

void init() {
    if (g_initialized) {
        return;
    }
    g_initialized = true;
    g_current_score = 0;

    if (!mount_card()) {
        std::printf("HIGHSCORE load status=session_only score=0\n");
        return;
    }

    uint32_t score_a = 0;
    uint32_t score_b = 0;
    bool valid_a = false;
    bool valid_b = false;
    if (load_slots(&score_a, &valid_a, &score_b, &valid_b)) {
        std::printf("HIGHSCORE load status=ok score=%d slots=%u/%u\n",
                    g_current_score, valid_a ? 1u : 0u, valid_b ? 1u : 0u);
    } else {
        std::printf("HIGHSCORE load status=empty score=0\n");
    }
}

int current() {
    return g_current_score;
}

bool submit(int score) {
    if (!g_initialized) {
        init();
    }
    if (score < 0) {
        score = 0;
    }
    if (score <= g_current_score) {
        return false;
    }

    g_current_score = score;
    const uint32_t candidate = static_cast<uint32_t>(score);

    if (!mount_card()) {
        std::printf("HIGHSCORE save status=session_only score=%d\n",
                    g_current_score);
        return true;
    }

    uint32_t score_a = 0;
    uint32_t score_b = 0;
    bool valid_a = false;
    bool valid_b = false;
    const bool has_persisted_record =
        load_slots(&score_a, &valid_a, &score_b, &valid_b);
    // 起動時にSDが抜かれていて後から挿入された場合、RAM上の0だけを見て
    // 新記録扱いにしない。既存の保存値を再読込した時点で、そちらを優先する。
    if (has_persisted_record && candidate <= static_cast<uint32_t>(g_current_score)) {
        return false;
    }
    g_current_score = score;

    // 未使用スロットを優先し、両方に記録がある場合は低い方を更新する。
    // したがって、書き込み途中の電源断でも高い方の記録は残る。
    const char* path = kSlotA;
    char slot = 'A';
    if (!valid_a) {
        path = kSlotA;
        slot = 'A';
    } else if (!valid_b) {
        path = kSlotB;
        slot = 'B';
    } else if (score_a <= score_b) {
        path = kSlotA;
        slot = 'A';
    } else {
        path = kSlotB;
        slot = 'B';
    }

    if (!write_slot(path, candidate)) {
        std::printf("HIGHSCORE save status=session_only score=%d slot=%c\n",
                    g_current_score, slot);
        return true;
    }

    uint32_t verify = 0;
    if (!read_slot(path, &verify) || verify != candidate) {
        std::printf("HIGHSCORE save status=verify_failed score=%d slot=%c\n",
                    g_current_score, slot);
        return true;
    }
    std::printf("HIGHSCORE save status=ok score=%d slot=%c\n",
                g_current_score, slot);
    return true;
}

}  // namespace skyace::high_score
