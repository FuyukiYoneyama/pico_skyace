# v0.9.13 ハイスコア・README画像更新ビルド記録

実施日: 2026-09-12

## 変更

- タイトル画面とゲームオーバー画面に `HI-SCORE` を表示するようにした。
- ゲームオーバー確定時に新記録だけを更新し、SDカードのルートへ
  `SKYHI_A.DAT`／`SKYHI_B.DAT` のチェックサム付き2スロットで保存する。
  SDなし・マウント失敗・書き込み失敗でもセッション内の記録を保持する。
- READMEのゲームプレイ画像を、ユーザー提供の
  `/home/fuyuki/pico_dvl/codex/log/skyace_0020.BMP`（160×160）を2倍化した
  `docs/images/gameplay.jpg`（320×320）へ差し替えた。
- 挙動変更に合わせて `VERSION` を `0.9.13` へ更新した。

## 成果物

- [pico_skyace.uf2](../../../build/pico_skyace.uf2)
  - SHA-256: `7a2f59d1843835dfbd51998293c6be4279e56bb1ef018326799b7d992db95cac`
- `pico_skyace.bin`（エミュレーター入力）
  - SHA-256: `84926c78fbb76e39303649ffe58f9eb28b9c7a62b91950b7b940b9f1089d034e`
- `pico_skyace.elf`（`arm-none-eabi-size`）:
  `text=101524 data=0 bss=59764 dec=161288 hex=27608`
- `docs/images/gameplay.jpg`
  - SHA-256: `39ad7baf8e5b779bdda11bc283d072c635cf1567017fe15eae8c21c9104664e6`

## 検証

RP2040 Releaseビルド:

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

警告なしで完了した。ホストロジックテストも1/1合格:

```text
cmake -S tests -B /tmp/pico-skyace-host-0.9.13 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/pico-skyace-host-0.9.13 -j2
ctest --test-dir /tmp/pico-skyace-host-0.9.13 --output-on-failure
```

公式LCDエミュレーターにFAT32 SDモデルを接続し、タイトル表示とハイスコア読み込み
経路を確認した。`pico_skyace version 0.9.13`、`HIGHSCORE load status=empty score=0`、
例外なし、unsupported MMIOなし、キーボードdropなし、非黒画素102400で合格した。
タイトル画像には `VERSION 0.9.13` と `HI-SCORE 000000` が描画されている。

- [タイトルシナリオ](../../../tests/emulator/title_version_smoke.json) /
  [report.json](../../../build-artifacts/2026-09-12-v0.9.13/emulator-title/report.json)
- [title-version.png](../../../build-artifacts/2026-09-12-v0.9.13/emulator-title/snapshots/title-version.png)
- [README掲載の戦闘中画像](../../images/gameplay.jpg)

## 未実施・適用範囲

- ハイスコアの新記録発生からSD再起動後の再読込までの実機一連確認は未実施。
  SDカードがない場合のゲーム継続は既存のF5 no-card経路と同じフォールバック方針で、
  新規保存処理の失敗時もRAM値を失わない。
- 実機での音量・LCD電気特性、最終 `1.0.0` 更新、最終ビルド、タグ／リリース作成は
  これまでどおり保留する。
