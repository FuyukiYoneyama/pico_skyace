# v0.9.14 ハイスコア最上段表示ビルド記録

実施日: 2026-09-12

## 変更

- タイトル画面とゲームオーバー画面の `HI-SCORE` を画面最上段へ移動した。
- タイトル画面のバージョン表示と結果情報に重ならないよう、共通の最上段位置を使用した。
- 表示変更に合わせて `VERSION` を `0.9.14` へ更新した。

## 成果物

- [pico_skyace.uf2](../../../build/pico_skyace.uf2)
  - SHA-256: `f4eab79c9acc15f21fda3b8dd4e72683a19996941a3dd21401a5af8d043bd4d8`
- `pico_skyace.bin`（エミュレーター入力）
  - SHA-256: `22d2350eb6cc761da12f6d394905fcd4cafcd87df2630dbfc6bfda15c81e72b5`
- `pico_skyace.elf`（`arm-none-eabi-size`）:
  `text=101524 data=0 bss=59764 dec=161288 hex=27608`

## 検証

RP2040 Releaseビルド:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

77/77ターゲットが警告なしで完了した。ホストロジックテストも1/1合格:

```text
cmake --build /tmp/pico-skyace-host-0.9.13 -j2
ctest --test-dir /tmp/pico-skyace-host-0.9.13 --output-on-failure
```

公式LCDエミュレーターにFAT32 SDモデルを接続し、タイトル表示、バージョン、
ハイスコア読み込み経路を確認した。UARTには `pico_skyace version 0.9.14` と
`HIGHSCORE load status=empty score=0` が出力され、例外なし、unsupported MMIOなし、
非黒画素102400で合格した。キャプチャ画像では `HI-SCORE 000000` が画面最上段に、
タイトルと `VERSION 0.9.14` がその下に描画されている。

- [タイトルシナリオ](../../../tests/emulator/title_version_smoke.json)
- [emulator report.json](../../../build-artifacts/2026-09-12-v0.9.14/emulator-title/report.json)
- [UARTログ](../../../build-artifacts/2026-09-12-v0.9.14/emulator-title/uart.log)
- [title-version.png](../../../build-artifacts/2026-09-12-v0.9.14/emulator-title/snapshots/title-version.png)

## 未実施・適用範囲

- ハイスコアの新記録発生からSD再起動後の再読込までの実機一連確認は未実施。
- 実機での音量・LCD電気特性、最終 `1.0.0` 更新、最終ビルド、タグ／リリース作成は
  これまでどおり保留する。
