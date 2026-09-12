# v0.9.18 検証ビルド記録

実施日: 2026-09-12

この版は、電源投入時のCH340/ホスト側COM再認識で起動UARTの先頭行が
取りこぼされる問題を修正した作業版である。`stdio_init_all()`後に500ms待って
から`PICO_SKYACE_BOOT`を出力する。`1.0.0`のタグ・リリースはまだ作成しない。

## 変更点

- `VERSION`を`0.9.18`へ更新した。
- UART初期化後に500msの整定待ちを追加し、電源再投入時にCH340/COM列挙が完了して
  から起動識別行を送るようにした。
- `PICO_SKYACE_BOOT`のflush、LCD初期化境界での識別再掲、共通UTC`BUILD_ID`は
  v0.9.17から維持した。

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
sha256sum build/pico_skyace.uf2 build/pico_skyace.elf build/pico_skyace.bin
```

結果:

- ビルド: 77/77ターゲット、警告なし
- `arm-none-eabi-size`: `text=101908 data=0 bss=59764 dec=161672`
- UF2: [`build/pico_skyace.uf2`](../../../build/pico_skyace.uf2)
- UF2 SHA-256: `981fb2dceb8acbf66fbb1a23b99b3f7b9b26c4c4bb4602337ddeafbf3f129827`
- UF2サイズ: `192000` bytes
- ELF SHA-256: `63d34b9229fdb1f41ac196b8f8814b9b12e1e05574b54274f467287015a9db96`
- raw BIN SHA-256: `0ecc8023c2264fb3d69ae4f25bf49fafaae3a99848bf935f017226d737ab93ea`
- 共通`BUILD_ID`: `2026-09-12T07:45:54Z`
- UF2 family: `rp2040`、build attributes: `Release`

## ホストテスト

既存の`build/host-tests`でビルドとCTestを実行し、1/1テスト合格。

## PicoCalcエミュレーター

製品BINを公式LCD・キーボード・FAT32 SDモデルへ接続し、
[`title_version_smoke.json`](../../../tests/emulator/title_version_smoke.json)を
`--quantum 1`で実行した。

結果: 全5ステップ`pass`、実行時間1690ms。`firmware initialized`は520ms時点で
合格し、500ms整定後の識別行を取得した。例外なし、unsupported MMIOなし、LCDの
`pixels_dropped=0`、`orphan_data_bytes=0`。

UART先頭:

```text
PICO_SKYACE_BOOT app=pico_skyace version=0.9.18 build="2026-09-12T07:45:54Z" WATCHDOG_CAUSED_REBOOT=0 clock_ok=1 sysclk_target_khz=250000 sysclk_actual_khz=250000 uart_baud=115200 phase=stdio
```

保存証跡:

- レポート: `build-artifacts/2026-09-12-v0.9.18/emulator-title/report.json`
- UARTログ: `build-artifacts/2026-09-12-v0.9.18/emulator-title/uart.log`
- 画面キャプチャ: `build-artifacts/2026-09-12-v0.9.18/emulator-title/snapshots/title-version.png`
- SHA-256マニフェスト: [`MANIFEST.sha256`](MANIFEST.sha256)

### 製品タイミング経路の追加試行

`title_demo_cycle_product.json`を既定の10億サイクル上限で起動した試行は、
仮想4014msで`cycle_limit`に達して未完了となった。この未完了のレポートとUARTは
[`title-demo-cycle-product-incomplete-cycle-limit/`](../../../build-artifacts/2026-09-12-v0.9.18/title-demo-cycle-product-incomplete-cycle-limit/)
へ移動して保全した。正式記録と同じ500億サイクル／量子65536での再試行は、
エミュレーターの処理時間が過大だったため成果物を作らず中断した。したがって、
この版の製品タイミング経路の合格根拠は、前版から継承した記録ではなく、今回の
タイトル起動スモーク（500ms整定後の識別行取得）に限定する。

## 実機受入との対応

`/home/fuyuki/pico_dvl/codex/log/20260912_162230.log`はSHA-256
`ab6b185e6afeef71a8bf1a930d8484995641cfd624015defb42f3d0cb58d0d7a`（92行）で保全されている。
このログはゲーム動作（Wave 4デモ、30秒遷移、GameOver帰還、SD/ハイスコア）を示すが、
先頭に`version`または`PICO_SKYACE_BOOT`がないため、0.9.18 UF2の実行証跡とは扱わない。
0.9.18 UF2で電源OFF/ONし、ログ先頭に`version=0.9.18`が出ることを別途確認する。

## 公開工程

この版ではタグやGitHub Releaseを作成しない。`1.0.0`の最終ビルド、タグ、Releaseは
リポジトリをPublicへ変更する工程と分離して実施する。
