# 作業失敗台帳

## 2026-09-10: threat-direction host build used a nonexistent local build directory

- 対象: 攻撃方向表示変更のホストテスト
- 環境: WSL bash、`pico_skyace` リポジトリ
- 失敗コマンド: `cmake --build build-host-tests -j2` および
  `ctest --test-dir build-host-tests --output-on-failure`
- 終了結果: `build-host-tests` が存在せず、ビルドとCTestは実行されなかった。
- 原因: ホストテスト用の既存ディレクトリを確認せず、READMEの例のパスをそのまま指定した。
- 次回の成功経路: `/tmp` に専用のホストビルドディレクトリを作り、`cmake -S tests -B <tmp> -G Ninja`
  で構成してからビルドとCTestを実行する。
- 再試行禁止: `build-host-tests` が存在しない状態で同じコマンドを再実行しない。

## 2026-09-10: threat-direction firmware build used a helper before its definition

- 対象: 攻撃方向インジケーターのRP2040 Releaseビルド
- 環境: WSL bash、既存の`build/`ディレクトリ
- 失敗コマンド: `cmake --build build -j2`
- 終了結果: `game.cpp`で`draw_attack_direction_indicator()`から
  `draw_guidance_arrow()`を参照した時点では、その定義が後ろにあり、コンパイルが終了コード1で失敗した。
- 原因: 新しい描画ヘルパーを既存ヘルパーの定義前へ挿入したが、前方宣言を追加していなかった。
- 次回の成功経路: `draw_guidance_arrow()`の前方宣言を追加してから、同じ`build/`で再ビルドする。
- 再試行禁止: 前方宣言を追加せず、同じ順序のソースでビルドを再実行しない。

## 2026-09-10: threat-direction official LCD smoke exceeded the command time window

- 対象: 攻撃方向表示変更後の`picocalc_emu`公式LCD付き短時間起動確認
- 環境: `picocalc-run`、`pio-rgb565`、`boot_play_smoke.json`
- 失敗コマンド: 製品BINを公式LCDモデルで5,000,000,000 cycles、30秒の実行枠で起動した確認
- 終了結果: 実行枠内にrunnerのレポート／スナップショットが生成されず、確認を完了できなかった。
- 原因: 公式LCDの画素転送モデルが初期化・描画に時間を要し、今回の短い実行枠を超えた。
- 次回の成功経路: 同じ長時間の公式LCD経路は再試行せず、ホストテストとRP2040ビルドを確認根拠にする。必要な実機表示確認は実機テスト時に行う。
- 再試行禁止: 同じcycles・公式LCDモデル・短い実行枠の組み合わせを繰り返さない。

## 2026-09-11: BGM host test reused a cleaned temporary build directory

- 対象: BGMフレーズ変更後のホストテスト再確認
- 環境: WSL bash、`/tmp/pico-skyace-threat-host.SAXwMa`
- 失敗コマンド: `ctest --test-dir /tmp/pico-skyace-threat-host.SAXwMa --output-on-failure`
- 終了結果: 一時ディレクトリが存在せず、CTestは実行されなかった。
- 原因: 前回の一時ホストビルドが保持される前提で、存在確認なしに再利用した。
- 次回の成功経路: 新しい明示的な`mktemp -d /tmp/...`へホストテストを構成し直してから実行する。
- 再試行禁止: 消去済みの`/tmp/pico-skyace-threat-host.SAXwMa`を同じパスで再利用しない。
