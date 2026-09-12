# v0.9.16 検証ビルド記録

実施日: 2026-09-12

この版は、v1.0.0公開前の作業版である。診断ログ、エミュレーター検証、
成果物の対応付けをこの版番号で行い、`1.0.0`への更新は作業完了後の
最終ビルド直前に一度だけ行う。

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
- `arm-none-eabi-size`: `text=101524 data=0 bss=59764 dec=161288`
- UF2: [`build/pico_skyace.uf2`](../../../build/pico_skyace.uf2)
- UF2 SHA-256: `98a8f2bea1d50052aa833477e7fd98f9b199d9b68261c2fc388f08a560734822`
- ELF SHA-256: `11ee4930b9ba0f12082ea232deffc8a91ec3c8daa76dd02d5aacb8206e9d3351`
- エミュレーター入力BIN SHA-256: `00ed444014a87e974c734e620e4e43028123b2b0c2c602d9d81315ed78137f7d`
- UF2 family: `rp2040`、build attributes: `Release`

## ホストテスト

`cmake -S tests -B <tmp> -G Ninja -DCMAKE_BUILD_TYPE=Debug`、
`ctest --test-dir <tmp> --output-on-failure`を実行し、1/1テスト合格。

## PicoCalcエミュレーター

製品BINを公式LCD・キーボード・FAT32 SDモデルへ接続し、
[`title_version_smoke.json`](../../../tests/emulator/title_version_smoke.json)を実行した。
エミュレーターのPIOサンプラが31.25 MHz相当の転送を取りこぼさないよう、
このシナリオでは`--quantum 1`（1命令単位）を明示している。これはファームウェアの
設定を変更するものではない。

```sh
picocalc-run --bin /tmp/pico-skyace-0.9.16.bin \
  --bootrom /home/fuyuki/pico_dvl/codex/picoem-picocalc/roms/rp2040/bootrom-rp2040-b2.bin \
  --board picocalc --lcd-variant pio-rgb565 --keyboard --sd --quantum 1 \
  --scenario tests/emulator/title_version_smoke.json
```

結果: 全5ステップ`pass`、`scenario_done`、例外なし、unsupported MMIOなし、
キーボードdrop 0。

- UART: `pico_skyace version 0.9.16`、`WATCHDOG_CAUSED_REBOOT=0`
- LCD: `display_on=true`、`colmod=0x65`、`pixels_written=934643`、
  `pixels_dropped=0`、`orphan_data_bytes=0`
- framebuffer: 320×320、非黒画素`102400`
- レポート: `build-artifacts/2026-09-12-v0.9.16/emulator-title/report.json`
- UARTログ: `build-artifacts/2026-09-12-v0.9.16/emulator-title/uart.log`
- 画面キャプチャ: `build-artifacts/2026-09-12-v0.9.16/emulator-title/snapshots/title-version.png`
- 保存証跡のSHA-256マニフェスト: [`MANIFEST.sha256`](MANIFEST.sha256)

## 実機受入との対応

ユーザーから、現行ビルドについてコールドブート、LCD、キーボード、同時入力、
BGM・効果音・エンジン音、SDあり／なし、電源再投入、最新の敵攻撃性、Wave 4デモ、
太陽色、20分上限負荷が問題ないとの報告を受けている。対象UF2の版番号・SHA-256・
実施時刻は提供されていないため、これは手動受入記録として扱い、公開時の最終UF2へ
対応付ける作業を残す。

## 公開工程

この版ではタグやGitHub Releaseを作成しない。`main`へのコミット・プッシュ後、
ソースを変更しない最終手順として`VERSION=1.0.0`へ更新し、再ビルド・SHA-256記録・
`v1.0.0`タグ作成を行う。GitHub ReleaseはリポジトリをPublicへ変更した後に作成する。
