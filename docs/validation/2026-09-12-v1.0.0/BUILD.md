# v1.0.0 最終検証ビルド記録

実施日: 2026-09-12

この記録は、公開タグを作成する直前の `1.0.0` 最終ビルドを固定するための
ものです。`VERSION` の更新だけを行った版で、ゲーム本体の挙動は
`v0.9.18`から変更していません。タグとGitHub Releaseは、この記録のUF2を
実機で受け入れた後に作成します。

## ソースとビルド条件

- ソースリビジョン: `12b7420d5e6367a2851f4523b81e4d1d0216242e`
- Pico SDK: 2.2.0 (`/home/fuyuki/pico/pico-sdk`、revision
  `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`)
- ターゲット: RP2040 / `pico`、Release
- CMake: 3.28.3、Ninja: 1.11.1
- ARM GCC: 13.2.1 (`15:13.2.rel1-2`)
- sysclk: 250 MHz（READMEの仕様外クロック注記を参照）
- `PICO_SKYACE_PERF_DIAGNOSTICS`: OFF（製品設定）
- 共通 `BUILD_ID`: `2026-09-12T08:24:02Z`

## 再現コマンド

作業ツリーの `build/` を初期化してから、SDKパスを明示して実行した。

```sh
cmake --fresh -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPICO_SDK_PATH=/home/fuyuki/pico/pico-sdk
cmake --build build --clean-first -j2
cmake --build build/host-tests -j2
ctest --test-dir build/host-tests --output-on-failure
```

結果はファームウェア83/83ターゲット、警告なし。ホストテストは1/1合格。

## サイズとハッシュ

`arm-none-eabi-size build/pico_skyace.elf` の結果:

```text
text=101908  data=0  bss=59764  dec=161672  hex=27788
```

| 成果物 | サイズ | SHA-256 |
|---|---:|---|
| [`pico_skyace.uf2`](../../../build/pico_skyace.uf2) | 192000 bytes | `de9dd26802ebf2e3428ea22241d1fe87cd8d892c4a29264c6a9e12de923e6913` |
| `pico_skyace.elf` | 1063716 bytes | `b9818e5d331530984c59112adf54f717a3be73f6edafd1fd9d65f615d2790177` |
| `pico_skyace.bin` | 95768 bytes | `3c69e911faea5eca959bc653e26bca10fdfbe2e72bdf8d398237334e042cc007` |

ビルド定義にも `PICO_SKYACE_VERSION_STRING="1.0.0"` と
`PICO_SKYACE_BUILD_ID="2026-09-12T08:24:02Z"` が入っていることを確認した。

## PicoCalcエミュレーター最終スモーク

最終 `build/pico_skyace.bin` を公式LCD（`pio-rgb565`）、キーボード、FAT32
SDモデルへ接続し、`--quantum 1` で
[`title_version_smoke.json`](../../../tests/emulator/title_version_smoke.json)
を実行した。

結果は5/5ステップ `pass`、`scenario_done`、実行時間1690 ms。例外、
unsupported MMIO、LCD画素dropはなく、`pixels_dropped=0`、
`orphan_data_bytes=0` だった。タイトル画面は102400画素が非黒で描画された。

UART先頭:

```text
PICO_SKYACE_BOOT app=pico_skyace version=1.0.0 build="2026-09-12T08:24:02Z" WATCHDOG_CAUSED_REBOOT=0 clock_ok=1 sysclk_target_khz=250000 sysclk_actual_khz=250000 uart_baud=115200 phase=stdio
```

保存した証跡:

- レポート: [`report.json`](../../../build-artifacts/2026-09-12-v1.0.0/emulator-title/report.json)
- UARTログ: [`uart.log`](../../../build-artifacts/2026-09-12-v1.0.0/emulator-title/uart.log)
- 画面キャプチャ: [`title-version.png`](../../../build-artifacts/2026-09-12-v1.0.0/emulator-title/snapshots/title-version.png)
- 実機UARTログ（UF2Loader起動＋電源OFF/ON起動）:
  [`20260912_173039.log`](hardware/20260912_173039.log)
- SHA-256マニフェスト: [`MANIFEST.sha256`](MANIFEST.sha256)

## 実機受入との対応

実機ログ `/home/fuyuki/pico_dvl/codex/log/20260912_173039.log`（48行、SHA-256
`38455a14e43b762d8306298061be312b787371acd7c99c2fa4269e8c5b8a3bca`）を
プロジェクト内へ同一内容で保全した。ログ中の2つの起動列は、いずれも
`version=1.0.0`、`BUILD ID id="2026-09-12T08:24:02Z"`、
`sysclk_actual_khz=250000`で、最終UF2のSHA-256
`de9dd26802ebf2e3428ea22241d1fe87cd8d892c4a29264c6a9e12de923e6913`と対応する。

- 1列目（UF2Loaderによるブート）: `WATCHDOG_CAUSED_REBOOT=1`。Wave 4の
  Demoが31.299秒付近で開始し、30秒後の`MODE Demo->Title(timeout)`まで完走した。
- 2列目（電源OFF/ONによるブート）: `WATCHDOG_CAUSED_REBOOT=0`。Wave 4の
  Demoが31.439秒付近で開始し、30秒後の`MODE Demo->Title(timeout)`まで完走した。
  これはコールドブート時のウォッチドッグ異常なしを示す正式な確認列である。

UF2Loader列の`=1`は、その列の起動前に発生したリセット理由を示す値であり、
同列の実行中に再起動したことを示すものではない。電源OFF/ON列は`=0`で、両列とも
タイトル表示、LCD・キーボード・SD初期化、Wave 4デモ開始、30秒デモ終了からTitleへの
帰還を確認できた。この実機受入により、最終UF2のハードウェアゲートを完了とする。

## 公開工程

実機受入記録を確認後、リポジトリをPublicへ変更し、`v1.0.0`タグとGitHub Releaseを
作成した。Releaseは2026-09-12T08:49:04Zに公開され、URLは
<https://github.com/FuyukiYoneyama/pico_skyace/releases/tag/v1.0.0>。
添付した `pico_skyace.uf2` は本記録のSHA-256
`de9dd26802ebf2e3428ea22241d1fe87cd8d892c4a29264c6a9e12de923e6913`と一致する。
