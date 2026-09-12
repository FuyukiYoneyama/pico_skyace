# 実機性能ログの取り方

製品ビルドには計測コードを入れず、`PICO_SKYACE_PERF_DIAGNOSTICS=ON` の
診断ビルドだけで、デモを放置した集計ログを出す。診断ビルドの出力は
測定用であり、配布用UF2には使わない。

## 計測内容

- 最初の `Title->Demo` から指定時間（既定5分）のゲームループを集計する。
- タイトル滞在フレームは `non_demo_frames` に分け、`p95_upper_us` は実プレイと同じ
  `update_demo()`／`update_play()`／`render_demo()`／`present_scaled2x()` を通った
  Demoフレームだけから求める。
- `present_scaled2x()` 完了までを測るため、実LCDへのblocking PIO転送を含む。
  `sleep_until()` の待機時間は含めない。
- スタックは起動時にパターンで塗り、終了時に高水位を走査する。`stack_free_bytes`
  と `stack_overflow` を記録する。

## ログの意味

診断行はすべて `PERF_DEMO` で始まり、空白区切りの `key=value` とする。毎フレームの
ログは出さず、集計区間の境界と完了結果だけを出力する。

| 行 | 発生回数 | 意味 |
|---|---:|---|
| `event=boot` | 起動ごとに1回 | 初期化完了後の起動境界。`watchdog_reboot` は直前リセットの理由を示す。 |
| `event=start` | 起動ごとに最大1回 | 最初の `Title->Demo` で計測窓を開始した。まだ結果ではない。起動先頭を取り逃しても `boot_uptime_us` と `watchdog_reboot` が残る。 |
| `event=session_start` | Demo開始ごとに1回（最初を除く） | 30秒のTitle帰還後に次のDemoセッションを開始した。 |
| `event=session_end` | Demo終了ごとに1回 | `reason=timeout` など、Demoセッションが正常にTitleへ戻った境界。 |
| `event=done` | 起動ごとに最大1回 | 計測窓が終了し、p95・実行時スタック項目を確定した。これが集計対象の完了行。 |

`event=start` があって `event=done` がないログは途中終了として扱い、数値を合格判定に使わない。
`event=boot` や `event=start` が複数回現れる場合は、起動が複数回あったことを示す。
`schema=1` は
このキー名と単位の組を示す。`window_frames` は窓内の全ゲームループ、
`demo_frames` はそのうちDemo描画を完了したフレーム、`non_demo_frames` はTitleなどの
除外フレームである。したがって、p95は `demo_frames` だけから計算する。
`stack_used_bytes` は起動時のスタックと保護ガードを含む保守的な高水位上限、
`stack_free_bytes` は塗りつぶし領域で未使用だった下側の空きであり、両者は
「実際に書き込まれたバイト数」と同義ではない。

## 保存と後片付け（必須）

`/tmp` は作業中のステージング領域であり、管理者による予告なしの削除や
マシン再起動で失われる。ログ、レポート、画面キャプチャ、計測に使ったBIN／UF2、
生成スクリプト、入力シナリオ、CMakeキャッシュ、実行オプションを`/tmp`だけに置かない。
計測または検証が終わった直後に、リポジトリの
`build-artifacts/<date>-<version>/<run>/`またはユーザーが指定した永続ディレクトリへ
コピーする。保存先には少なくとも次を含める。

- 生UARTログ（加工前）と採用した集計行
- runnerのJSONレポート、シナリオ、スナップショット
- 実行したBIN／UF2とSHA-256
- ビルド設定、ソースのコミットID、コマンドライン、必要な生成スクリプト

保存後に`sha256sum`でマニフェストを作り、別のシェルから全ファイルを読み出して
`sha256sum -c`が通ることを確認する。この確認が終わるまで`rm`、`rm -r`、一時ディレクトリの
再利用、または「不要そう」という理由での削除を行わない。未分類のファイルは削除せず、
保存先と採否を決めてから整理する。

## 診断UF2の作成

