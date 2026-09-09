# v0.9.3 デモ／タイトル曲再調整 ビルド記録

実施日: 2026-09-10

## 成果物

- [pico_skyace.uf2](../../../build-artifacts/2026-09-10-demo/pico_skyace.uf2)
  - SHA-256: `d9ecc5ef699aa2806f0e14a0e693485dffdd88e39d2075bd93a2af41df4b455b`
- [pico_skyace.elf](../../../build-artifacts/2026-09-10-demo/pico_skyace.elf)
  - SHA-256: `81961b3050c35a9b37f1dc7bf7ec044b585932cc3c6d30e12384c7f9809095fc`
- [pico_skyace.bin](../../../build-artifacts/2026-09-10-demo/pico_skyace.bin)
  - SHA-256: `b4d2e565f1a40e5ea7d4a766dc89dbefcfcfe4241da776a2ff8cdd110d2abb7d`

製品ビルドは`PICO_SKYACE_TITLE_DEMO_DELAY_MS`を指定せず、タイトルからデモへ
30秒で遷移する既定値を使用した。`arm-none-eabi-size`は`text=93204`、
`data=0`、`bss=59148`、`dec=152352`。`picotool info -a`でUF2のfamily IDが
`rp2040`、build attributesが`Release`であることを確認した。

## 検証

ホストロジックテスト:

```sh
cmake -S tests -B <tmp-host-build> -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build <tmp-host-build> -j2
ctest --test-dir <tmp-host-build> --output-on-failure
```

結果: 1/1合格。

製品BINのタイトル表示（バージョン`0.9.3`）:

- シナリオ: [`title_version_smoke.json`](../../../tests/emulator/title_version_smoke.json)
- 結果: `pass`、仮想620ms、exceptionなし、unsupported MMIOなし、キーボードdrop 0
- [report.json](../../../build-artifacts/2026-09-10-demo/emulator-production-title/report.json)
- [title-version.png](../../../build-artifacts/2026-09-10-demo/emulator-production-title/snapshots/title-version.png)

実プレイ経路のデモ診断（最終ソースから作った短縮テストBIN）:

- `PICO_SKYACE_TITLE_DEMO_DELAY_MS=1000`だけを追加した診断ビルドで、待ち時間を
  1秒に短縮した。ゲーム側のデモ更新・敵AI・武器・衝突・Wave・HUD・描画は製品BINと同一。
- 再現時は`cmake`に`-DCMAKE_CXX_FLAGS='-mcpu=cortex-m0plus -mthumb
  -DPICO_SKYACE_TITLE_DEMO_DELAY_MS=1000'`を渡す（製品ビルドには渡さない）。
- シナリオ: `pico-skyace-demo-series`（Title→Demo後、0/0.5/1/1.5/2秒で撮影）
- 結果: 全11ステップ`pass`、仮想3.44秒、exceptionなし、unsupported MMIOなし、
  キーボードdrop 0。画面は固定デモ用描画ではなく、通常プレイと同じ空・機体・敵・
  レーダー・HUDを表示する。
- [report.json](../../../build-artifacts/2026-09-10-demo/emulator-short-delay-series/report.json)
- [demo-2000.png](../../../build-artifacts/2026-09-10-demo/emulator-short-delay-series/snapshots/demo-2000.png)

さらにデモ開始後3秒まで走らせたシナリオでも`pass`し、実際にミサイル発射後の
`MSL 11`を含むHUDを取得した。

- [report.json](../../../build-artifacts/2026-09-10-demo/emulator-short-delay-long/report.json)
- [demo-real-long.png](../../../build-artifacts/2026-09-10-demo/emulator-short-delay-long/snapshots/demo-real-long.png)

短縮診断BINのUARTには`MODE Title->Demo`が出力され、連続フレームでは実プレイの
ミサイル残数・敵表示・Wave表示が更新される。製品BINのタイトル待ち時間は30秒の
ままなので、この診断結果を製品版の1秒設定と混同しないこと。

## 未確認

実機でのスピーカー音質（拡張フレーズの聴感、パーカッション音量、エンジン音の
スロットル／旋回追従）と、製品BINでの30秒待ちをLCDエミュレーターで最後まで
待つ長時間シナリオは未実施。UF2書き込み後に実機で確認する。
