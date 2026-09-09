# Phase 1 ビルド記録

> これは v0.9.2 の履歴記録です。デモを実プレイ化し、タイトル曲を再調整した
> 最新版 v0.9.3 の成果物と検証結果は、[こちらの記録](../2026-09-10-demo/BUILD.md)
> を参照してください。

実施日: 2026-09-10（Phase 1継続検証）

対象: `pico_skyace` v0.9.2（作業ツリー、基準コミット `4c599e6` からの未コミット変更を含む）

## 成果物

旧版のUF2/ELF/BINは再生成可能な履歴用バイナリとして整理済み。検証ログと画面
キャプチャは残してあり、必要なバイナリは現行版のビルド手順から再生成する。

## 実行した検証

ホストロジックテスト:

```sh
cmake -S tests -B <tmp-host-build> -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build <tmp-host-build>
ctest --test-dir <tmp-host-build> --output-on-failure
```

結果: 1/1合格。

RP2040ビルド:

```sh
PICO_SDK_PATH=/home/fuyuki/pico/pico-sdk \
  cmake -S . -B <tmp-pico-build> -G Ninja
cmake --build <tmp-pico-build>
```

結果: 成功。`text=92916`, `data=0`, `bss=59160`, `dec=152076`。新規警告なし。
`picotool info`でもUF2のfamily IDが`rp2040`であることを確認した。

エミュレーター診断実行（登録targetではない新規BINの通し確認）:

- シナリオ: [`boot_play_smoke.json`](../../../tests/emulator/boot_play_smoke.json)
- 結果: `pass`（起動→Enter→Play→描画、1.485秒の仮想時間）
- UART: 458 bytes、キーボードdrop: 0、exception: なし、unsupported MMIO: なし
- [report.json](../../../build-artifacts/2026-09-09-phase1/emulator-boot-play-smoke/report.json)
- [uart.log](../../../build-artifacts/2026-09-09-phase1/emulator-boot-play-smoke/uart.log)
- [play.png](../../../build-artifacts/2026-09-09-phase1/emulator-boot-play-smoke/play.png)

今回の画面遷移・自動攻撃・バージョン表示実装（v0.9.2）についても同じ新規BINで
起動→PlayとPause→Resumeを再実行し、両シナリオとも`verdict.status=pass`、
`scenario.status=pass`、exceptionなし、unsupported MMIOなし、キーボードdrop 0を
確認した。UART起動行には`pico_skyace version 0.9.2`が出力される。Title 30秒後の
Demoと、デモ内での敵機への自動攻撃は長時間シナリオで通し確認した。Demo 30秒後の
Title、GameOver 10秒後のTitleはコード上のフレームタイマー（33ms周期）で実装済みだが、
エミュレーターでの長時間経路は未実施。

タイトル表示確認:

- シナリオ: [`title_version_smoke.json`](../../../tests/emulator/title_version_smoke.json)
- 結果: `pass`（タイトル描画と`pico_skyace version 0.9.2`の起動ログ）
- [report.json](../../../build-artifacts/2026-09-09-phase1/emulator-title-version-smoke/report.json)
- [title-version.png](../../../build-artifacts/2026-09-09-phase1/emulator-title-version-smoke/title-version.png)

タイトル→デモ・自動攻撃シナリオ:

- シナリオ: [`title_demo_attack_smoke.json`](../../../tests/emulator/title_demo_attack_smoke.json)
- 結果: `pass`（Title→Demoを30.44秒で検出し、500ms後の自動攻撃画面を描画）
- UART: 462 bytes、キーボードdrop: 0、exception: なし、unsupported MMIO: なし
- [report.json](../../../build-artifacts/2026-09-09-phase1/emulator-title-demo-attack-smoke/report.json)
- [uart.log](../../../build-artifacts/2026-09-09-phase1/emulator-title-demo-attack-smoke/uart.log)
- [demo-attack.png](../../../build-artifacts/2026-09-09-phase1/emulator-title-demo-attack-smoke/demo-attack.png)

Pause／再開シナリオ:

- シナリオ: [`pause_resume_smoke.json`](../../../tests/emulator/pause_resume_smoke.json)
- 結果: `pass`（`Play->Pause`、ポーズ中の上部画面安定、`Pause->Play`）
- UART: 512 bytes、キーボードdrop: 0、未処理イベント: 0
- [report.json](../../../build-artifacts/2026-09-09-phase1/emulator-pause-resume-smoke/report.json)
- [uart.log](../../../build-artifacts/2026-09-09-phase1/emulator-pause-resume-smoke/uart.log)

音声操作入力シナリオ（音量・音色の最終評価は実機）:

- シナリオ: [`audio_controls_smoke.json`](../../../tests/emulator/audio_controls_smoke.json)
- 結果: `pass`（O加速、右バンク、機首上げの入力後も描画継続）
- UART: 458 bytes、キーボードdrop: 0、exception: なし、unsupported MMIO: なし
- [report.json](../../../build-artifacts/2026-09-09-phase1/emulator-audio-controls-smoke/report.json)
- [uart.log](../../../build-artifacts/2026-09-09-phase1/emulator-audio-controls-smoke/uart.log)

## 未実施

エミュレーターでの長時間・全操作（再出撃、攻撃予告回避、結果固定）の確認、
正式な登録targetとしての合否判定、実機への書き込み、UARTログ、F5画像、操作感、
フレーム時間、20分連続安定性、実機スピーカーでのパーカッションの音圧・
エンジン音のスロットル追従・左右定位は未確認。
