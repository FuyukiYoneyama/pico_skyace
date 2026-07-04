// game/pico_rescue の fatfs_diskio.cpp を skyace::sdcard へ橋渡しするよう
// 名前空間だけ変えて移植したもの。
#include "ff.h"
#include "diskio.h"
#include "platform/sd/picocalc_sdcard.h"

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    return skyace::sdcard::init()
               ? 0
               : static_cast<DSTATUS>(STA_NOINIT |
                                      (skyace::sdcard::is_present() ? 0 : STA_NODISK));
}

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    if (!skyace::sdcard::is_present()) {
        return STA_NODISK | STA_NOINIT;
    }
    return skyace::sdcard::is_initialized() ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || buff == nullptr || count == 0) {
        return RES_PARERR;
    }
    return skyace::sdcard::read_sectors(static_cast<uint32_t>(sector), buff, count)
               ? RES_OK
               : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || buff == nullptr || count == 0) {
        return RES_PARERR;
    }
    return skyace::sdcard::write_sectors(static_cast<uint32_t>(sector), buff, count)
               ? RES_OK
               : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    uint32_t sector_count = 0;

    if (pdrv != 0) {
        return RES_PARERR;
    }

    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;
        case GET_SECTOR_COUNT:
            if (buff == nullptr || !skyace::sdcard::get_sector_count(&sector_count)) {
                return RES_ERROR;
            }
            *static_cast<DWORD*>(buff) = sector_count;
            return RES_OK;
        case GET_SECTOR_SIZE:
            if (buff == nullptr) {
                return RES_PARERR;
            }
            *static_cast<WORD*>(buff) = 512;
            return RES_OK;
        case GET_BLOCK_SIZE:
            if (buff == nullptr) {
                return RES_PARERR;
            }
            *static_cast<DWORD*>(buff) = 1;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}
