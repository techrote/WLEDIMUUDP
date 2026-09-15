# WLEDIMUUDP — Testing and CI Contract

## Purpose

The project must be debuggable without having to wonder simultaneously whether the packet format, network stack, IMU driver and motion mapping are all wrong.

Testing is therefore layered deliberately.

## Required automated gates

The initial CI baseline should eventually include:

1. deterministic formatting/lint check;
2. host/native unit tests for protocol, motion and mapping code;
3. host utility tests where practical;
4. reference ESP32-S3 firmware build;
5. warning audit for project code;
6. documentation/link/config sanity checks where cheap and deterministic.

An implementation issue must not weaken existing gates merely to merge.

## Protocol tests

Audio Sync V2 tests are mandatory and should include:

- encoded size is exactly 44 bytes;
- exact header bytes `30 30 30 30 32 00` (`00002\0`);
- reserved bytes are zero;
- known binary32 values produce exact expected little-endian bytes at offsets 8, 12, 36 and 40;
- `samplePeak` appears at offset 16;
- all 16 band bytes occupy offsets 18–33;
- band values are clamped to `0..254`;
- NaN/Inf inputs are sanitised deterministically;
- no uninitialised/padding bytes leak into output;
- a reference decoder round-trips golden packets;
- repeated encoding of the same frame is byte-identical.

Golden fixtures should be small enough to review directly in source.

## Host packet tooling tests

The host reference sender/decoder should support automated tests for:

- known-pattern frame generation;
- encode/decode agreement;
- configurable multicast address/port/rate parsing;
- loopback/local UDP test where the platform permits;
- graceful malformed-packet rejection;
- packet timestamp/count diagnostics without depending on internet access.

The host sender is also the first interoperability tool for stock WLED and should exist before IMU integration.

## Motion feature trace tests

Pure motion processing must be tested with deterministic synthetic traces.

Required trace families:

- stationary level;
- stationary tilted;
- slow roll;
- translational sway;
- constant spin;
- tap/impact;
- shake;
- motion-to-stillness decay;
- malformed/non-finite samples.

Tests should assert semantic invariants plus selected golden traces/hashes where useful.

Examples:

- stationary tilted input does not create material motion-energy output;
- tap peak is bounded in duration;
- stillness converges toward silence;
- shake differs spectrally from roll;
- no band exceeds 254;
- fixed traces reproduce exactly.

## Mapping tests

Motion→synthetic-audio tests should keep feature extraction and packet encoding separate.

Required checks include:

- zero/still input yields near-silent frame;
- orientation can shift spectral shape while preserving zero/near-zero total energy when motionless;
- translation, rotation, shake and impact produce distinguishable band distributions;
- `sampleRaw` responds faster than `sampleSmth`;
- peak latching/refractory behavior is bounded;
- major peak is finite and coherent with the synthetic spectrum;
- profile/config changes are deterministic.

## Firmware build tests

CI should compile the reference ESP32-S3/QMI8658 firmware from a clean checkout using only documented dependencies.

Firmware CI does not need real credentials. Build-time configuration must support a non-secret placeholder/test mode.

Warnings from project code should be treated as defects unless documented and justified.

## Network behavior tests

Core CI cannot guarantee multicast delivery on every runner. Prefer deterministic host tests for packet generation and small local transport tests where available.

The firmware/network adapter should have test seams for:

- disconnected state;
- reconnect state;
- send success/failure counters;
- no fake motion packet on transport failure;
- configurable destination.

## Stock-WLED interoperability evidence

A real stock-WLED receiver test is a separate evidence layer.

Before claiming a receiver version is validated, record:

- exact WLED release/commit;
- Audio Sync receive/network-only configuration;
- sender revision;
- packet rate and destination;
- known-pattern test result;
- at least several audio-reactive effects observed;
- any effect-specific anomalies.

If hardware is unavailable, say so. Do not infer physical interoperability solely from source conformance.

## Performance and resource checks

For the reference firmware, keep an eye on:

- steady-state loop/sample timing;
- dropped/missed samples;
- UDP send cadence;
- dynamic allocation count in hot path;
- firmware flash/RAM size;
- reconnect behavior.

Hard performance budgets should only be introduced after measurement. The architecture should nevertheless remain small and allocation-light by construction.

## CI implementation sequence

CI grows with the roadmap:

- WU-001: formatting + native protocol tests + placeholder/reference firmware compile;
- WU-002: host probe tests;
- WU-003: deterministic motion trace tests;
- WU-004: mapping/profile tests;
- WU-005: full reference firmware build and adapter tests;
- WU-006+: integration, compatibility and release checks.

## Merge evidence

Every implementation PR should state:

- tests run;
- firmware environments built;
- CI run/status;
- warnings observed;
- hardware validation performed or explicitly not performed;
- documentation reconciled.

A green automated gate authorises merge under the repository workflow, but must never be described as physical validation when it is not.