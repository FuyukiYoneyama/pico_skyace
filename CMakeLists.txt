cmake_minimum_required(VERSION 3.18)

if(DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

if(NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is required, for example -DPICO_SDK_PATH=/path/to/pico-sdk")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_skyace C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/VERSION
)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION PICO_SKYACE_VERSION)
string(STRIP "${PICO_SKYACE_VERSION}" PICO_SKYACE_VERSION)

pico_sdk_init()

add_executable(pico_skyace
    src/main.cpp
    src/platform/picocalc_display.cpp
    src/platform/lcd_rgb565_pio.cpp
    src/platform/picocalc_keyboard.cpp
    src/platform/picocalc_audio.cpp
    src/platform/screenshot_capture.cpp
    src/platform/high_score.cpp
    src/platform/sd/picocalc_sdcard.cpp
    src/platform/sd/fatfs_diskio.cpp
    src/game/fixed_math.cpp
    src/game/game_rules.cpp
    src/game/gfx.cpp
    src/game/game.cpp
    third_party/ChanFatFS/src/ff.c
    third_party/ChanFatFS/src/ffunicode.c
)

pico_generate_pio_header(pico_skyace ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/lcd_spi_min.pio)

target_include_directories(pico_skyace PRIVATE
    src
    third_party/ChanFatFS/include
)

target_compile_options(pico_skyace PRIVATE -O3 -Wall -Wextra)

# third_party/ChanFatFS/src/ff.c の gen_numname() で -Wstringop-overflow が
# 誤検知する（既知のFatFs+GCCパターン）。呼び出し元 dir_register() は
# 衝突回数 n を for (n = 1; n < 100; n++) で必ず99以下に制限しており、
# gen_numname() 内のハッシュ計算はこの小さい n を種にする設計のため、
# 8バイトバッファ ns[8] からはみ出すことは実行時には起こり得ない。
# ベンダーコードは無改変で保つため、この1ファイルに限定して警告を抑制する。
set_source_files_properties(third_party/ChanFatFS/src/ff.c PROPERTIES
    COMPILE_OPTIONS "-Wno-stringop-overflow"
)
target_compile_definitions(pico_skyace PRIVATE
    PICO_SKYACE_VERSION_STRING="${PICO_SKYACE_VERSION}"
    PICO_DEFAULT_UART_BAUD_RATE=115200
    PICO_STACK_SIZE=0x1000
)

target_link_libraries(pico_skyace
    pico_stdlib
    hardware_clocks
    hardware_pio
    hardware_dma
    hardware_i2c
    hardware_pwm
    hardware_spi
    hardware_watchdog
)

pico_add_extra_outputs(pico_skyace)

pico_enable_stdio_usb(pico_skyace 0)
pico_enable_stdio_uart(pico_skyace 1)
