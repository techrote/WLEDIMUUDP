# WLEDIMUUDP

WLEDIMUUDP is a standalone **IMU → WLED Audio Sync V2 UDP bridge**.

Its purpose is deliberately narrow: an ESP32-class sender reads an IMU, converts motion into a synthetic audio-reactive control stream, and transmits packets that **stock WLED** can consume through its existing Audio Sync receive path. The sender does not require LEDs, ESPsand, a custom WLED build, or a WLED usermod.

## Current implementation state

WU-001 through WU-004 establish the complete pure-core path before live hardware/network integration:

- explicit host-testable WLED Audio Sync V2 encoder and strict decoder;
- direct-review 44-byte golden fixture;
- deterministic host Audio Sync patterns, send/listen/decode tooling and localhost UDP coverage;
- deterministic sensor-independent motion feature extraction with calibration primitives and reusable synthetic traces;
- versioned **Balanced-v1** motion→synthetic-audio mapping;
- four semantic spectrum groups for translation, rotation, shake/jerk and impacts;
- orientation-dependent spectral shaping without false activity at rest;
- deterministic magnitude/major-peak and one-shot packet peak semantics;
- native mapper inspection tool that prints the semantic frame and exact encoded packet;
- ESP32-S3 smoke build exercising motion→mapping→44-byte encoding;
- deterministic formatting/CI and secret-safe configuration skeleton.

Live QMI8658/QMI8658C acquisition and ESP32 Wi-Fi multicast are the next WU-005 milestone. Physical stock-WLED effect validation/tuning remains WU-006.

## Quick start

Use Python 3.12 or another version supported by the pinned PlatformIO release.

```powershell
python -m pip install -r requirements-dev.txt
python -m platformio test -e native
python -m platformio run -e host_probe
python -m platformio run -e mapping_probe
python -m platformio run -e esp32s3
```

On Windows, exercise the network host probe without sending packets:

```powershell
.\.pio\build\host_probe\program.exe send --dry-run --pattern single-band --frames 4
```

Inspect representative Balanced-v1 mapper output and the exact 44-byte packet without IMU hardware:

```powershell
.\.pio\build\mapping_probe\program.exe spin
.\.pio\build\mapping_probe\program.exe tilt-left
.\.pio\build\mapping_probe\program.exe impact
```

Mapper presets are `still`, `sway`, `spin`, `shake`, `impact`, `tilt-left`, and `tilt-right`.

Send the default 160-frame scripted Audio Sync sequence to stock WLED's default multicast destination:

```powershell
.\.pio\build\host_probe\program.exe send
```

Decode the committed golden packet:

```powershell
.\.pio\build\host_probe\program.exe decode --hex "30303030320000000000803f000020400100000102030405060708090a0b0c0d0e0f0000000080410000dc43"
```

See [`docs/rag/HOST_PROBE.md`](docs/rag/HOST_PROBE.md) for network probe patterns/listening/receiver setup, and [`docs/rag/WU004_IMPLEMENTATION.md`](docs/rag/WU004_IMPLEMENTATION.md) for the locked Balanced-v1 mapping contract.

The `esp32s3` firmware remains a compile/smoke target at this stage. It does not yet join Wi-Fi, read a live IMU, or drive LEDs.

## Protocol baseline

The project targets WLED Audio Sync V2 compatibility:

- 44-byte payload;
- header `00002\0`;
- explicit little-endian IEEE-754 binary32 floats;
- `samplePeak` at offset 16;
- 16 GEQ bytes at offsets 18–33, clamped to `0..254`;
- magnitude at offset 36;
- major peak at offset 40;
- default multicast destination `239.0.0.1:11988`;
- host probe default/max send rate 50 Hz.

The canonical encoder writes the wire layout explicitly; it does not transmit a native packed C++ struct. Motion/mapping code produces semantic frames and does not duplicate the packet ABI.

## Credentials

`config/wifi.example.hpp` is a placeholder for WU-005 ESP32 transport work. When Wi-Fi is implemented, copy it to `config/wifi.local.hpp`; that local file is ignored by Git. Never commit real SSIDs, passwords, tokens, or other secrets.

## Authoritative documentation

The project RAG pack lives under [`docs/rag/`](docs/rag/):

- [`PROJECT.md`](docs/rag/PROJECT.md) — scope, goals, non-goals, terminology.
- [`ARCHITECTURE.md`](docs/rag/ARCHITECTURE.md) — module boundaries and runtime data flow.
- [`WLED_AUDIO_SYNC_V2.md`](docs/rag/WLED_AUDIO_SYNC_V2.md) — protocol compatibility contract.
- [`HOST_PROBE.md`](docs/rag/HOST_PROBE.md) — WU-002 host sender/listener/decoder and receiver setup.
- [`MOTION_MAPPING.md`](docs/rag/MOTION_MAPPING.md) — accepted motion and Balanced-v1 synthetic-audio semantics.
- [`HARDWARE.md`](docs/rag/HARDWARE.md) — reference hardware and portability rules.
- [`TESTING_AND_CI.md`](docs/rag/TESTING_AND_CI.md) — automated verification and evidence rules.
- [`ROADMAP.md`](docs/rag/ROADMAP.md) — reviewed implementation sequence and accepted milestones.
- [`SOURCES.md`](docs/rag/SOURCES.md) — pinned upstream WLED provenance and re-verification rules.
- [`ISSUE_INDEX.md`](docs/rag/ISSUE_INDEX.md) — implementation issue/dependency index.
- [`WU001_IMPLEMENTATION.md`](docs/rag/WU001_IMPLEMENTATION.md) — WU-001 bootstrap/protocol choices.
- [`WU003_IMPLEMENTATION.md`](docs/rag/WU003_IMPLEMENTATION.md) — WU-003 motion-core contract.
- [`WU004_IMPLEMENTATION.md`](docs/rag/WU004_IMPLEMENTATION.md) — WU-004 Balanced-v1 mapping contract.

[`AGENTS.md`](AGENTS.md) defines the repository-wide rules for autonomous implementation work.
