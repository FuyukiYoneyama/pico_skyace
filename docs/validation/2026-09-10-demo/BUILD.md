# v0.9.3 デモ／タイトル曲再調整 ビルド記録

実施日: 2026-09-10

## 成果物

- [pico_skyace.uf2](../../../build/pico_skyace.uf2)
  - SHA-256: `2cdcb88fc1ec0cb841b57efb6677294521e19c773aa9ec04502c307b35a99b44`

最新の製品ビルドは標準の`build/`に置く。`build-artifacts/`には検証ログと画面
キャプチャだけを残し、再生成可能な中間・診断バイナリは保管しない。

製品ビルドは`PICO_SKYACE_TITLE_DEMO_DELAY_MS`を指定せず、タイトルからデモへ
30秒で遷移する既定値を使用した。`arm-none-eabi-size`は`text=93908`、
`data=0`、`bss=59164`、`dec=153072`。`picotool info -a`でUF2のfamily IDが
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

## 製品BINの完全タイミング経路（2026-09-10）

以下は`PICO_SKYACE_TITLE_DEMO_DELAY_MS`を定義していない製品BIN
（SHA-256: `db495f5f2e36a0cd2fa0f4921679a3aac3adcb61861dded8a56eedc91ca467c6`）を
使った長時間シナリオである。シナリオの100秒窓はタイミングを短縮するものではなく、
LCDエミュレーターのフレーム転送遅延を吸収する観測余裕である。

### Title 30秒 → Demo 30秒 → Title

- シナリオ: [`title_demo_cycle_product.json`](../../../tests/emulator/title_demo_cycle_product.json)
- 実行条件: `picocalc-run --cycles 50000000000 --quantum 65536 --board picocalc --lcd-variant pio-rgb565 --keyboard`
- 結果: `pass`、`scenario_done`、exceptionなし、unsupported MMIOなし、キーボードdrop 0。
- ステップ観測: 初期化 `7,147 ms`、`MODE Title->Demo` `40,753 ms`、
  `MODE Demo->Title(timeout)` `74,644 ms`。
- UART内のRP2040タイマー値: `time_us=40711326` → `74607697`。
  30秒期限後のマーカー時刻は、描画転送を終えたフレーム境界で観測されるため、
  LCDエミュレーター上の壁時計には遅延が含まれる。
- [report.json](../../../build-artifacts/2026-09-10-demo/emulator-production-title-demo-cycle/report.json)
- [uart.log](../../../build-artifacts/2026-09-10-demo/emulator-production-title-demo-cycle/uart.log)
- [scenario.json](../../../build-artifacts/2026-09-10-demo/emulator-production-title-demo-cycle/scenario.json)

公式のLCD付きバックエンド（commit `58e73010636bb1b60fdb1ccace40db29b5bb96cc`、
dirty=false）で実行し、320×320 framebufferは非黒画素`102400`、LCD書込は
`1126400` pixelsだった。

### GameOver 10秒 → Title

- シナリオ: [`gameover_timeout_product.json`](../../../tests/emulator/gameover_timeout_product.json)
- 入力: Enterで開始後、Down＋O（最大スロットル）を保持して通常の地面衝突を起こし、
  GameOverマーカー後に入力を解放。
- 実行条件: `picocalc-run --cycles 30000000000 --quantum 65536 --board picocalc --lcd-variant pio-rgb565 --keyboard`
- 結果: 全9ステップ`pass`、`scenario_done`、exceptionなし、unsupported MMIOなし、
  キーボードdrop 0。
- ステップ観測: Play→GameOver `71,194 ms`、GameOver→Title `81,200 ms`。
- UART内のRP2040タイマー値: `MODE Play->GameOver ... time_us=71157851` →
  `MODE GameOver->Title(timeout) ... time_us=81159000`（差分`10,001,149 µs`）。
- [report.json](../../../build-artifacts/2026-09-10-demo/emulator-production-gameover-timeout/report.json)
- [uart.log](../../../build-artifacts/2026-09-10-demo/emulator-production-gameover-timeout/uart.log)
- [scenario.json](../../../build-artifacts/2026-09-10-demo/emulator-production-gameover-timeout/scenario.json)
- [高速シンク差分](../../../build-artifacts/2026-09-10-demo/emulator-production-gameover-timeout/backend-fast-uart.patch)

GameOverの長時間UART実行は、LCD画素デコードを省略してPIO TX FIFO/TXSTALLだけを
即時成立させる一時的な高速シンク（backend commit
`f32eba1878aeabc6dfc8954b363230ef1e4c2b52`, dirty=true）を使用した。これは
UART専用の状態遷移検証であり、framebuffer/LCD電気タイミングの合格を意味しない。
製品BIN自体は同じ`db495f…`で、ゲーム更新・衝突・入力・RP2040タイマーは変更していない。

## 未確認

実機でのスピーカー音質（拡張フレーズの聴感、パーカッション音量、エンジン音の
スロットル／旋回追従）、GameOver画面の実機表示、電源・SDカードを含む実機経路は
未確認。`/dev/ttyACM*`/`/dev/ttyUSB*`がこの環境に存在しないため、UF2書き込み後に
実機で確認する。
