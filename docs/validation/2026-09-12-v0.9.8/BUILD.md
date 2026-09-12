# v0.9.8 接近警報調整ビルド記録

実施日: 2026-09-12

## 変更

- 接近ブザーを、攻撃フェーズへ入った瞬間ではなく、敵が実際に自機へ近づいている
  場合だけ鳴らすようにした。
- 警報距離はWave補正後の射撃距離+90mを基準に、620〜820mへ制限した。
- 距離が縮んでいることを1フレームごとに確認し、遠方での過剰な警報を抑えた。
- 挙動変更に合わせて`VERSION`を`0.9.8`へ更新した。

## 成果物

- [pico_skyace.uf2](../../../build/pico_skyace.uf2)
  - SHA-256: `e0a5605897e71af9d3e93e14f73628caec64f7fc272475cbec9bffc6b9a2d9d1`
- `pico_skyace.bin`（エミュレーター入力）
  - SHA-256: `556e22dc3e0e593f409ac80075d564a9f333aea681ab9eeee74da97314dcf894`
- `pico_skyace.elf`（`arm-none-eabi-size`）:
  `text=98708 data=0 bss=59184 dec=157892`

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
- UARTで`pico_skyace version 0.9.8`を確認
- LCD framebufferの非黒画素: `102400`
- [report.json](../../../build-artifacts/2026-09-12-v0.9.8/emulator-title/report.json)
- [uart.log](../../../build-artifacts/2026-09-12-v0.9.8/emulator-title/uart.log)
- [title-version.png](../../../build-artifacts/2026-09-12-v0.9.8/emulator-title/snapshots/title-version.png)

## 未実施

- 実機スピーカーでの警報距離・音量・識別性確認
- 警報発生を含む長時間の公式LCD framebuffer経路
- 最終`1.0.0`更新、最終ビルド、タグ／リリース作成
