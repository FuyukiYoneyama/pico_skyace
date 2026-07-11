# Sky Ace（pico_skyace）

PicoCalc（RP2040 標準構成）向けの、初代エースコンバット風・後方追従視点の
疑似 3D 空戦ゲーム。

- 内部 160×160 RGB565 レンダリング → PIO SPI で 2 倍拡大し 320×320 へ4象限で転送
- 固定小数点（位置 Q8 / 三角関数 Q12、角度は brad = 256 で一周）
- 敵機は 3D 座標を持ち、距離に応じたスプライト拡縮で描画
- 地平線はピッチで上下・ロールで傾斜、地面は遠近ストライプでスクロール
- ロックオン → 誘導ミサイル、照準内ヒットスキャンの機銃
- ウェーブ制（全滅で次ウェーブ、敵数が増える）。ウェーブが進むと敵タイプが
  増える: Fighter（標準）、Bomber（低速・高耐久・回避しない大型機）、
  Interceptor（高速・低耐久・高命中率）
- 8kHz IRQ ミキシングによるノイズ入り効果音・エンジン音・5チャンネルの
  チップチューン風BGM（リード/アルペジオ/ブラス/ベース + 合成ドラム。
  チャンネルごとにデューティ比・エンベロープを変え、リードにはビブラート、
  ドラムはキック/スネア/ハット/クラッシュ/タムをノイズと矩形波で合成）
- F5 キーで SD カードに BMP スクリーンショットを保存
- 約 30fps（sysclk 250 MHz、LCD 62.5 MHz SPI 相当）

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
| F5 | スクリーンショットを SD カードの `/screenshots/` に保存 |
| Enter | タイトル / ゲームオーバーから開始 |
| Esc | プレイ中にタイトルへ戻る |

照準（画面中央の円）に敵を約 0.7 秒とらえ続けると LOCK（赤枠）になり、
その状態で撃ったミサイルが誘導される。高度 0 に落ちると墜落。

## 必要なハードウェア

- [PicoCalc](https://www.clockworkpi.com/picocalc)（コアボードは Raspberry Pi Pico
  (RP2040) を使用。Pico 2 / RP2350 では未検証）
- 320×320 ST7365P LCD、I2C キーボード（PicoCalc 標準構成）
- スクリーンショット機能を使う場合は SD カード（FAT32 フォーマット）

## ビルド

[pico-sdk](https://github.com/raspberrypi/pico-sdk) と ARM GCC ツールチェーン、
CMake、Ninja が必要。

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S . -B build -G Ninja
cmake --build build
# 成果物: build/pico_skyace.uf2
```

BOOTSEL を押しながら USB 接続し、`pico_skyace.uf2` をドラッグ&ドロップで書き込む。

## LCD ドライバについて（重要）

`src/platform/lcd_rgb565_pio.cpp`・`.h`・`lcd_spi_min.pio` は、著者の他の
RP2040/PicoCalc プロジェクトで実機検証済みの実装をそのまま流用したもので、
**この3ファイルは変更しないこと。** 独自に似たロジックを再実装すると、
CS（チップセレクト）を長時間下げっぱなしにする連続バーストでパネルの同期が
崩れるという、座標計算とは無関係な低レベルの電気的問題を再現してしまう。

背景・原因調査・教訓の詳細は [docs/LCD_BRINGUP_HISTORY.md](docs/LCD_BRINGUP_HISTORY.md)
を参照。画面が崩れる場合は、まずこの3ファイル自体が無改変かを疑うこと。
描画の呼び出し方を変えたい場合は `picocalc_display.cpp`（アダプタ層）を変更する。

## ログ

UART0（USB-C の CH340 経由）、115200 bps 8N1。起動時に version / build id /
sysclk / backlight / 直前がウォッチドッグ復帰だったか(`WATCHDOG_CAUSED_REBOOT`)
を出力する。プレイ中は `WAVE START` / `SPAWN`（敵タイプ含む）/ `MODE` 遷移も
出力するので、フリーズ/黒画面が起きた場合は UART ログの最後の行が手がかりに
なる。ゲームループは 3 秒ごとにウォッチドッグを蹴っており、フリーズすると
自動リセットして次回起動ログに `WATCHDOG_CAUSED_REBOOT=1` が出る。

## 構成

```
src/
├── main.cpp                    # クロック設定・初期化・起動
├── config/board_config.h       # ピン・クロック定数（keyboard/PSRAM関連）
├── platform/
│   ├── lcd_rgb565_pio.cpp/.h   # LCDドライバ本体（変更禁止、上記参照）
│   ├── lcd_spi_min.pio         # 同上。PIOプログラムも無改変コピー
│   ├── picocalc_display.cpp/.h # 上記への薄いアダプタ（skyace::display API）
│   ├── picocalc_keyboard.*     # I2C キーボードドライバ
│   ├── picocalc_audio.*        # 8kHz IRQ 音声ミキサー（SFX/エンジン音/BGM）
│   ├── screenshot_capture.*    # F5 スクリーンショット（フレームバッファ→BMP）
│   └── sd/                     # SD カード SPI ドライバ・FatFs ブリッジ
├── game/
│   ├── fixed_math.*             # sin/cos/tan/atan2/isqrt/乱数（固定小数点）
│   ├── gfx.*                    # 160x160 フレームバッファ描画プリミティブ
│   ├── font5x7.h                # 5x7 ビットマップフォント
│   ├── bgm_track.h              # BGM 用ノートデータ（MIDIから生成）
│   └── game.*                   # ゲームロジック・レンダリング・HUD
third_party/
└── ChanFatFS/                   # ChaN 氏の FatFs（別ライセンス、下記参照）
```

## バージョン

`VERSION` ファイルで管理（major.minor.patch）。ソース挙動が変わるビルドを
渡すときは必ず数値を上げること。

## ライセンス

このリポジトリの著者自身のコードは [MIT License](LICENSE) の下で公開している。

### サードパーティ

- `third_party/ChanFatFS/`: [ChaN 氏の FatFs](http://elm-chan.org/fsw/ff/00index_e.html)。
  BSD 系の独自ライセンス（[LICENSE.txt](third_party/ChanFatFS/LICENSE.txt) 参照）。
- BGM（`src/game/bgm_track.h`）は、ChatGPT（OpenAI）に生成させた、初代
  エースコンバット風の雰囲気を意図したオリジナル曲の MIDI からノートデータを
  機械的に抽出・生成したもの。既存ゲームの楽曲データの複製・採譜ではなく、
  特定の著作物からの抽出でもない。
- ビルドには [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
  （BSD-3-Clause）が必要（本リポジトリには含まれない）。
