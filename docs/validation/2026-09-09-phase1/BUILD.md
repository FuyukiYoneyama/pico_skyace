# Phase 1 ビルド記録

実施日: 2026-09-09

対象: `pico_skyace`（作業ツリー、基準コミット `4c599e6` からの未コミット変更を含む）

## 成果物

- [pico_skyace.uf2](../../../build-artifacts/2026-09-09-phase1/pico_skyace.uf2)
  - SHA-256: `e44924722fbb48beb966ea783c99239a7d609980bf3f2b79928edf149710220b`
- [pico_skyace.elf](../../../build-artifacts/2026-09-09-phase1/pico_skyace.elf)
  - SHA-256: `68547ba5a77d1b97e18f189882628f093edc1d375eb9ee2a34798ce8f4b7ea86`
- [pico_skyace.bin](../../../build-artifacts/2026-09-09-phase1/pico_skyace.bin)
  - SHA-256: `468df4c21be0532c344b30768d3d0120bbfcbebf09ac2b8f73d6f730e54563ae`

`build-artifacts/` は `.gitignore` 対象のローカル保管場所であり、UF2/ELFはGitへ追加しない。

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

結果: 成功。`text=90300`, `data=0`, `bss=59132`, `dec=149432`。新規警告なし。
`picotool info`でもUF2のfamily IDが`rp2040`であることを確認した。

エミュレーター診断実行（登録targetではない新規BINの通し確認）:

- シナリオ: [`boot_play_smoke.json`](../../../tests/emulator/boot_play_smoke.json)
- 結果: `pass`（起動→Enter→Play→描画、1.485秒の仮想時間）
- UART: 458 bytes、キーボードdrop: 0、exception: なし、unsupported MMIO: なし
- [report.json](../../../build-artifacts/2026-09-09-phase1/emulator-boot-play-smoke/report.json)
- [uart.log](../../../build-artifacts/2026-09-09-phase1/emulator-boot-play-smoke/uart.log)
- [play.png](../../../build-artifacts/2026-09-09-phase1/emulator-boot-play-smoke/play.png)

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
