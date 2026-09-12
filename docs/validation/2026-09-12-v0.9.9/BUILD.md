# v0.9.9 ウェーブクリア演出順序修正ビルド記録

実施日: 2026-09-12

## 変更

- 最後の敵を倒した直後にクリア画面へ切り替えず、残っている爆発演出が完全に消える
  まで通常画面を描画するようにした。
- 爆発がすべて終了した時点で、`WAVE CLEAR`画面とクリアファンファーレを開始する。
- 挙動変更に合わせて`VERSION`を`0.9.9`へ更新した。

## 成果物

- [pico_skyace.uf2](../../../build/pico_skyace.uf2)
  - SHA-256: `f0eb4c466f8538263a8f9100369a447c0a34eb33c4d72f2b460f9655909af95e`
- `pico_skyace.bin`（エミュレーター入力）
  - SHA-256: `077e9fc777194d65eb3b2c76ac4c03460da20445c1ae361b3f0d0725ffdba4d6`
- `pico_skyace.elf`（`arm-none-eabi-size`）:
  `text=98836 data=0 bss=59184 dec=158020`

## 検証

RP2040 Releaseビルド:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

警告なしで完了した。ホストロジックテストも1/1合格:

```text
cmake -S tests -B /tmp/pico-skyace-host-0.9.9 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/pico-skyace-host-0.9.9 -j2
ctest --test-dir /tmp/pico-skyace-host-0.9.9 --output-on-failure
```

タイトル表示の公式LCDエミュレーター確認（`--quantum 1`）:

- シナリオ: [`title_version_smoke.json`](../../../tests/emulator/title_version_smoke.json)
- 結果: `pass`、`scenario_done`、exceptionなし、unsupported MMIOなし、キーボードdrop 0
- UARTで`pico_skyace version 0.9.9`を確認
- LCD framebufferの非黒画素: `102400`
- [report.json](../../../build-artifacts/2026-09-12-v0.9.9/emulator-title/report.json)
- [uart.log](../../../build-artifacts/2026-09-12-v0.9.9/emulator-title/uart.log)
- [title-version.png](../../../build-artifacts/2026-09-12-v0.9.9/emulator-title/snapshots/title-version.png)

ソース経路を確認し、最後の敵がいなくなった場合でも`any_active_explosion()`がtrueの
間はクリアタイマーを開始せず、爆発終了後にだけクリアタイマーとファンファーレを
開始することを確認した。

## 未実施

- 実機での爆発終了後のクリア画面タイミング確認
- ウェーブ全滅を含む長時間の公式LCD framebuffer経路
- 最終`1.0.0`更新、最終ビルド、タグ／リリース作成
