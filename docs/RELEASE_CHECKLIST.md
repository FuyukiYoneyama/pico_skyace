# v1.0.0 release checklist

This is the maintainer's release record for the RP2040 PicoCalc target. It is a
release gate, not a requirement that an end user discover controls without
documentation. User testing is optional feedback; the required evidence is
build, behavior, target hardware, and distribution evidence.

## Scope

The v1.0.0 scope is the current endless-wave Sky Ace game on the standard
RP2040 PicoCalc configuration: title, play, pause, game-over, demo, weapons,
enemy AI, HUD, audio, and optional SD screenshots. Pico 2/RP2350 is outside the
supported target until separately tested.

## Already present in the repository

- [x] MIT license for the project's original source (`LICENSE`).
- [x] Consolidated attribution and fan-made/trademark notice (`NOTICE.md`).
- [x] FatFs R0.14a license retained with the vendored source.
- [x] BGM provenance and distribution record (`docs/BGM_PROVENANCE.md`).
- [x] Build and operation instructions in `README.md`.
- [x] Host logic test target; current result: 1/1 test passed.
- [x] RP2040 Release UF2 build; current artifact is in `build/`.
- [x] Emulator smoke evidence for boot/title/play/pause/audio and a shortened
      real-play demo path; see `docs/validation/2026-09-10-demo/BUILD.md`.
- [x] Production-timing emulator scenarios are now checked in for the complete
      title/demo cycle and game-over timeout (`tests/emulator/`).

## Required before creating the v1.0.0 tag

- [ ] Change `VERSION` and all versioned release text to `1.0.0`.
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
      The strict Title→Demo→Title and GameOver→Title guards use the production
      BIN and pass with no exception, unsupported MMIO, or keyboard drops. The
      GameOver guard is UART-only; a full-LCD GameOver framebuffer run remains
      impractical in the current cycle-accurate model and is not claimed here.
- [x] Exercise the production F5 path with an emulated FAT32 SD card and the
      no-card fallback. The SD model reports zero protocol errors; the no-card
      run explicitly reports detect-high, `no_card`, and mount failure recovery.
- [ ] Verify the final UF2 on an actual standard RP2040 PicoCalc: cold boot,
      LCD, keyboard, simultaneous input, BGM/SFX/engine audio, SD screenshot
      success and no-card recovery, and power-cycle recovery.
- [ ] Record a 20-minute upper-load play run and frame-time/memory evidence;
      the plan's provisional target is a 33 ms 95th-percentile frame. A
      separate two-minute production-BIN stability sample and a non-hardware
      diagnostic p95/stack sample are archived, but neither satisfies the
      20-minute gate or proves the real LCD-transfer frame time.
- [x] Existing title/gameplay images are explicitly labeled as historical; a
      final-firmware capture can replace them for the v1.0.0 release page.
- [ ] Create the `v1.0.0` Git tag and GitHub Release, attaching the UF2 and its
      checksum alongside `CHANGELOG.md`, `NOTICE.md`, and the supported-target
      notes.

## Stop conditions

Do not label an item passed from source inspection alone when it requires a
physical device or a production-timing run. If a target-hardware run is not
available, publish the firmware as a preview/development build instead of
calling it a tested v1.0.0 release.
