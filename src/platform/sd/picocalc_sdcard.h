// SPI0 経由の生ブロック SD カードドライバ。著者の別プロジェクトの
// 実機動作確認済み実装を、名前空間だけ skyace 用に変えて移植したもの。
// プロトコル部分は無改変。
#pragma once

#include <stdbool.h>
#include <stdint.h>

namespace skyace::sdcard {

bool is_present();
bool init();
bool is_initialized();
bool read_sectors(uint32_t lba, uint8_t* buffer, uint32_t count);
bool write_sectors(uint32_t lba, const uint8_t* buffer, uint32_t count);
bool get_sector_count(uint32_t* sector_count);
void reset();

}  // namespace skyace::sdcard
