# WLEDIMUUDP — Upstream Source Provenance

This file records the external sources used to establish and re-verify compatibility contracts so future implementation agents can check assumptions rather than relying on conversational history.

## WLED Audio Reactive source

Originally verified on 2026-09-15 and re-verified for WU-002 on **2026-09-16** against WLED `main` commit:

`06ae26db67107cb3f6a3d107a92340035991a063`

Primary implementation source:

- https://github.com/wled/WLED/blob/06ae26db67107cb3f6a3d107a92340035991a063/usermods/audioreactive/audio_reactive.cpp

Facts used by this repository include:

- Audio Sync V2 packed payload is 44 bytes;
- V2 header is `00002` plus terminating zero in the six-byte header field;
- field offsets documented in `WLED_AUDIO_SYNC_V2.md` match the upstream packed structure;
- WLED’s transmitter constrains the 16 `fftResult` bytes to `0..254`;
- current transmitter sends the packed structure directly as bytes;
- default Audio Sync UDP port is `11988`;
- ESP32 receive path joins multicast `239.0.0.1`;
- Audio Sync receive mode disables local sound processing in the relevant receive configuration;
- WLED includes a network-only sound/input mode.

## Release/documentation snapshot for WU-002

On 2026-09-16 the latest stable GitHub release observed was **WLED v16.0.1**. This is a documentation/source compatibility reference, **not** a physical receiver-validation claim.

Official WLED documentation consulted:

- Audio Reactive overview: https://kno.wled.ge/advanced/audio-reactive/
- UDP Realtime / Sound Sync: https://kno.wled.ge/interfaces/udp-realtime/
- Main WLED repository: https://github.com/wled/WLED

The Sound Sync documentation states that external senders may send more slowly than the approximately 20 ms cadence but should not send faster. WU-002 therefore locks the host probe to a configurable `1..50 Hz` range with 50 Hz default/max.

## Physical evidence boundary

No physical stock-WLED receiver was available during WU-001/WU-002 implementation. Automated byte conformance, deterministic pattern tests and localhost UDP transport are not substitutes for recording an actual receiver, installed WLED version, receive settings, effects and network topology. Physical compatibility evidence remains pending for later integration work.

## Re-verification rule

Before changing packet ABI, compatibility claims, destination defaults, receiver setup instructions, send cadence or release compatibility, inspect current upstream WLED source/docs again and update:

- `WLED_AUDIO_SYNC_V2.md`;
- `HOST_PROBE.md` where applicable;
- this provenance file;
- relevant tests/fixtures;
- compatibility notes/release evidence.

Do not silently treat this snapshot as immutable protocol law if upstream changes.
