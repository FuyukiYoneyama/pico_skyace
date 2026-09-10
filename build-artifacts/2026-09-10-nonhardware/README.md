# v0.9.3 非実機検証記録

実機を使わず、既存の製品BIN `build/pico_skyace.bin` に対して実施した検証の
記録である。対象BINのSHA-256は
`db495f5f2e36a0cd2fa0f4921679a3aac3adcb61861dded8a56eedc91ca467c6`。
この記録ではファームウェアのソース、`VERSION`、既存の製品BINを変更していない。

## 結果

| 経路 | 判定 | 主な確認 |
|---|---|---|
| Strict Title→Demo→Title UART | 合格 | 30,584,627 µsでDemo、60,648,619 µsでTitle。期限差は30,063,992 µs。 |
| Strict GameOver→Title UART | 合格 | 通常のDown+O墜落。GameOver→Title差は10,001,149 µs。 |
| GameOver直接リトライ | 合格 | `MODE GameOver->Play`を確認。再出撃時にWave初期化も確認。 |
| Boot/Play framebuffer | 合格 | 320×320の非黒画素102,400。 |
| Pause→Resume framebuffer | 合格 | ポーズ中の領域安定と復帰後の描画を確認。非黒画素102,400。 |
| 音声操作入力の描画経路 | 合格 | O、←/→、↑/↓、Space入力を含むシナリオが完走。非黒画素102,400。 |
| 武器・標的境界 | 合格 | ミサイル枠飽和、機銃＋スロットル＋旋回の同時入力、継続描画を確認。 |
| SDあり F5 | 合格 | FAT32モデルで `SCREENSHOT done status=ok`。プロトコルエラー0。 |
| SDなし F5 | 合格 | `gpio22=1 present=0`、`SD init status=no_card`、mountエラー後に復帰。 |
| 代表上限負荷 | 合格（2分） | 製品BINで120,000 ms、約30.27 Gcycles、例外・unsupported MMIO・入力dropなし。 |
| 性能・スタック診断 | 計測完了（判定保留） | 診断BINで300フレーム。p95上限264,999 µs、スタック使用944/4096 bytes。高速シンクの仮想値で実機合否には不使用。 |

各ディレクトリには `report.json`、UARTログ、実行シナリオを保存した。画面を伴う
シナリオにはPNGも保存している。SDありの構造化トレースは
`screenshot-sd/sd.trace.json`（1,737イベント、digest
`789ade44a9111488b4009d42ddb9cf9e97b0dbc5ac828c9c360a478ecfb9fed1`）である。

## 実行バックエンドと解釈範囲

framebufferシナリオは公式LCDモデルのクリーンなバックエンド commit
`58e73010636bb1b60fdb1ccace40db29b5bb96cc` を使用した。長時間UARTシナリオと
入力境界シナリオは、LCD画素デコードを省略した一時的な高速PIOシンク
commit `5d365c08191aaa3e490c8ac6d491df55e152ba2a` を使用した。この差分は
[`backend-fast-uart.patch`](backend-fast-uart.patch) に保存しているため、UART専用の
時間・状態遷移検証であり、公式LCD framebufferや通常PIO転送モデルの合格を意味しない。

SDシナリオは同じ高速シンクに、カード未挿入時の物理プルアップ（detect=High）を
明示する診断差分 commit `24bee45f36d9a53cd65cf56522915786c3d95075` を加えた。
[`backend-empty-sd-detect.patch`](backend-empty-sd-detect.patch) に保存している。
この差分もエミュレーター側だけのもので、製品BINには含まれない。

性能診断の詳細は [`perf-diagnostic/README.md`](perf-diagnostic/README.md) にまとめた。

## 後続の実機報告と残る確認

この非実機記録の作成後、ユーザーから現行v0.9.3製品UF2について、コールドブート、
LCD、キーボード、同時入力、BGM・パーカッション・効果音・エンジン音、
Title→Demo→Title、GameOver→Title、SDあり／なしのF5、電源再投入が問題なく、
SDありでは保存できたとの手動確認報告を受領した。このディレクトリ自体にはその
実機の写真・UARTログ・UF2識別情報は含まれない。

残るリリース確認は次のとおり。

- 最終1.0.0 UF2で同じ実機スモーク確認を繰り返す。
- GameOverの10秒経路はUARTで完走したが、公式LCDモデルを含む長時間framebuffer版は
  未完了であり、上記のUART合格を表示・通常PIO転送モデルの合格とは扱わない。
- 2分上限負荷は代表サンプルであり、リリースゲートの20分連続プレイとフレーム時間
  p95測定は未実施。
- `1.0.0`へのバージョン更新、最終ビルド、タグ、リリース作成はこの記録では行っていない。

電圧・波形・信号品質の測定は実施しておらず、今回の公開判定条件にも含めていない。
「通常PIO転送モデル」はエミュレーター内のプロトコル経路を意味する。
