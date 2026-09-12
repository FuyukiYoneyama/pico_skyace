# v1.0.0 release checklist

This is the maintainer's release record for the RP2040 PicoCalc target. It is a
release gate, not a requirement that an end user discover controls without
documentation. User testing is optional feedback; the required evidence is
build, behavior, target hardware, and distribution evidence.

## Scope

The v1.0.0 scope is the current endless-wave Sky Ace game on the standard
RP2040 PicoCalc configuration: title, play, pause, game-over, demo, weapons,
enemy AI, HUD, audio, SD screenshots, and optional SD-backed high scores. Pico 2/RP2350 is outside the
supported target until separately tested.

## Already present in the repository

- [x] MIT license for the project's original source (`LICENSE`).
- [x] Consolidated attribution and fan-made/trademark notice (`NOTICE.md`).
- [x] FatFs R0.14a license retained with the vendored source.
- [x] BGM provenance and distribution record (`docs/BGM_PROVENANCE.md`).
- [x] Build and operation instructions in `README.md`.
- [x] High-score display and SD-backed persistence with a session-only fallback
      (`SKYHI_A.DAT` / `SKYHI_B.DAT`) are implemented and documented.
- [x] Host logic test target; current result: 1/1 test passed.
- [x] Product ELF Flash/RAM size and a non-hardware diagnostic stack high-water
      sample are recorded under
      [`build-artifacts/2026-09-10-nonhardware/perf-diagnostic/`](../build-artifacts/2026-09-10-nonhardware/perf-diagnostic/).
      A diagnostic-only five-minute Demo logger now emits machine-readable
      `PERF_DEMO` boot/session boundary lines and one `event=done` line; its
      procedure and schema are in [`docs/PERF_DIAGNOSTICS.md`](PERF_DIAGNOSTICS.md).
      The diagnostic frame-time value is explicitly not used as the real-device
      33 ms performance verdict.
- [x] RP2040 Release UF2 build; current artifact is in `build/`.
- [x] Product UART boot identity is emitted as a flushed `PICO_SKYACE_BOOT`
      line with app/version/build/watchdog/clock/UART fields; the display
      initialization boundary repeats the app/version/build identity, and a
      500 ms CH340/host-COM settle interval precedes the first identity line on
      cold power-on. The implementation and v0.9.18 emulator evidence are in
      [`v0.9.18 BUILD.md`](validation/2026-09-12-v0.9.18/BUILD.md).
- [x] Emulator smoke evidence for boot/title/play/pause/audio and a shortened
      real-play demo path; see `docs/validation/2026-09-10-demo/BUILD.md`.
- [x] Production-timing emulator scenarios are now checked in for the complete
      title/demo cycle and game-over timeout (`tests/emulator/`).
- [x] README discloses that the product configuration sets RP2040 to 250 MHz,
      above the 133 MHz maximum in Raspberry Pi's published RP2040
      specifications, and explains the resulting individual-unit/power/thermal
      caveat with a link to the official specification.

- [ ] For every `/tmp`-based build or measurement, preserve the raw log, report,
      input/scenario, reproduction inputs, used BIN/UF2, and command/configuration
      in a persistent artifact directory; verify a SHA-256 manifest before any
      cleanup. An unclassified temporary file is not eligible for deletion.

The current verification build is `0.9.18`. It is intentionally kept below
`1.0.0` while diagnostics and emulator checks are run. The final `1.0.0`
version bump is a one-time bookkeeping step immediately before the clean
release build; no diagnostic or emulator-only changes are made under `1.0.0`.

## Required before creating the v1.0.0 tag

- [ ] Change `VERSION` and all current versioned release text to `1.0.0` as the
      final no-source-change release step.
- [ ] Rebuild from the final tagged source with the documented SDK/toolchain,
      record size information and the UF2 SHA-256.
- [x] Run the production binary (without shortened demo timing) through the
      complete title/demo/title and game-over/title UART scenarios; archived
      reports are in
      [`docs/validation/2026-09-10-demo/BUILD.md`](validation/2026-09-10-demo/BUILD.md).
      The title/demo run includes the official LCD model. The game-over run is
      UART-only and uses a documented fast PIO sink because the full LCD model
      makes the deliberate 349-frame crash path impractical; its report is not
      a full framebuffer/normal-PIO-transfer-model pass.
