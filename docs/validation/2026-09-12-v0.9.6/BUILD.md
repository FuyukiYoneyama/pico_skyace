# v0.9.6 ウェーブ境界描画修正ビルド記録

実施日: 2026-09-12

## 変更

- ウェーブクリア中に更新を止めていた撃墜演出とミサイルを、次ウェーブ開始時に
  明示的に破棄するようにした。
- これにより、前ウェーブで撃墜した敵の爆発や弾道が次ウェーブの画面へ残らない。
- 挙動修正に合わせて`VERSION`を`0.9.6`へ更新した。

## 成果物

- [pico_skyace.uf2](../../../build/pico_skyace.uf2)
  - SHA-256: `bda0dc980257023519131fa2d4808ee67c74ac9ddb4738922a353224c68467c2`
- `pico_skyace.bin`（エミュレーター入力）
  - SHA-256: `04d6ba26b1f5128264743126b247bd8b840804db9391f71a08ac2f6e4cad2d16`
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
cmake -S tests -B /tmp/pico-skyace-host-0.9.6 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/pico-skyace-host-0.9.6 -j2
ctest --test-dir /tmp/pico-skyace-host-0.9.6 --output-on-failure
```

タイトル表示の公式LCDエミュレーター確認（`--quantum 1`）:

- シナリオ: [`title_version_smoke.json`](../../../tests/emulator/title_version_smoke.json)
- 結果: `pass`、`scenario_done`、exceptionなし、unsupported MMIOなし、キーボードdrop 0
- UARTで`pico_skyace version 0.9.6`を確認
- LCD framebufferの非黒画素: `102400`
- [report.json](../../../build-artifacts/2026-09-12-v0.9.6/emulator-title/report.json)
- [uart.log](../../../build-artifacts/2026-09-12-v0.9.6/emulator-title/uart.log)
- [title-version.png](../../../build-artifacts/2026-09-12-v0.9.6/emulator-title/snapshots/title-version.png)

静的な経路確認として、`start_wave()`境界で`g_ex`（爆発）と`g_ms`（ミサイル）を
ゼロ化し、ウェーブ切替後の描画対象を新ウェーブの敵だけに限定することを確認した。

## 未実施

- 実機でのウェーブ切替表示確認
- ウェーブ全滅を含む長時間の公式LCD framebuffer経路
- 最終`1.0.0`更新、最終ビルド、タグ／リリース作成
