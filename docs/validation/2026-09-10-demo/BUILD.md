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
UART専用の状態遷移検証であり、公式LCDモデルのframebuffer／通常PIO転送の合格を意味しない。
製品BIN自体は同じ`db495f…`で、ゲーム更新・衝突・入力・RP2040タイマーは変更していない。

## 追加の非実機検証（2026-09-10）

製品BIN（SHA-256: `db495f5f2e36a0cd2fa0f4921679a3aac3adcb61861dded8a56eedc91ca467c6`）を
使い、厳密ガード、再出撃、入力境界、SD有無、描画、代表負荷を追加確認した。
全レポート・UARTログ・シナリオ・必要な画面キャプチャは
[`2026-09-10-nonhardware`](../../../build-artifacts/2026-09-10-nonhardware/) に保存している。

### 厳密UARTガードと入力境界

- [`strict-title-uart`](../../../build-artifacts/2026-09-10-nonhardware/strict-title-uart/):
  Title→Demoを`time_us=30584627`、Demo→Titleを`60648619`で検出した。差分は
  `30063992 µs`で、シナリオの33秒観測窓内に合格した。
- [`strict-gameover-uart`](../../../build-artifacts/2026-09-10-nonhardware/strict-gameover-uart/):
  Down＋Oの通常墜落でGameOverを`71157851 µs`、Title帰還を`81159000 µs`で検出した。
  差分は`10001149 µs`である。
- [`direct-retry-uart`](../../../build-artifacts/2026-09-10-nonhardware/direct-retry-uart/):
  GameOverでEnterを押し、`MODE GameOver->Play`とWave初期化を確認した。
- [`weapon-edges-uart`](../../../build-artifacts/2026-09-10-nonhardware/weapon-edges-uart/):
  ミサイル枠飽和、機銃・スロットル・旋回の同時入力後もシナリオが継続した。

### 描画とSD

公式LCDモデル（backend commit `58e73010636bb1b60fdb1ccace40db29b5bb96cc`）で
Boot/Play、Pause→Resume、音声操作入力、武器境界を実行し、各シナリオは合格、
320×320 framebufferの非黒画素は`102400`だった。

- [`boot-play-framebuffer`](../../../build-artifacts/2026-09-10-nonhardware/boot-play-framebuffer/)
- [`pause-resume-framebuffer`](../../../build-artifacts/2026-09-10-nonhardware/pause-resume-framebuffer/)
- [`audio-controls-framebuffer`](../../../build-artifacts/2026-09-10-nonhardware/audio-controls-framebuffer/)
- [`weapon-edges-framebuffer`](../../../build-artifacts/2026-09-10-nonhardware/weapon-edges-framebuffer/)

F5はFAT32 SDモデルで`SCREENSHOT done status=ok`まで完走し、コマンド636、読込163
ブロック、書込464ブロック、プロトコルエラー0を記録した（
[`screenshot-sd`](../../../build-artifacts/2026-09-10-nonhardware/screenshot-sd/)）。
カードなしではdetect=High、`SD init status=no_card`、`SCREENSHOT error stage=mount`
を確認した（
[`screenshot-no-sd`](../../../build-artifacts/2026-09-10-nonhardware/screenshot-no-sd/)）。
SDのカード検出Highは一時的なエミュレーター診断差分で明示しており、製品BINには含まれない。

### 代表上限負荷

[`upper-load-2m`](../../../build-artifacts/2026-09-10-nonhardware/upper-load-2m/) では、
製品BINでO・右・↑・Spaceを保持した120秒の代表負荷を実行した。約30.27 Gcycles、
`scenario_done`、例外なし、unsupported MMIOなし、キーボードdrop 0で合格した。
これは20分連続プレイとフレーム時間p95のリリースゲートを満たすものではない。

診断コードを一時的に加えたBINでは、LCD画素デコードを省略した高速PIOシンク上で
300フレームの処理時間とスタック高水位も集計した（
[`perf-diagnostic`](../../../build-artifacts/2026-09-10-nonhardware/perf-diagnostic/)）。
結果はmin `183502 µs`、平均 `247292 µs`、p95上限 `264999 µs`、max `268438 µs`、
スタック使用`944/4096 bytes`だった。ただし、これはエミュレーター仮想時間と診断
バックエンドの値であり、実機の33ms合否や実LCD転送込みの性能を示さない。
製品BINには計測コードを含めていない。

長時間UARTとSDの一部はLCD画素デコードを省略した高速PIOシンク
（[`backend-fast-uart.patch`](../../../build-artifacts/2026-09-10-nonhardware/backend-fast-uart.patch)）
を使用した。したがって、それらは状態遷移・入力・ファイルI/Oの検証であり、
公式LCD framebufferや通常PIO転送モデルの合格とは別扱いである。
なお、本プロジェクトの今回の検証では電圧・波形・信号品質を測定しておらず、
それらを確認済みとも公開条件ともしていない。ここでいうPIO転送は、エミュレーター内の
プロトコル経路だけを指す。

## 実機確認（ユーザー報告）

2026-09-10に、ユーザーから現行v0.9.3製品UF2の手動確認結果を受領した。
UF2のハッシュ、写真／UARTログ、実施時刻、計測器による電気的測定値は提供されて
いないため、これは再現可能な機械計測証跡ではなく、実機スモーク確認の記録である。

- コールドブートと電源再投入: 問題なし。
- LCD、キーボード、同時入力: 問題なし。
- BGM（パーカッションを含む）、効果音、エンジン音: 問題なし。
- Title→Demo→Title、GameOver→Title: いずれも問題なし。
- SDあり／なしのF5経路: 動作。SDありでスクリーンショット保存を確認。

この報告により、現行v0.9.3版の実機スモーク確認は完了扱いとする。ただし、
`VERSION`を1.0.0へ更新した最終UF2はまだ作成していないため、リリース前に同じ
確認を最終UF2でも繰り返す。残る性能ゲートは20分上限負荷と実機のフレーム時間・
メモリ計測である。
