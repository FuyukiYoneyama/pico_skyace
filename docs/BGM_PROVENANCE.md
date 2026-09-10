# BGM provenance and distribution record

## Asset covered

- Source file: `src/game/bgm_track.h`
- Runtime format: deterministic note and drum tables; the firmware does not
  parse a MIDI file or load an audio sample
- Uses: title, game-over, and demo music
- Current arrangement length: 102.7 seconds before looping

## Origin

The track was requested as an original, compact chiptune composition for Sky
Ace, with a late-1980s/early-console air-combat atmosphere. ChatGPT (OpenAI)
generated the initial musical material. The project then converted that
material into note tables and edited the arrangement, including the expanded
3.2-second lead phrases and the synthesized kick, snare, hat, crash, and tom
parts.

No copyrighted game recording, score, MIDI, or sample was supplied as a source,
and no third-party audio asset is stored in this repository. The filename
`ace_combat_1_inspired_original.mid` in the source comment describes the
original working material; that MIDI file is not distributed and is not needed
to build the firmware.

## Distribution terms

The note data and the source file are distributed with this project under the
[MIT License](../LICENSE), to the extent permitted by applicable law. OpenAI's
[Terms of Use](https://openai.com/policies/terms-of-use/) state that, between
the user and OpenAI and to the extent permitted by law, the user owns the
Output. The same terms also state that output may not be unique and that the
user is responsible for evaluating the output and respecting third-party
rights. This record is provenance, not a warranty that any jurisdiction will
recognize copyright in generated material.

## Review record

- No existing *Ace Combat* audio or score was intentionally copied or sampled.
- No third-party audio file is included in the source tree.
- The project's title and README identify the game as an unofficial fan-made
  work and do not claim affiliation with Bandai Namco Entertainment.
- Any future replacement or addition of music must add its source, license, and
  modification record here before being included in a release.
