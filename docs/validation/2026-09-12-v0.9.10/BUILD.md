# v0.9.10 被弾演出・高度速度連動ビルド記録

実施日: 2026-09-12

## 変更

- 被弾時に自機スプライトを交互フレームで明るく点滅させ、既存の画面端の
  赤い被弾枠と併用する。
- 実際の垂直速度に応じて、上昇中は速度を減速し、下降中は速度を加速する。
  水平飛行時は従来のスロットル制御を維持する。
- 変更に合わせて `VERSION` を `0.9.10` へ更新した。

## 成果物

- [pico_skyace.uf2](../../../build/pico_skyace.uf2)
  - SHA-256: `6f086db38051f681967ef52ce1c6f28f8758ff2ff5cf77a06dc4ae9856b50124`
- `pico_skyace.bin`（エミュレーター入力）
  - SHA-256: `8914f1919fa314d945872eab1656e253fb9b5f3ac46d0481420a205b07a50a63`
- `pico_skyace.elf`（`arm-none-eabi-size`）:
  `text=98892 data=0 bss=59184 dec=158076 hex=2697c`

## 検証

RP2040 Release ビルド:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

警告なしで完了した。ホストロジックテストも 1/1 合格:

```text
cmake -S tests -B /tmp/pico-skyace-host-0.9.10 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/pico-skyace-host-0.9.10 -j2
ctest --test-dir /tmp/pico-skyace-host-0.9.10 --output-on-failure
```

公式 LCD エミュレーターのタイトル表示確認（`--quantum 1`）:

- シナリオ: [`title_version_smoke.json`](../../../tests/emulator/title_version_smoke.json)
- 結果: `pass`、`scenario_done`、exception なし、unsupported MMIO なし、キーボード drop 0
- UART で `pico_skyace version 0.9.10` を確認
- LCD framebuffer の非黒画素: `102400`
- [report.json](../../../build-artifacts/2026-09-12-v0.9.10/emulator-title/report.json)
- [uart.log](../../../build-artifacts/2026-09-12-v0.9.10/emulator-title/uart.log)
- [title-version.png](../../../build-artifacts/2026-09-12-v0.9.10/emulator-title/snapshots/title-version.png)

同じ公式 LCD エミュレーターで Enter 後の通常プレイも確認した。さらに加速・右旋回・
上昇キーを順に入力する操作スモークを通し、速度／高度更新を含むフレームが描画された。
両シナリオとも `pass`、exception なし、unsupported MMIO なし、framebuffer の非黒画素
`102400` だった。

- 通常プレイ: [`boot_play_smoke.json`](../../../tests/emulator/boot_play_smoke.json)
  / [report.json](../../../build-artifacts/2026-09-12-v0.9.10/emulator-play/report.json)
- 操作スモーク: [`audio_controls_smoke.json`](../../../tests/emulator/audio_controls_smoke.json)
  / [report.json](../../../build-artifacts/2026-09-12-v0.9.10/emulator-controls/report.json)

ソース経路も確認した。`apply_player_damage()` は被弾時に `dmg_flash` を設定し、
`render_player_plane()` は有効期間中の交互フレームで明るいパレットへ切り替える。
速度変更は `speed * sin(pitch)` で求めた垂直速度から計算し、45〜150 m/s の範囲に
クランプしたうえで、同じピッチ成分を移動へ使う。

## 未実施

- 実機での自機フラッシュの見え方、および上昇・下降時の速度変化量の最終調整。
- ウェーブ全滅を含む長時間の公式 LCD framebuffer 経路。
- 最終 `1.0.0` 更新、最終ビルド、タグ／リリース作成。
