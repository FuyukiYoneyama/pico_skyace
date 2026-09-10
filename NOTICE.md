# Sky Ace notices

This file records the attribution and distribution notices that accompany the
`pico_skyace` source tree and the firmware built from it.

## Sky Ace

Copyright (c) 2026 FUYUKI YONEYAMA. The project source is distributed under the
[MIT License](LICENSE).

Sky Ace is an unofficial fan-made game. It is not affiliated with, endorsed by,
or sponsored by Bandai Namco Entertainment or the holders of the *Ace Combat*
trademarks. The name *Ace Combat* is used only to describe the intended visual
inspiration; no game code, artwork, sound, music, logo, or other asset from that
series is included in this repository.

## Music

The music note data in [`src/game/bgm_track.h`](src/game/bgm_track.h) was created
for this project as an original chiptune arrangement using ChatGPT (OpenAI) and
then edited into the deterministic note tables used by the firmware. No
third-party recording, sample, MIDI file, or score is distributed with the
repository, and no copyrighted game music was supplied as a source for the
arrangement. The provenance and distribution record is in
[`docs/BGM_PROVENANCE.md`](docs/BGM_PROVENANCE.md).

The project distributes the source file and its resulting note data under the
MIT License to the extent permitted by applicable law. OpenAI's current Terms of
Use state that, as between the user and OpenAI and to the extent permitted by
law, the user owns the Output; the user remains responsible for ensuring that
the input and the resulting use do not infringe third-party rights. See the
[OpenAI Terms of Use](https://openai.com/policies/terms-of-use/) for the terms
applicable to the account and service used to create the output.

## FatFs (ChaN)

`third_party/ChanFatFS/` contains ChaN's FatFs R0.14a source code, used by the
SD-card screenshot feature. It is not part of the project's MIT-licensed
original code. Its complete license notice is preserved in
[`third_party/ChanFatFS/LICENSE.txt`](third_party/ChanFatFS/LICENSE.txt).

When redistributing the FatFs source, retain its copyright notice, license
condition, and disclaimer. The bundled source is unmodified; the build only
suppresses one known GCC false-positive warning for that vendor file.

The official FatFs license description is available from
[ChaN's FatFs documentation](https://elm-chan.org/fsw/ff/doc/appnote.html).

## Raspberry Pi Pico SDK

The firmware is built against an externally installed Raspberry Pi Pico SDK.
The SDK is not bundled in this repository. The SDK is distributed by Raspberry
Pi Trading Ltd. under the BSD-3-Clause license; users must follow the license
and notice files shipped with the SDK version used for their build.

## Scope of these notices

These notices identify the material intentionally included in this repository;
they do not grant rights to any third-party trademark or asset not included
here. If a future release adds an external asset or dependency, its license and
source must be added to this file before that release is published.
