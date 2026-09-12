# v0.9.17 検証ビルド記録

実施日: 2026-09-12

この版は、v1.0.0公開前の作業版である。起動UARTの版識別を修正し、
`PICO_SKYACE_BOOT`の1行ヘッダーをUART初期化直後とLCD初期化境界で出力する。
`1.0.0`への更新は、この版の確認後に行う最終工程として残す。

## 変更点

- `PICO_SKYACE_BOOT app=pico_skyace version=... build=...`を追加した。
- ヘッダーには`WATCHDOG_CAUSED_REBOOT`、クロック、UART速度を含め、出力後に
  `stdio_flush()`を呼ぶ。
- LCD初期化境界でもアプリ名・バージョン・共通`BUILD_ID`を再出力する。
- 翻訳単位ごとに異なる`__TIME__`は版識別に使わず、CMakeが一度生成したUTCの
  `PICO_SKYACE_BUILD_ID`を全ソースで共有する。
- 既存の人間向け起動ログと検証シナリオ用の行は互換性のため維持した。

## ビルド

環境:

- Pico SDK: 2.2.0 (`/home/fuyuki/pico/pico-sdk`)
- ターゲット: RP2040 / `pico`、Release
- sysclk: 250 MHz
- 診断コード: OFF（製品設定）

実行:

```sh
cmake --build build -j2
arm-none-eabi-size build/pico_skyace.elf
picotool info -a build/pico_skyace.uf2
```

結果:

- ビルド: 77/77ターゲット、警告なし
- `arm-none-eabi-size`: `text=101900 data=0 bss=59764 dec=161664`
- UF2: [`build/pico_skyace.uf2`](../../../build/pico_skyace.uf2)
- UF2 SHA-256: `a9fdc3e7607fc4300cd8e3a13110eb111c016a185a9237387ff7651af6f43f44`
- ELF SHA-256: `e9ef6ff756ed1218774f2bf8c16eb6f33605be040768c0fe1636ed4f811e3894`
- raw BIN SHA-256: `1cadb13edee1d33b4c6829da29429504eefc6fe7e31200fefdedd9e532531224`
- 共通`BUILD_ID`: `2026-09-12T07:16:56Z`
- UF2 family: `rp2040`、build attributes: `Release`

## ホストテスト

既存の`build/host-tests`で`cmake --build`とCTestを実行し、1/1テスト合格。

## PicoCalcエミュレーター

製品BINを公式LCD・キーボード・FAT32 SDモデルへ接続し、
[`title_version_smoke.json`](../../../tests/emulator/title_version_smoke.json)を
`--quantum 1`で実行した。

結果: 全5ステップ`pass`、`scenario_done`、例外なし、unsupported MMIOなし、
キーボードdrop 0、LCDの`pixels_dropped=0`、`orphan_data_bytes=0`。

UART先頭:

```text
PICO_SKYACE_BOOT app=pico_skyace version=0.9.17 build="2026-09-12T07:16:56Z" WATCHDOG_CAUSED_REBOOT=0 clock_ok=1 sysclk_target_khz=250000 sysclk_actual_khz=250000 uart_baud=115200 phase=stdio
```

LCD初期化境界にも同じ版番号・`BUILD_ID`が出力されることを確認した。

- レポート: `build-artifacts/2026-09-12-v0.9.17/emulator-title/report.json`
- UARTログ: `build-artifacts/2026-09-12-v0.9.17/emulator-title/uart.log`
- 画面キャプチャ: `build-artifacts/2026-09-12-v0.9.17/emulator-title/snapshots/title-version.png`
- 保存証跡のSHA-256マニフェスト: [`MANIFEST.sha256`](MANIFEST.sha256)

## 実機受入との対応

今回の実機ログ`/home/fuyuki/pico_dvl/codex/log/20260912_154959.log`は、
`0.9.17`修正版を書き込む前のログであるため、動作確認の参考記録として扱う。
最終UF2の実機ログでは、先頭の`PICO_SKYACE_BOOT`行と表示境界の再掲行を確認してから
`1.0.0`へ進める。

## 公開工程

この版ではタグやGitHub Releaseを作成しない。`1.0.0`の最終ビルド、タグ、Releaseは
リポジトリをPublicへ変更する工程と分離して実施する。
