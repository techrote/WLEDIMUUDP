# WU-001 — Locked Implementation Contract

## Purpose

This file records implementation choices made while completing WU-001 so later issues can build on concrete behavior rather than planning assumptions.

## Toolchain baseline

- C++17 core code.
- PlatformIO Core pinned through `requirements-dev.txt`.
- `native` environment for host protocol tests.
- `esp32s3` environment using the generic ESP32-S3 DevKitC target as a compile/smoke target only.
- clang-format is pinned and CI-enforced.

The WU-001 ESP32-S3 target deliberately has no Wi-Fi, IMU or LED runtime dependency.

## Protocol data model

`SyntheticAudioFrame` is semantic data, not the wire packet. It contains:

- `sample_raw` and `sample_smoothed` floats;
- boolean `sample_peak`;
- sixteen unsigned band values wide enough for the encoder to demonstrate clamping;
- `magnitude`;
- `major_peak`.

The canonical encoder always produces `AudioSyncV2Packet`, an exact 44-byte array.

## Encoder sanitisation

Before encoding:

- non-finite or negative `sample_raw` becomes `0`;
- non-finite or negative `sample_smoothed` becomes `0`;
- `sample_raw` / `sample_smoothed` are clamped to `0..255`;
- bands are clamped to `0..254`;
- non-finite or negative `magnitude` becomes `0`, then clamps to `0..255`;
- non-finite or non-positive `major_peak` becomes `1.0`;
- `sample_peak` is encoded as `0` or `1`;
- all reserved bytes remain zero because the packet buffer is zero-initialised and only documented fields are written.

Floats are converted to IEEE-754 binary32 bits and written to the packet explicitly in little-endian byte order. The encoder does not serialize a native C++ struct.

## Reference decoder behavior

The decoder is intended for tests and later host tooling, not for the sender hot path. It rejects:

- null input;
- lengths other than 44 bytes;
- the wrong six-byte header;
- non-zero reserved bytes;
- non-finite float fields;
- a non-positive `major_peak`.

It otherwise exposes the received semantic values without silently applying the sender's level clamps.

## Golden fixture

The directly reviewable WU-001 golden packet uses:

- `sample_raw = 1.0`;
- `sample_smoothed = 2.5`;
- `sample_peak = 1`;
- bands `0..15`;
- `magnitude = 16.0`;
- `major_peak = 440.0`.

The resulting exact 44 bytes are committed in the native test directory and compared byte-for-byte.

## Evidence boundary

WU-001 proves source/byte conformance and reference firmware compilation only. It does not claim:

- multicast transmission;
- stock-WLED physical interoperability;
- live IMU behavior;
- motion mapping quality.

Those evidence layers belong to WU-002 and later milestones.
