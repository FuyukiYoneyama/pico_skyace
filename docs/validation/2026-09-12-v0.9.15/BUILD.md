# v0.9.15 診断ログ版管理ビルド記録

実施日: 2026-09-12

## 変更

- PicoCalc向けソースの診断起動・Demoセッションログ追加を `VERSION=0.9.15`
  として版管理した。
- v0.9.15 の診断UF2は、起動時に `PERF_DEMO event=boot`、計測完了時に
  `PERF_DEMO event=done` を出力する。

## 成果物

- 製品UF2: `build/pico_skyace.uf2`
  - SHA-256: `9e13b9db517816ffcee5b00d8802d69ce0f25c5b8ad22a9b9fa463ec49435289`
  - 診断機能: OFF
- 診断UF2: `/tmp/pico-skyace-perf-hardware/pico_skyace.uf2`
  - SHA-256: `e0e5d21240c8144590c713bfed820be07dd5155d87f7655bcb858e6929d34218`
  - 診断機能: ON、計測窓300秒
- 製品ELFサイズ: `text=101524 data=0 bss=59764 dec=161288`
- 診断ELFサイズ: `text=105388 data=0 bss=59768 dec=165156`

## 実機ログ

入力ログ: `/home/fuyuki/pico_dvl/codex/log/20260912_135758.log`

最初の起動から `event=done` までを1回の計測窓として採用した。

```text
PERF_DEMO event=boot schema=1 version=0.9.15 uptime_us=848317 watchdog_reboot=0
PERF_DEMO event=start schema=1 version=0.9.15 session=1 boot_uptime_us=30868034 watchdog_reboot=0 window_us=300000000 target_p95_us=33000
PERF_DEMO event=done schema=1 version=0.9.15 boot_uptime_us=30868034 watchdog_reboot=0 window_us=300000000 elapsed_us=300010913 window_frames=5133 demo_frames=2480 non_demo_frames=2653 demo_sessions=5 demo_resets=0 min_us=56308 avg_us=60409 p95_upper_us=64999 max_us=64621 target_p95_us=33000 timing_within_target=0 stack_bytes=4096 stack_used_bytes=1208 stack_free_bytes=2888 stack_overflow=0
```

判定:

- 版番号: `0.9.15` で一致。
- 電源再投入後の起動境界: `watchdog_reboot=0`。
- 5分窓: 完了（`elapsed_us=300010913`、約300.011秒）。
- Demoセッション: 5回、再出撃リセット0回。
- 33ms暫定目標: 未達（`timing_within_target=0`）。現行31.25MHz・全画面転送の
  実測ベースラインとして扱い、v1.0の機能ゲートにはしない。
- スタック: 空き2,888バイト、オーバーフローなし。

`event=done`の後に追加の起動ログがあるが、完了後のため上記計測窓には影響しない。