- [x] Run the strict 33-second UART guards and the available framebuffer
      variants, plus direct retry, pause/resume, and weapon/target edge cases;
      reports are archived under
      [`build-artifacts/2026-09-10-nonhardware/`](../build-artifacts/2026-09-10-nonhardware/).
      The strict Title->Demo->Title and GameOver->Title guards use the production
      BIN and pass with no exception, unsupported MMIO, or keyboard drops. The
      GameOver guard is UART-only; a full-LCD GameOver framebuffer run remains
      impractical in the current cycle-accurate model and is not claimed here.
- [x] Exercise the production F5 path with an emulated FAT32 SD card and the
      no-card fallback. The SD model reports zero protocol errors; the no-card
      run explicitly reports detect-high, `no_card`, and mount failure recovery.
- [x] User-reported hardware smoke test of the current v0.9.3 product UF2:
      cold boot, LCD output, keyboard input, simultaneous movement/weapon
      input, BGM (including percussion), SFX, engine sound, Title->Demo->Title,
      GameOver->Title, SD-present/absent F5 handling, and power-cycle recovery
      all passed. This is a manual report; the UF2 hash, photos/UART capture,
      and exact test timestamp were not supplied.
- [x] User-reported hardware smoke test of the current build on an actual
      standard RP2040 PicoCalc passed: cold boot, LCD, keyboard, simultaneous
      input, BGM/SFX/engine audio, SD save/no-card recovery, power-cycle
      recovery, revised Wave aggression, Wave 4 demo start, and sun color.
      The reported UF2 version, SHA-256, and exact test timestamp were not
      supplied; before tagging `1.0.0`, associate this acceptance with the final
      UF2 (or repeat the smoke test after the final build).
- [x] Cold-power UART identity fix was confirmed with the v0.9.18 UF2: the
      preserved `20260912_170847.log` contains three boot sequences, each
      starting with `PICO_SKYACE_BOOT ... version=0.9.18`, with
      `WATCHDOG_CAUSED_REBOOT=0` and successful LCD/keyboard/SD/high-score
      initialization. Its SHA-256 is recorded in the v0.9.18 build record.
- [ ] After the final `1.0.0` build, associate that exact UF2 SHA-256 and build
      ID with a short real-device smoke log. The v0.9.18 hardware log proves
      the cold-boot UART fix, but does not by itself identify the future
      `1.0.0` UF2.
- [x] User-reported 20-minute upper-load play run completed without issue on
      2026-09-10. This is a manual stability-acceptance record; the tested
      UF2 hash, UART log, and exact run metadata were not supplied.
- [x] Record frame-time and memory evidence on the target hardware. The
      v0.9.15 diagnostic log `20260912_135758.log` completed its 300-second
      window from one boot: Demo p95 upper bound `64.999 ms`, average
      `60.409 ms`, maximum `64.621 ms`, stack `1208/4096 bytes`, free `2888
      bytes`, and `stack_overflow=0`. The boot record reports
      `version=0.9.15 watchdog_reboot=0`; `demo_resets=0` and the single
      `event=done` establish a complete window. Additional boot records appear
      only after `event=done` and are outside this measurement. The provisional
      33 ms target is not met, but it is not a v1.0 functional requirement: at
      the stable 31.25 MHz link, sending a full 320x320 RGB565 frame has a
      52.4 ms wire-time floor before software overhead. The measured value is
      recorded as the current hardware baseline; no 30 fps claim is made.
- [x] README gameplay image was replaced with the supplied combat capture; a
      final-firmware capture may replace it again for the v1.0.0 release page.
- [ ] Create the `v1.0.0` Git tag and GitHub Release, attaching the UF2 and its
      checksum alongside `CHANGELOG.md`, `NOTICE.md`, and the supported-target
      notes.

## Stop conditions

Do not label an item passed from source inspection alone when it requires a
physical device or a production-timing run. If a target-hardware run is not
available, publish the firmware as a preview/development build instead of
calling it a tested v1.0.0 release.
