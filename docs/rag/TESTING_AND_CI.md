# WLEDIMUUDP — Testing and CI Contract

## Purpose

The project must be debuggable without having to wonder simultaneously whether the packet format, network stack, IMU driver and motion mapping are all wrong. Testing is therefore layered deliberately.

## Required automated gates

The CI baseline grows with the roadmap and must include:

1. deterministic formatting/lint check;
2. host/native unit tests for protocol, motion and mapping code as those modules exist;
3. host utility tests where practical;
4. reference ESP32-S3 firmware build;
5. warning audit for project code;
6. documentation/link/config sanity checks where cheap and deterministic.

An implementation issue must not weaken existing gates merely to merge.

## WU-001 accepted gate

WU-001 established:

- Python 3.12 in GitHub Actions;
- PlatformIO Core `6.1.18`;
- clang-format `18.1.8`;
- strict clang-format dry-run over project C/C++ sources;
- PlatformIO `native` Unity protocol tests compiled as C++17 with `-Wall -Wextra -Wpedantic -Werror`;
- PlatformIO `esp32s3` reference compile/smoke build using the Arduino framework.

The accepted WU-001 suite contains 10 named protocol tests. This is automated source/byte/build evidence only; it is not physical stock-WLED or live-IMU validation.

## WU-002 host-probe gate

WU-002 preserves every WU-001 gate and adds:

- 10 named host-probe/transport tests in the native Unity run;
- deterministic coverage for all 11 selectable pattern names, including the composed `scripted` mode;
- exact scripted segment order and 160-frame cycle regression;
- proof that host pattern packets are produced by the canonical WU-001 encoder;
- CLI defaults/overrides plus invalid argument rejection;
- exact packet hex round trip and malformed decoder rejection;
- a real localhost UDP datagram whose received bytes must equal the canonical 44-byte packet;
- `host_probe` native executable build with the same warning-as-error posture;
- execution of the built CLI in `send --dry-run` smoke mode;
- preservation of the ESP32-S3 firmware build after host-only socket code is added.

There are therefore **20 named native tests across the accepted WU-001 protocol suite and WU-002 host suite** on the WU-002 branch. The first substantive WU-002 implementation head passed formatting, native tests, host build, CLI smoke and ESP32-S3 build before documentation reconciliation.

## Protocol tests

Audio Sync V2 tests include:

- encoded size exactly 44 bytes;
- exact header bytes `30 30 30 30 32 00` (`00002\0`);
- reserved bytes zero;
- known binary32 values at offsets 8, 12, 36 and 40;
- `samplePeak` at offset 16;
- all 16 band bytes at offsets 18–33;
- band clamp `0..254`;
- deterministic NaN/Inf sanitisation;
- no uninitialised/padding bytes on output;
- reference decoder/golden round trip;
- repeated encoding byte-identical;
- malformed decoder rejection.

The golden fixture is small enough to review directly in source and is also reused as the documented host-decoder example.

## Host packet tooling tests

The host reference sender/decoder must keep packet ABI logic in `WledImuUdpCore` and test:

- known-pattern deterministic generation;
- encode/decode agreement;
- configurable destination/port/rate parsing;
- invalid arguments fail clearly;
- loopback/local UDP exact byte preservation where the platform permits;
- graceful malformed-packet rejection;
- scripted frame order/count behavior;
- diagnostic output paths without internet access.

The host sender is the first interoperability tool for stock WLED and deliberately exists before IMU integration.

## Motion feature trace tests

Pure motion processing must be tested with deterministic synthetic traces. Required families include stationary level/tilted, slow roll, translational sway, constant spin, tap/impact, shake, motion-to-stillness decay and malformed/non-finite samples.

Tests should assert semantic invariants plus selected golden traces/hashes where useful. Examples include tilt without false motion energy, bounded tap duration, stillness convergence, shake/roll distinction and finite bounded outputs.

## Mapping tests

Motion→synthetic-audio tests should keep feature extraction and packet encoding separate. Required checks include near-silent stillness, orientation shaping without false volume, distinguishable translation/rotation/shake/impact distributions, raw-vs-smoothed response, bounded peak behavior, finite coherent major peak and deterministic profiles.

## Firmware build tests

CI compiles the reference ESP32-S3 environment from a clean checkout using documented dependencies. Until WU-005, this remains a protocol-smoke target: it proves portable core compilation while intentionally omitting Wi-Fi, IMU and LED runtime dependencies.

Firmware CI needs no real credentials. Warnings from project code are defects unless documented and justified.

## Network behavior tests

Core CI cannot prove multicast delivery across arbitrary infrastructure. WU-002 therefore tests the local transport seam with a real localhost UDP datagram, while multicast join/send behavior remains observable through the host tool and subject to later LAN/receiver validation.

The future firmware/network adapter additionally needs seams for disconnected/reconnect state, send/error counters, no fake motion packet on failure and configurable destination.

## Stock-WLED interoperability evidence

A real stock-WLED receiver test is a separate evidence layer. Before claiming a receiver version is validated, record:

- exact WLED release/commit;
- Audio Sync receive/network-only configuration;
- sender revision;
- packet rate and destination;
- known-pattern result;
- several audio-reactive effects observed;
- effect-specific anomalies;
- relevant network topology.

WU-002 had no physical receiver available. Its reference to WLED v16.0.1 is source/documentation provenance, not physical compatibility evidence.

## Performance and resource checks

For the reference firmware, track steady-state timing, dropped samples, UDP cadence, hot-path allocations, flash/RAM size and reconnect behavior. Hard budgets should follow measurement rather than assumption.

## CI implementation sequence

- WU-001: formatting + native protocol tests + placeholder/reference firmware compile;
- WU-002: host probe tests + native executable build + CLI smoke + local UDP loopback;
- WU-003: deterministic motion trace tests;
- WU-004: mapping/profile tests;
- WU-005: full reference firmware build and adapter tests;
- WU-006+: integration, compatibility and release checks.

## Merge evidence

Every implementation PR should state tests run, firmware environments built, CI run/status, warnings observed, hardware validation performed or explicitly unavailable, and documentation reconciliation.

A green automated gate authorises merge under the repository workflow, but must never be described as physical validation when it is not.
