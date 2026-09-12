# v0.9.7 HUD配置修正ビルド記録

実施日: 2026-09-12

## 変更

- GUN残弾をレーダーと重ならない最下行へ移動した。
- 右側のHPとミサイル表示を上下入れ替え、HPを上段、ミサイルを最下段へ配置した。
- 最下行を左GUN／右ミサイルの兵装行として揃えた。
- レーダーを2px上へ移動し、兵装行との境界を確保した。
- 表示変更に合わせて`VERSION`を`0.9.7`へ更新した。

## 成果物

- [pico_skyace.uf2](../../../build/pico_skyace.uf2)
  - SHA-256: `0da756c21556cdab5f2e1aed11f2971c541308e7e4633aeadc814e25e5495d7e`
- `pico_skyace.bin`（エミュレーター入力）
  - SHA-256: `244fa967c0d219fa7d2dc729f4d991f51d3e2ca16ad7b99496dd2a1682ee21ae`
- `pico_skyace.elf`（`arm-none-eabi-size`）:
  `text=98684 data=0 bss=59184 dec=157868`

## 検証

RP2040 Releaseビルド:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

警告なしで完了した。ホストロジックテストも1/1合格:

```text
cmake -S tests -B /tmp/pico-skyace-host-0.9.7 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/pico-skyace-host-0.9.7 -j2
ctest --test-dir /tmp/pico-skyace-host-0.9.7 --output-on-failure
```

タイトル表示の公式LCDエミュレーター確認（`--quantum 1`）:

- シナリオ: [`title_version_smoke.json`](../../../tests/emulator/title_version_smoke.json)
- 結果: `pass`、`scenario_done`、exceptionなし、unsupported MMIOなし、キーボードdrop 0
- UARTで`pico_skyace version 0.9.7`を確認
- LCD framebufferの非黒画素: `102400`
- [report.json](../../../build-artifacts/2026-09-12-v0.9.7/emulator-title/report.json)
- [uart.log](../../../build-artifacts/2026-09-12-v0.9.7/emulator-title/uart.log)
- [title-version.png](../../../build-artifacts/2026-09-12-v0.9.7/emulator-title/snapshots/title-version.png)

HUDの座標はソースレビューで確認した。内部160x160画面で、レーダーは中心`(21,135)`・
半径16、SPD／ALTは`y=137/145`、HPは`y=146`、GUN／ミサイルは`y=153`で、
最下行のGUNとミサイルがレーダーおよびHPバーに重ならない配置になっている。

## 未実施

- 実機での新HUD配置の視認性確認
- ウェーブ切替・ゲームオーバーを含む長時間の公式LCD framebuffer経路
- 最終`1.0.0`更新、最終ビルド、タグ／リリース作成
