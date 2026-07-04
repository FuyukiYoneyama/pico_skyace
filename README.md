# Sky Ace（pico_skyace）

PicoCalc（RP2040 標準構成）向けの、初代エースコンバット風・後方追従視点の
疑似 3D 空戦ゲーム。

- 内部 160×160 RGB565 レンダリング → PIO SPI で 2 倍拡大し 320×320 へ4象限で転送
- 固定小数点（位置 Q8 / 三角関数 Q12、角度は brad = 256 で一周）
- 敵機は 3D 座標を持ち、距離に応じたスプライト拡縮で描画
- 地平線はピッチで上下・ロールで傾斜、地面は遠近ストライプでスクロール
- ロックオン → 誘導ミサイル、照準内ヒットスキャンの機銃
- ウェーブ制（全滅で次ウェーブ、敵数が増える）
- 約 30fps（sysclk 250 MHz、LCD 62.5 MHz SPI 相当）

**LCD ドライバは `general/lcd/src/lcd_rgb565_pio.cpp`（2026-07-04 実機動作確認済み）を
無改変でコピーしたもの**（`src/platform/lcd_rgb565_pio.cpp`）。独自実装との違いと
長期化した不具合の詳細は [docs/LCD_BRINGUP_HISTORY.md](docs/LCD_BRINGUP_HISTORY.md)
と `general/01_DISPLAY_LCD.md` §8.1 を参照。

## スクリーンショット

実機（PicoCalc）で F5 キャプチャ機能を使って撮影したもの。

| タイトル画面 | ゲームプレイ（ロックオン中） |
|---|---|
| ![タイトル画面](docs/images/title.jpg) | ![ゲームプレイ画面](docs/images/gameplay.jpg) |

## 操作

| キー | 動作 |
|------|------|
| ← / → | ロール（旋回） |
| ↑ / ↓ | 機首上げ / 下げ |
| Space | 機銃（押しっぱなし） |
| M | ミサイル発射（ロックオン中は誘導） |
| O / L | 加速 / 減速 |
| Enter | タイトル / ゲームオーバーから開始 |
| Esc | プレイ中にタイトルへ戻る |

照準（画面中央の円）に敵を約 0.7 秒とらえ続けると LOCK（赤枠）になり、
その状態で撃ったミサイルが誘導される。高度 0 に落ちると墜落。

## ビルド

```sh
export PICO_SDK_PATH=/home/fuyuki/pico/pico-sdk
cmake -S . -B build -G Ninja
cmake --build build
# 成果物: build/pico_skyace.uf2
```

BOOTSEL を押しながら USB 接続し、`pico_skyace.uf2` をドラッグ&ドロップで書き込む。

画面が崩れる場合は LCD 転送の問題。`src/platform/lcd_rgb565_pio.cpp` は
検証済みファイルの無改変コピーなので、まずこのファイル自体を変更していないか
確認すること（`general/lcd/src/lcd_rgb565_pio.cpp` と diff を取って一致させる）。
詳細は [docs/LCD_BRINGUP_HISTORY.md](docs/LCD_BRINGUP_HISTORY.md) を参照。

## ログ

UART0（USB-C の CH340 経由）、115200 bps 8N1。起動時に version / build id / 
sysclk / backlight を出力する。

## 構成

```
src/
├── main.cpp                    # クロック設定・初期化・起動
├── config/board_config.h       # ピン・クロック定数（keyboard/PSRAM関連）
├── platform/
│   ├── lcd_rgb565_pio.cpp/.h   # LCDドライバ本体。general/lcd から無改変コピー（変更禁止）
│   ├── lcd_spi_min.pio         # 同上。PIOプログラムも無改変コピー
│   ├── picocalc_display.cpp/.h # 上記への薄いアダプタ（skyace::display API）
│   └── picocalc_keyboard.*     # I2C キーボードドライバ（pico_rescue 由来）
└── game/
    ├── fixed_math.*             # sin/cos/tan/atan2/isqrt/乱数（固定小数点）
    ├── gfx.*                    # 160x160 フレームバッファ描画プリミティブ
    ├── font5x7.h                # 5x7 ビットマップフォント
    └── game.*                   # ゲームロジック・レンダリング・HUD
```

**`platform/lcd_rgb565_pio.cpp`・`.h`・`lcd_spi_min.pio` は変更しないこと。**
`general/lcd/src/` の同名ファイルと常に無改変で一致させる（`diff` で確認）。
描画の呼び出し方を変えたい場合は `picocalc_display.cpp`（アダプタ層）を変更する。

## バージョン

`VERSION` ファイルで管理（major.minor.patch）。ソース挙動が変わるビルドを
渡すときは必ず数値を上げること。
