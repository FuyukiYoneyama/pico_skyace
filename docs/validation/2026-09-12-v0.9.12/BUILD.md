# v0.9.12 ミサイル近接配置・README構成調整ビルド記録

実施日: 2026-09-12

## 変更

- ミサイルの翼下パイロン左右オフセットを0.5m相当へ縮め、翼から大きく離れて
  見えないようにした。成功した発射ごとの左／右交互切り替えは維持する。
- READMEは遊び方・操作・戦闘の流れを先頭に置き、ファンメイド権利注記を末尾の
  `※` 注記へ移し、技術スペックを後段へ移動した。
- 変更に合わせて `VERSION` を `0.9.12` へ更新した。

## 成果物

- [pico_skyace.uf2](../../../build/pico_skyace.uf2)
  - SHA-256: `9237750e5cf4150b83188a28f5ece2bdcdd9ff9283ca76980c65e58f3e8a6daf`
- `pico_skyace.bin`（エミュレーター入力）
  - SHA-256: `709189b5027f68bc5b4360192808219187aa2f9b5eddab58b4b7d0ccffea758e`
- `pico_skyace.elf`（`arm-none-eabi-size`）:
  `text=99052 data=0 bss=59188 dec=158240 hex=26a20`

## 検証

RP2040 Release ビルド:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

警告なしで完了した。ホストロジックテストも 1/1 合格:

```text
cmake -S tests -B /tmp/pico-skyace-host-0.9.12 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/pico-skyace-host-0.9.12 -j2
ctest --test-dir /tmp/pico-skyace-host-0.9.12 --output-on-failure
```

公式 LCD エミュレーター（`--quantum 1`）でタイトル表示と、Enter 後の発射確認を
実施した。タイトルシナリオは `pass`、UART の `pico_skyace version 0.9.12`、
exception なし、unsupported MMIO なし、framebuffer の非黒画素 `102400` だった。
発射シナリオも `pass`、非黒画素 `102400` で、連続2発の1発目左・2発目右の画像を
保存した。

- [タイトルシナリオ](../../../tests/emulator/title_version_smoke.json) / [report.json](../../../build-artifacts/2026-09-12-v0.9.12/emulator-title/report.json)
- [missile-left.png](../../../build-artifacts/2026-09-12-v0.9.12/emulator-missile-visual/snapshots/missile-left.png)
- [missile-right.png](../../../build-artifacts/2026-09-12-v0.9.12/emulator-missile-visual/snapshots/missile-right.png)
- [発射シナリオ report.json](../../../build-artifacts/2026-09-12-v0.9.12/emulator-missile-visual/report.json)

ソース経路も確認した。`fire_missile()` はyawに合わせた機体右方向へ0.5m相当の
オフセットと翼下2mの高さ差を加え、成功した発射後に左右フラグを反転する。
`render_hud()` の機銃トレーサは自機の機首座標から照準へ1本だけ描画する。

READMEはタイトル直後に遊び方を置き、操作表と戦闘の流れを先に読める構成にした。
権利に関する説明は冒頭から除き、末尾の `## 注記` に `※` 付きで残している。

## 未実施

- 実機でのミサイル発射位置・左右交互の見え方、および機首トレーサの最終確認。
- ウェーブ全滅を含む長時間の公式 LCD framebuffer 経路。
- 最終 `1.0.0` 更新、最終ビルド、タグ／リリース作成。
