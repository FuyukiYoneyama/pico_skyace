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
確認を最終UF2でも繰り返す。20分上限負荷は下記のユーザー報告で受入済みであり、
残る性能ゲートは実機のフレーム時間・メモリ／実行時スタック計測である。

## 未リリース Wave攻撃性調整ビルド（2026-09-10）

実機フィードバックを受けたWave別の敵AI調整を、`VERSION`を1.0.0へ変更せずに
標準の`build/`へビルドした。これはプレイテスト用の未リリースビルドであり、
上記のv0.9.3実機報告の対象には含まれない。

- ホストテスト: CTest `1/1` 合格。
- RP2040 Releaseビルド: 新規警告なし。
- サイズ: `text=94380 data=0 bss=59164 dec=153544`。
- SHA-256: `build/pico_skyace.bin` =
  `1c1ef7bc111d9e02228907b1e652557dd7f90aee71904fe0bbd1a3d26d33d0f0`、
  `build/pico_skyace.elf` =
  `849413d7e1daea68ecd2bbe0bc84f85b96a6bec4b1a934639f2a94b93cf6f062`、
  `build/pico_skyace.uf2` =
  `16683d844041423dd69ee08ee8759e6193da3350938c51fece7cc9492ccee89e`。

Wave 1を基準に、後半Waveの追跡開始・旋回・移動・射撃を強め、Wave 5以降は
射撃距離内で攻撃態勢を維持する。18フレームの攻撃予告、敵数上限5、ミサイル・
爆発の固定配列は変更していない。難易度の最終的な操作感は、このUF2を実機で
プレイして確認する。ユーザーから「非常によくなった」との受入報告を受けたため、
Wave攻撃性調整は完了扱いとする。1.0.0最終UF2では同じ挙動を再確認する。

## 攻撃方向表示更新ビルド（2026-09-10）

`INCOMING`警告中に攻撃元の方位テキスト（`FROM ...`）を表示し、照準周囲へ赤い
方向矢印を追加した。画面内の攻撃機でも方向を読み取れ、画面外・背後・上下・斜めは
カメラ空間の方位に対応する。`VERSION`は1.0.0へ変更していない未リリースの開発版である。

- ホストテスト: CTest `1/1` 合格。
- RP2040 Releaseビルド: 新規警告なし。
- サイズ: `text=95676 data=0 bss=59164 dec=154840`。
- SHA-256: `build/pico_skyace.bin` =
  `a94692b5f1ead7488674dfc1aaae3eac8ec4110fc9f591e420dbf5efd88fab48`、
  `build/pico_skyace.elf` =
  `1692a18ca1eec78fe3e9a2ff3b6184c8db19585f810c6d6169d8cdb1823ffb16`、
  `build/pico_skyace.uf2` =
  `66f4bf2dba8a922e526a6aa2983660c95eabb22944fa2f5759d71ae1bd09f8ab`。

## タイトル曲終端変化ビルド（2026-09-11）

A〜Hの各3.2秒フレーズについて、2回目の末尾4音だけを別の着地形へ変更した。
基本TAILとTAIL_ALTの長さはどちらも800msで、リード総時間は`102700ms`のまま
一致することを専用チェックで確認した。これは`VERSION`を1.0.0へ変更していない
開発ビルドである。

- ホストテスト: CTest `1/1` 合格。
- BGMリード総時間チェック: `lead_duration_ms=102700 expected=102700`。
- RP2040 Releaseビルド: 新規警告なし。
- サイズ: `text=95676 data=0 bss=59164 dec=154840`。
- SHA-256: `build/pico_skyace.bin` =
  `277bfb26162c907afe632d67ae87de5fb923db93ecc54a4d5e8774a2e5dc94f1`、
  `build/pico_skyace.elf` =
  `68be97e84ad82839d16974d3d208a3bbb6785ee1fca043b94cba6e8eefde7dbd`、
  `build/pico_skyace.uf2` =
  `b0f71b8fce3708c0c1cc964b24859db9e1404c3119737e9ff2294fd474e5f068`。

## 20分上限負荷（ユーザー報告）

2026-09-10、ユーザーから20分間の上限負荷プレイが問題なく完了した（OK）との
報告を受領した。これは長時間安定性の手動受入記録である。対象UF2のSHA-256、
UARTログ、実施時刻、フレーム時間p95、メモリ／実行時スタック余裕は提供されて
いないため、再現可能な機械計測証跡や33ms性能判定としては扱わない。`VERSION`を
1.0.0へ更新した最終UF2での再確認と、フレーム時間・メモリ／スタック計測は残件である。