実機用（5分窓）は、製品ビルドとは別のディレクトリで作る。

```sh
diag=/tmp/pico-skyace-perf-hardware
cmake -S . -B "$diag" -G Ninja \
  -DPICO_SDK_PATH=/home/fuyuki/pico/pico-sdk \
  -DCMAKE_BUILD_TYPE=Release \
  -DPICO_SKYACE_PERF_DIAGNOSTICS=ON \
  -DPICO_SKYACE_PERF_WINDOW_MS=300000
cmake --build "$diag" -j2
```

生成された `"$diag/pico_skyace.uf2"` を一時的に実機へ書き込む。製品の
`build/pico_skyace.uf2` は上書きしない。実機からログを回収したら、UF2、ログ、
ビルド設定、実行時刻、`VERSION`、SHA-256を上記の永続保存先へ先にコピーし、
マニフェスト検証が終わってから`$diag`を削除する。

## 実機での無人取得

1. UARTロガーを115200bps、8N1で起動する。UART0（GP0/GP1）を使う場合は
   3.3V対応USB-UARTとGNDを接続し、RS-232や5V信号は直結しない。
2. ロガーを開始した状態で実機をリセット／電源投入する。
3. 何も操作せず待つ。製品設定では約30秒後にDemoへ入り、以後30秒ごとの
   Title帰還を挟みながら、Demoフレームだけが5分窓へ累積される。
4. `event=done` が出るまで待つ。これが集計完了の機械判定点である。途中で
   `event=boot` が再度出た場合は再起動なので、その窓は未完了として扱う。

```text
PERF_DEMO event=boot schema=1 version=0.9.16 uptime_us=... watchdog_reboot=0
PERF_DEMO event=start schema=1 version=0.9.16 session=1 boot_uptime_us=... watchdog_reboot=0 window_us=300000000 target_p95_us=33000
PERF_DEMO event=session_end schema=1 version=0.9.16 session=1 uptime_us=... reason=Demo->Title(timeout)
PERF_DEMO event=session_start schema=1 version=0.9.16 session=2 uptime_us=...
PERF_DEMO event=done schema=1 version=0.9.16 boot_uptime_us=... watchdog_reboot=0 window_us=300000000 elapsed_us=... window_frames=... demo_frames=... non_demo_frames=... demo_sessions=... demo_resets=... min_us=... avg_us=... p95_upper_us=... max_us=... target_p95_us=33000 timing_within_target=... stack_bytes=4096 stack_used_bytes=... stack_free_bytes=... stack_first_dirty=... stack_fill_end=... stack_guard_bytes=256 stack_overflow=0
```

行は空白区切りの `key=value` 形式なので、ログから `event=done` の行を抜き出して
そのまま集計できる。`p95_upper_us` は1msビンの上限値で、暫定目標は33000以下。
`stack_overflow=0` と `stack_free_bytes` の実測値も記録する。Demo中に死亡した場合は
`demo_resets` に回数が出るため、無人実行の展開も追跡できる。

完了行だけを抽出する最小コマンドは次のとおり。

```sh
awk '/^PERF_DEMO event=done / { line = $0 } END { if (line == "") exit 1; print line }' uart.log
```

## Flash/RAMの記録

スタック計測用の診断UF2ではなく、同じソースの計測なし製品ビルドに対して実行する。

```sh
arm-none-eabi-size build/pico_skyace.elf
arm-none-eabi-size -A build/pico_skyace.elf
sha256sum build/pico_skyace.uf2 build/pico_skyace.elf
```

診断コードのヒストグラムとスタック走査が追加されたサイズは、製品のFlash/RAM値として
扱わない。測定後は必ず計測なしの最終製品UF2へ戻す。

## 適用範囲

このログは、デモの描画・敵AI・武器・衝突・BGM割り込みを無人で確認するためのもの。
F5のSD書き込みや手動キー入力の瞬間的な負荷は含めない。必要なら別経路として測定する。
既存のエミュレーター診断値はLCD転送を省略した仮想値なので、実機の合否証跡には使わない。
