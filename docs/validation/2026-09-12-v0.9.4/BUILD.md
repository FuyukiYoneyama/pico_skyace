# v0.9.4 Wave初動・デモ開始・太陽色調整 ビルド記録

実施日: 2026-09-12

## 変更

- Wave開始直後は敵を遠巻きの巡航状態にする。
- 残敵がWave開始時の半分以下、またはWave経過が20秒を超えた時点で、攻撃フェーズへ
  切り替える。
- 攻撃フェーズ中にミサイル回避を終えた敵も、遠巻き巡航へ戻らず追跡へ復帰する。
- デモはWAVE 4から開始する。
- 背景の太陽を白色から暖色系の黄色へ変更する。
- `VERSION`を`0.9.4`へ更新した。

## 成果物

- [pico_skyace.uf2](../../../build/pico_skyace.uf2)
  - SHA-256: `6c19dd066b2e576360eafacbfb6ba1a9adcc60814686a8f9465528f68868508f`
- `pico_skyace.elf`（`arm-none-eabi-size`）:
  `text=98292 data=0 bss=59184 dec=157476`

## 検証

ホストロジックテスト:

```text
cmake -S tests -B /tmp/pico-skyace-host-0.9.4 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/pico-skyace-host-0.9.4 -j2
ctest --test-dir /tmp/pico-skyace-host-0.9.4 --output-on-failure
```

結果: 1/1合格。残敵数・20秒境界（600フレームは未発動、601フレームで発動）・
WAVE開始時の遠巻き条件を`game_rules`のホストテストで確認した。

RP2040 Releaseビルドは警告なしで完了した。CMakeは`VERSION`を読み込み、タイトル画面
表示と起動UARTへ`0.9.4`を渡す。

タイトル表示の公式LCDエミュレーター確認:

- シナリオ: [`title_version_smoke.json`](../../../tests/emulator/title_version_smoke.json)
- 結果: `pass`、`scenario_done`、exceptionなし、unsupported MMIOなし、キーボードdrop 0
- UARTで`pico_skyace version 0.9.4`を確認（現行UF2から再生成したBINのSHA-256:
  `b7fd4df85ac110cfa59160f3f667113fe46f7d2c1dc415b311d1398f7db5ac39`）
- [report.json](../../../build-artifacts/2026-09-12-v0.9.4/emulator-title/report.json)
- [uart.log](../../../build-artifacts/2026-09-12-v0.9.4/emulator-title/uart.log)
- [title-version.png](../../../build-artifacts/2026-09-12-v0.9.4/emulator-title/snapshots/title-version.png)

デモ開始WAVE 4のUART条件は、[`title_demo_attack_smoke.json`](../../../tests/emulator/title_demo_attack_smoke.json)
へ回帰チェックとして追加した。製品タイミングのLCD付きデモ30秒経路は、この記録では
完走結果を取得していないため、合格とは扱わない。WAVE 4開始と攻撃条件の判定ロジックは
ホストテストおよびソース実装で確認している。

## 未実施

- 実機でのWAVE 4デモ、攻撃開始タイミング、太陽色の視認確認
- 最終`1.0.0`更新、最終ビルド、タグ／リリース作成
