# 非実機の性能・メモリ診断

製品BINを変更せず、`PICO_SKYACE_PERF_DIAGNOSTICS=1`を追加した一時診断ビルドで
取得した記録である。`VERSION`は0.9.3のまま、診断BINとUF2はGit管理対象にしていない。
診断用ソース差分は [`perf-diagnostics.patch`](perf-diagnostics.patch) に保存した。

## 静的サイズ

製品ELF（`build/pico_skyace.elf`）:

- `arm-none-eabi-size`: `text=93908`, `data=0`, `bss=59164`, `dec=153072`
- ELF SHA-256: `0b9d0b7416fd29eb8fd6e1c70fe8f3c88c2334ef205f3e99adc203894d666e32`

診断ELF（計測コードと300要素ヒストグラムを含む）:

- `text=96612`, `data=0`, `bss=59168`, `dec=155780`
- ELF SHA-256: `6850194045f2515384a43e1edcdd3e60eed8da1715d8c70616b7668858c7ea7c`

セクション別の内訳は `product-size.sysv.txt` と `diagnostic-size.sysv.txt`、比較は
`size-summary.txt`を参照する。診断コードの追加分は製品BINには含まれない。

## フレーム時間サンプル

300フレームを、ループ先頭から`present_scaled2x()`完了まで計測した。待機の
`sleep_until()`は除外し、入力、ゲーム更新、描画、PIOへの送出、音声割り込みを含む。

- min: `183502 µs`
- average: `247292 µs`
- p95 upper bound: `264999 µs`（1msビン）
- max: `268438 µs`

この実行は`board=none`の高速PIOシンク（backend commit
`24bee45f36d9a53cd65cf56522915786c3d95075`）であり、LCDパネル画素デコードを省略している。
そのため、数値はエミュレーターの仮想タイマーと診断バックエンドの比較値であって、
実機の33ms性能判定には使えない。公式LCDモデルでは転送モデルのエミュレーションが
非常に遅く、同じサンプルを現実的なサイクル予算で完走できなかった。33msの合否と
実LCD転送込みのp95は、実機または別途校正された性能モデルが必要である。

## スタック

RP2040の4KiB `__StackBottom=0x20041000`〜`__StackTop=0x20042000`を起動時に
パターンで塗り、300フレーム後に高水位を調べた。

- first dirty: `0x20041c50`
- 使用量: `944 bytes`
- 未使用: `3152 bytes`

これは診断コード自身の呼出しも含むエミュレーター上の観測値である。静的な最大
関数フレームは`render_entities()`の368 bytes（`static-stack-usage.txt`）だった。
割り込みやSD書込を含む別経路の実機スタック余裕を証明するものではない。

## 実行証跡

- [`report.json`](report.json): シナリオ／例外／unsupported MMIO／入力drop
- [`uart.log`](uart.log): `PERF`と`STACK`の実測行
- [`host-timing.json`](host-timing.json): エミュレーター実行条件
- [`scenario.json`](scenario.json): 300フレーム診断シナリオ
