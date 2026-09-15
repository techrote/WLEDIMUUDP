# WLEDIMUUDP

WLEDIMUUDP is a standalone **IMU → WLED Audio Sync V2 UDP bridge**.

Its purpose is deliberately narrow: an ESP32-class sender reads an IMU, converts motion into a synthetic audio-reactive control stream, and transmits packets that **stock WLED** can consume through its existing Audio Sync receive path. The sender does not require LEDs, ESPsand, a custom WLED build, or a WLED usermod.

The project is designed around current WLED Audio Sync V2 compatibility: a packed 44-byte `00002` packet sent over UDP multicast to the stock Audio Reactive receive path. Motion is treated as a semantic source for synthetic level, peak, 16-band spectrum, magnitude, and major-peak data; WLED remains responsible for turning those controls into lighting effects.

## Core goals

- Work with stock WLED receivers; no custom receiver firmware.
- Keep the sender LED-independent.
- Make motion-to-light behavior expressive rather than mapping one accelerometer number to volume.
- Preserve an exact, host-testable packet encoder and deterministic motion-feature pipeline.
- Provide a reference ESP32-S3 + QMI8658/QMI8658C implementation without coupling the core to that IMU.
- Make compatibility observable with packet capture/reference tools and automated golden fixtures.
- Keep UDP Audio Sync as the product transport; ESP-NOW and ESPsand are explicitly out of scope.

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
- [`ISSUE_INDEX.md`](docs/rag/ISSUE_INDEX.md) — roadmap IDs mapped to GitHub issues #1–#7 and dependencies.

[`AGENTS.md`](AGENTS.md) defines the repository-wide rules for autonomous implementation work.