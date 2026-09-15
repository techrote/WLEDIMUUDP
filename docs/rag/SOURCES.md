# WLEDIMUUDP — Upstream Source Provenance

This file records the external sources used to establish the planning contracts. It exists so future implementation agents can re-check assumptions rather than relying on conversational history.

## WLED Audio Reactive source

Verified on 2026-09-15 against WLED commit:

`06ae26db67107cb3f6a3d107a92340035991a063`

Primary implementation source:

- https://github.com/wled/WLED/blob/06ae26db67107cb3f6a3d107a92340035991a063/usermods/audioreactive/audio_reactive.cpp

Facts used by this repository’s planning contract include:

- Audio Sync V2 packed payload is 44 bytes;
- V2 header is `00002` plus terminating zero in the six-byte header field;
- field offsets documented in `WLED_AUDIO_SYNC_V2.md` match the upstream packed structure;
- WLED’s transmitter constrains the 16 `fftResult` bytes to `0..254`;
- current transmitter sends the packed structure directly as bytes;
- default Audio Sync UDP port is `11988`;
- ESP32 receive path joins multicast `239.0.0.1`;
- Audio Sync receive mode disables local sound processing in the relevant receive configuration;
- WLED includes a network-only sound/input mode.

## Official WLED documentation

- Audio Reactive overview: https://kno.wled.ge/advanced/audio-reactive/
- Main WLED repository: https://github.com/wled/WLED

## Re-verification rule

Before changing packet ABI, compatibility claims, destination defaults, receiver setup instructions or release compatibility, inspect current upstream WLED source/docs again and update:

- `WLED_AUDIO_SYNC_V2.md`;
- this provenance file;
- relevant tests/fixtures;
- compatibility notes/release evidence.

Do not silently treat this 2026-09-15 snapshot as immutable protocol law if upstream has changed.