# Changelog

This file records user-visible changes. The exact firmware distributed for a
release is identified by its versioned Git tag and the SHA-256 recorded with the
UF2 asset.

## [Unreleased]

Release preparation for v1.0.0 is tracked in
[`docs/RELEASE_CHECKLIST.md`](docs/RELEASE_CHECKLIST.md). No v1.0.0 firmware is
claimed until the production build and the target-hardware gate are complete.

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
