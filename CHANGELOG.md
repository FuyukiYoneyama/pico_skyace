# Changelog

This file records user-visible changes. The exact firmware distributed for a
release is identified by its versioned Git tag and the SHA-256 recorded with the
UF2 asset.

## [Unreleased]

Release preparation for v1.0.0 is tracked in
[`docs/RELEASE_CHECKLIST.md`](docs/RELEASE_CHECKLIST.md). No v1.0.0 firmware is
claimed as a public release until the repository is Public and the versioned tag
and GitHub Release are created.

## [1.0.0] - 2026-09-12 (release preparation)

- Final version-only release-preparation build. Game behavior is unchanged from
  v0.9.18; the version, final build artifacts, and their verification record
  are fixed before the public tag and GitHub Release. The final UF2 is tied to
  the target-hardware smoke record; the public tag remains pending until the
  repository is made Public.

- Documented that the product config runs RP2040 at 250 MHz for LCD timing,
  above the 133 MHz limit stated in Raspberry Pi's RP2040 specifications, and
  recorded the associated hardware/individual-unit caveat in `README.md`.

## [0.9.18] - 2026-09-12

- Added a 500 ms UART settle interval after `stdio_init_all()` and before the
  first `PICO_SKYACE_BOOT` line. This gives the CH340/host COM port time to
  re-enumerate on a cold power-on while retaining the existing flushed identity
  and display-boundary fallback records.

## [0.9.17] - 2026-09-12

- Added a flushed, one-line `PICO_SKYACE_BOOT` identity record containing the
  application name, firmware version, build timestamp, watchdog state, clock,
  and UART settings before peripheral initialization. The display boundary
  repeats the application/version/build identity so a physical UART capture
  remains attributable even when the earliest boot bytes are unavailable.
- Retained the existing human-readable startup and gameplay records for
  compatibility with the validation scenarios.

## [0.9.16] - 2026-09-12

- Release-preparation verification build for the standard RP2040 PicoCalc
  target. It keeps the complete title/play/pause/game-over/demo flow, Wave
  combat, finite weapons, directional warnings, high scores, SD screenshots,
  and the audio system from v0.9.15.
- Diagnostics and emulator evidence are recorded before the final `1.0.0`
  version-only release step. This build is not the public release tag.

## [0.9.15] - 2026-09-12

- Versioned the PicoCalc source changes for diagnostic boot and Demo-session
  logging as a new firmware revision. The rebuilt diagnostic UF2 must report
  `version=0.9.15`; older 0.9.14 UF2 files are not interchangeable.

## [0.9.14] - 2026-09-12

- Moved the high-score display to the top row of both the title and game-over
  screens so it remains visible without covering the gameplay/title art.

## [0.9.13] - 2026-09-12

- Added a high-score display to the title and game-over screens.
- Persisted new records in two checksummed SD-card slots so a failed or
  interrupted update cannot erase the other copy.
- Kept the session score when an SD card is absent or a write fails, so storage
  problems never block a new sortie.
- Replaced the README gameplay image with the supplied in-game combat capture.

## [0.9.12] - 2026-09-12

- Tightened the underwing missile launch offset so the missiles appear close to
  the wing rails instead of far out to the sides.
- Reordered the README so player instructions come first; moved the fan-made
  rights note to a marked note near the licensing section and placed technical
  specifications later.

## [0.9.11] - 2026-09-12

- Changed missile launch visuals to originate from underwing pylons, alternating
  left and right on each successful launch.
- Simplified the GUN tracer to one line from the aircraft nose.

## [0.9.10] - 2026-09-12

- Added a bright alternating flash to the player aircraft when taking damage.
- Coupled speed to vertical flight: climbing applies drag and descending adds
  speed, while level flight retains its existing throttle behavior.

## [0.9.9] - 2026-09-12

- Delayed the `WAVE CLEAR` screen until the final enemy's explosion animation
  has fully finished, so the celebration follows the visible defeat.

## [0.9.8] - 2026-09-12

- Retuned the approach warning so it only sounds while an enemy is closing in
  within the near-range approach band, instead of immediately when the Wave
  enters its aggressive phase at a distant position.

## [0.9.7] - 2026-09-12

- Reorganized the lower HUD: GUN and missiles now share the bottom row on the
  left and right, HP moves above the missile count, and the radar is shifted up
  to keep the weapon row clear.

## [0.9.6] - 2026-09-12

- Cleared explosions, missiles, and target-lock state at the Wave boundary so
  defeated enemies from the previous Wave cannot remain visible when the next
  Wave starts.

## [0.9.5] - 2026-09-12

- Added a dedicated looping game-over BGM with a descending minor-key melody,
  sustained harmony, low bass, and sparse heartbeat-like drums. The title and
  demo continue to use the title track, and returning to the title restores it.

## [0.9.4] - 2026-09-12

- Increased enemy pressure by Wave: later enemies pursue sooner, turn faster,
  move faster, and fire from a wider/longer solution with shorter cooldowns.
  The attack warning remains in place so the stronger attack is still readable.
- Added explicit incoming-attack direction cues: `FROM ...` text plus a red
  direction arrow around the reticle, including rear and diagonal directions.
- Added two-stage enemy attack audio: a low approach alert when an enemy commits
  to pursuit (or enters attack range) and a higher warning when its gun solution
  is ready.
- Retuned the enemy alerts as a clear pulsed electronic buzzer followed by a
  continuous high lock tone, removing the unpleasant noise-only character.
- Kept unattended straight flight dangerous without an immediate rush: enemies
  begin in distant cruise, then commit to pursuit once the Wave aggression
  trigger is reached; attack-mode aircraft can catch the player at idle speed.
- Changed enemy collisions to push the attacker away and keep it alive, so the
  remaining-enemy count only decreases when the player actually shoots one down.
- Added finite GUN ammunition, a wider hit window, out-of-ammo game over, and
  Wave-scaled GUN replenishment.
- Added a static bright-blue `WAVE CLEAR` celebration screen, a short ascending
  clear fanfare, and the remaining-enemy count below the Wave HUD label.
- Varied the final notes of the second repetition in each title-music phrase
  unit while keeping the phrase timing and full track length unchanged.
- Restored a distant cruise phase at the start of each Wave. All living enemies
  enter the aggressive attack phase when half or fewer remain, or when that
  Wave has run for more than 20 seconds.
- Started the real-play demo at Wave 4 so the demo shows the mid-game pressure
  instead of the opening setup.
- Changed the background sun from white to a warm yellow palette.

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
