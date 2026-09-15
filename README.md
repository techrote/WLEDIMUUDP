# WLEDIMUUDP

WLEDIMUUDP is a standalone **IMU → WLED Audio Sync V2 UDP bridge**.

Its purpose is deliberately narrow: an ESP32-class sender reads an IMU, converts motion into a synthetic audio-reactive control stream, and transmits packets that **stock WLED** can consume through its existing Audio Sync receive path. The sender does not require LEDs, ESPsand, a custom WLED build, or a WLED usermod.

## WU-001 implementation state

The repository currently contains the protocol/bootstrap substrate only:

- explicit host-testable WLED Audio Sync V2 encoder;
- strict reference decoder for tests and later host tooling;
- direct-review 44-byte golden fixture;
- PlatformIO native test environment;
- minimal ESP32-S3 protocol-smoke firmware build;
- deterministic formatting and CI;
- secret-safe Wi-Fi configuration template for later transport work.

Real IMU acquisition, Wi-Fi multicast, motion extraction and motion→spectrum mapping are intentionally deferred to later WU issues.

## Quick start

Use Python 3.12 or another version supported by the pinned PlatformIO release.

```powershell
python -m pip install -r requirements-dev.txt
python -m platformio test -e native
python -m platformio run -e esp32s3
```

To check formatting locally:

```powershell
clang-format --dry-run --Werror lib/WledImuUdpCore/include/wledimuudp/audio_sync_v2.hpp lib/WledImuUdpCore/src/audio_sync_v2.cpp src/main.cpp test/test_audio_sync_v2/test_main.cpp test/test_audio_sync_v2/golden_fixture.hpp config/wifi.example.hpp
```

The `esp32s3` firmware is intentionally a compile/smoke target in WU-001. It does not join Wi-Fi, read an IMU, or drive LEDs.

## Protocol baseline

The project targets current WLED Audio Sync V2 compatibility:

- 44-byte payload;
- header `00002\0`;
- explicit little-endian IEEE-754 binary32 floats;
- `samplePeak` at offset 16;
- 16 GEQ bytes at offsets 18–33, clamped to `0..254`;
- magnitude at offset 36;
- major peak at offset 40;
- default future multicast destination `239.0.0.1:11988`.

The canonical encoder writes the wire layout explicitly; it does not transmit a native packed C++ struct.

## Credentials

`config/wifi.example.hpp` is a placeholder for later transport milestones. When Wi-Fi is implemented, copy it to `config/wifi.local.hpp`; that local file is ignored by Git. Never commit real SSIDs, passwords, tokens, or other secrets.

## Authoritative documentation

The project RAG pack lives under [`docs/rag/`](docs/rag/):

- [`PROJECT.md`](docs/rag/PROJECT.md) — scope, goals, non-goals, terminology.
- [`ARCHITECTURE.md`](docs/rag/ARCHITECTURE.md) — module boundaries and runtime data flow.
- [`WLED_AUDIO_SYNC_V2.md`](docs/rag/WLED_AUDIO_SYNC_V2.md) — protocol compatibility contract.
- [`MOTION_MAPPING.md`](docs/rag/MOTION_MAPPING.md) — IMU conditioning and synthetic-audio semantics.
- [`HARDWARE.md`](docs/rag/HARDWARE.md) — reference hardware and portability rules.
- [`TESTING_AND_CI.md`](docs/rag/TESTING_AND_CI.md) — automated verification and evidence rules.
- [`ROADMAP.md`](docs/rag/ROADMAP.md) — reviewed implementation sequence.
- [`SOURCES.md`](docs/rag/SOURCES.md) — pinned upstream WLED provenance and re-verification rules.
- [`ISSUE_INDEX.md`](docs/rag/ISSUE_INDEX.md) — implementation issue/dependency index.
- [`WU001_IMPLEMENTATION.md`](docs/rag/WU001_IMPLEMENTATION.md) — locked bootstrap/protocol choices.

[`AGENTS.md`](AGENTS.md) defines the repository-wide rules for autonomous implementation work.
