# v0.9.11 ミサイル発射位置・機銃トレーサ調整ビルド記録

実施日: 2026-09-12

## 変更

- ミサイルの発射位置を機体中心から左右の翼下パイロンへ移し、成功した発射ごとに
  左右を交互に切り替える。発射後の速度ベクトルと誘導・当たり判定は変更しない。
- 機銃のトレーサを機首から照準へ伸びる1本に整理した。
- 変更に合わせて `VERSION` を `0.9.11` へ更新した。

## 成果物

- [pico_skyace.uf2](../../../build/pico_skyace.uf2)
  - SHA-256: `87b29d20746c108a0471d147c13d1db5daa575ff1412725da6aed5642bdc3d2b`
- `pico_skyace.bin`（エミュレーター入力）
  - SHA-256: `ed3de28ad7d1bfa35c1716e7e79acd3863d225a28180fcfac194b3122027b574`
- `pico_skyace.elf`（`arm-none-eabi-size`）:
  `text=99068 data=0 bss=59188 dec=158256 hex=26a30`

## 検証

RP2040 Release ビルド:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

警告なしで完了した。ホストロジックテストも 1/1 合格:

```text
cmake -S tests -B /tmp/pico-skyace-host-0.9.11 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/pico-skyace-host-0.9.11 -j2
ctest --test-dir /tmp/pico-skyace-host-0.9.11 --output-on-failure
```

公式 LCD エミュレーター（`--quantum 1`）で、タイトル、Enter 後の通常プレイ、加速・
右旋回・上昇入力を含む操作スモークを確認した。いずれも `pass`、exception なし、
unsupported MMIO なし、framebuffer の非黒画素 `102400` だった。

- [タイトルシナリオ](../../../tests/emulator/title_version_smoke.json) / [report.json](../../../build-artifacts/2026-09-12-v0.9.11/emulator-title/report.json)
- [通常プレイシナリオ](../../../tests/emulator/boot_play_smoke.json) / [report.json](../../../build-artifacts/2026-09-12-v0.9.11/emulator-play/report.json)
- [操作スモーク](../../../tests/emulator/audio_controls_smoke.json) / [report.json](../../../build-artifacts/2026-09-12-v0.9.11/emulator-controls/report.json)

発射位置確認用の短時間シナリオでは、連続2発のミサイルを発射し、1発目が左翼下、
2発目が右翼下から出るフレームを保存した。シナリオは `pass`、framebuffer の非黒画素は
`102400` だった。

- [missile-left.png](../../../build-artifacts/2026-09-12-v0.9.11/emulator-missile-visual/snapshots/missile-left.png)
- [missile-right.png](../../../build-artifacts/2026-09-12-v0.9.11/emulator-missile-visual/snapshots/missile-right.png)
- [report.json](../../../build-artifacts/2026-09-12-v0.9.11/emulator-missile-visual/report.json)

ソース経路も確認した。`fire_missile()` はyawに合わせた機体右方向へ翼下オフセットを
加え、成功した発射後に左右フラグを反転する。`render_hud()` の機銃トレーサは自機の
ロールと上下動を反映した機首座標から、照準へ1本だけ描画する。

## 未実施

- 実機でのミサイル発射位置・左右交互の見え方、および機首トレーサの最終確認。
- ウェーブ全滅を含む長時間の公式 LCD framebuffer 経路。
- 最終 `1.0.0` 更新、最終ビルド、タグ／リリース作成。
