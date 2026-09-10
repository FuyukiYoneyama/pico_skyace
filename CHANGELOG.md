# Changelog

This file records user-visible changes. The exact firmware distributed for a
release is identified by its versioned Git tag and the SHA-256 recorded with the
UF2 asset.

## [Unreleased]

Release preparation for v1.0.0 is tracked in
[`docs/RELEASE_CHECKLIST.md`](docs/RELEASE_CHECKLIST.md). No v1.0.0 firmware is
claimed until the production build and the target-hardware gate are complete.

## [0.9.3] - 2026-09-10

- Replaced the fixed demo presentation with the same play/update/render path as
  normal gameplay, including enemy AI, weapons, collisions, waves, HUD, and
  automatic attacks.
- Kept the title music running through the demo and returned to the title after
  30 seconds.
- Extended the lead motif to a 3.2-second phrase and reduced each phrase unit
  from four repetitions to two while retaining the title-track length.
- Added synthesized kick, snare, hat, crash, and tom parts to the menu track.
- Made engine pitch, noise, level, and stereo bias respond to throttle and
  steering/pitch load.
- Added the build version to the title screen and startup UART log.

## [0.9.2] - 2026-09-09

- Added the 10-second game-over return, 30-second title-to-demo transition,
  demo auto-attack, pause/resume, direct retry, combat guidance, and fixed
  result statistics.
