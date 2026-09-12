# v0.9.5 ゲームオーバーBGM追加ビルド記録

実施日: 2026-09-12

## 変更

- ゲームオーバー突入時にタイトル曲を止め、下降メロディー・持続和音・低音・
  疎なキック／タムで構成した専用8秒BGMをループ再生する。
- Titleへ戻る時はタイトル曲へ、Playへ再出撃する時はBGM停止へ戻す。
- タイトル／デモのBGM、デモ中のタイトル曲継続は変更しない。
- 全ゲームオーバーBGMトラックの長さを8,000msへ揃え、静的検査を追加した。
- `VERSION`を`0.9.5`へ更新した。

## 成果物

- [pico_skyace.uf2](../../../build/pico_skyace.uf2)
  - SHA-256: `d58781e2ee7817e15b4b06dab5e044dcec483e5fb168341b147dca130096f411`
- `pico_skyace.bin`（エミュレーター入力）
  - SHA-256: `0782f19cf72947c3b04e875f61f2563cac520142820011b55bd0ee08fe3d685f`
- `pico_skyace.elf`（`arm-none-eabi-size`）:
  `text=98596 data=0 bss=59184 dec=157780`

## 検証

ホストロジックテスト:

```text
cmake -S tests -B /tmp/pico-skyace-host-0.9.5 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/pico-skyace-host-0.9.5 -j2
ctest --test-dir /tmp/pico-skyace-host-0.9.5 --output-on-failure
```

結果: 1/1合格。ゲームルールの既存テストに加え、RP2040ビルド時にゲームオーバー用
5トラックがすべて8,000msで一致する`static_assert`を通過した。

RP2040 Releaseビルドは警告なしで完了した。CMakeは`VERSION`を読み込み、タイトル画面
表示と起動UARTへ`0.9.5`を渡す。

タイトル表示の公式LCDエミュレーター確認:

- シナリオ: [`title_version_smoke.json`](../../../tests/emulator/title_version_smoke.json)
- 結果: `pass`、`scenario_done`、exceptionなし、unsupported MMIOなし、キーボードdrop 0
- UARTで`pico_skyace version 0.9.5`を確認
- [report.json](../../../build-artifacts/2026-09-12-v0.9.5/emulator-title/report.json)
- [uart.log](../../../build-artifacts/2026-09-12-v0.9.5/emulator-title/uart.log)
- [title-version.png](../../../build-artifacts/2026-09-12-v0.9.5/emulator-title/snapshots/title-version.png)

ゲームオーバー専用BGMの選択は、`finalize_gameover()`から
`play_gameover_music()`を呼ぶソース経路と、全トラック長の静的検査で確認した。
実機スピーカーでの音色・音量確認、およびゲームオーバー10秒経路を含む長時間LCD
エミュレーター実行はこの記録では完走結果を取得していないため、合格とは扱わない。

## 未実施

- 実機でのゲームオーバーBGMの音色・音量・状態遷移確認
- 製品タイミングのGameOver→Title長時間LCD経路
- 最終`1.0.0`更新、最終ビルド、タグ／リリース作成
