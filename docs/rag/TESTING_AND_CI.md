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

## WU-002 accepted host-probe gate

WU-002 preserved every WU-001 gate and added:

- 10 named host-probe/transport tests in the native Unity run;
- deterministic coverage for all 11 selectable pattern names, including the composed `scripted` mode;
- exact scripted segment order and 160-frame cycle regression;
- proof that host pattern packets are produced by the canonical WU-001 encoder;
- CLI defaults/overrides plus invalid argument rejection;
- exact packet hex round trip and malformed decoder rejection;
- a real localhost UDP datagram whose received bytes must equal the canonical 44-byte packet;
- `host_probe` native executable build with the same warning-as-error posture;
- execution of the built CLI in `send --dry-run` smoke mode;
- preservation of the ESP32-S3 firmware build after host-only socket code was added.

WU-002 was squash-merged through PR #9 as `181d1a108a8887bc74008b545b94368fe10725b5` after exact-head CI passed. The accepted baseline therefore contains 20 named native tests before WU-003 additions.

## WU-003 motion-core gate

WU-003 preserves the complete WU-001/WU-002 gate and adds 11 named native motion tests plus an ESP32-S3 compile-time smoke use of the portable motion library.

The motion suite verifies:

- the reusable nine-family fixture corpus exists and each generated trace has at least 400 timestamped samples;
- stationary level input converges to stillness with near-zero instantaneous/smoothed energy;
- stationary tilt changes orientation without creating persistent motion energy;
- constant-rate spin and translational sway remain semantically distinguishable;
- impact assertion is brief and produces one bounded trigger window under the tap fixture;
- motion followed by long stillness decays predictably;
- malformed/non-finite samples are rejected without poisoning future state;
- non-monotonic timestamps are rejected without state advance;
- calibration estimates gyro bias, gravity reference and positive noise floors from stationary samples;
- fixed initial state plus fixed timestamped trace is exactly repeatable for selected feature fields/state;
- deterministic 4.5/5.5 ms timing jitter remains finite and converges to stillness.

The first complete implementation head, `24ff0e4be7b01fb406aedb0c5baabc4f5790b3d3`, passed GitHub Actions run `35039697092`: strict formatting, all native tests, host executable build, real host CLI smoke execution, and the ESP32-S3 reference firmware build.

The WU-003 branch contains 31 named native tests in total: 10 protocol + 10 host/transport + 11 motion. Documentation-only reconciliation commits must pass the same complete gate before merge.

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

Pure motion processing is tested with deterministic synthetic traces for stationary level/tilted, slow roll, translational sway, constant spin, tap/impact, shake, motion-to-stillness decay and malformed/non-finite samples.

WU-003 fixtures live in `test/fixtures/motion_traces.hpp` and are intentionally reusable by WU-004. Tests assert semantic invariants rather than merely exercising code paths. Where exact replay matters, fixed timestamped traces must produce exactly repeatable accepted state on the native target.

Required feature-level invariants now include:

- static tilt changes orientation without false activity;
- spin produces angular activity without requiring linear acceleration;
- translation can produce linear activity without angular motion;
- impacts are latched/refractory rather than continuously asserted;
- stillness decays smoothed energy and eventually asserts `still`;
- invalid and stale samples never poison/advance filter state;
- outputs remain finite under documented deterministic timing jitter.

Physical sensor-noise and tap-threshold validation are not implied by these synthetic fixtures.

## Mapping tests

Motion→synthetic-audio tests should keep feature extraction and packet encoding separate. Required checks include near-silent stillness, orientation shaping without false volume, distinguishable translation/rotation/shake/impact distributions, raw-vs-smoothed response, bounded peak behavior, finite coherent major peak and deterministic profiles.

WU-004 should reuse the WU-003 trace corpus so mapping changes can be compared against the accepted feature vocabulary.

## Firmware build tests

CI compiles the reference ESP32-S3 environment from a clean checkout using documented dependencies. Until WU-005, this remains a portable-core smoke target: it proves protocol and motion-core compilation while intentionally omitting Wi-Fi, live IMU and LED runtime dependencies.

WU-003's smoke firmware instantiates `MotionFeatureExtractor` and processes one synthetic stationary `ImuSample`; this checks embedded portability without pretending to validate physical sensor behavior.

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

WU-002 had no physical receiver available. Its reference to WLED v16.0.1 is source/documentation provenance, not physical compatibility evidence. WU-003 likewise contains no physical IMU or receiver evidence.

## Performance and resource checks

For the reference firmware, track steady-state timing, dropped samples, UDP cadence, hot-path allocations, flash/RAM size and reconnect behavior. Hard budgets should follow measurement rather than assumption.

The WU-003 extractor is fixed-state and allocation-free in its processing hot path; synthetic fixture vectors are test-side assets only.

## CI implementation sequence

- WU-001: formatting + native protocol tests + placeholder/reference firmware compile;
- WU-002: host probe tests + native executable build + CLI smoke + local UDP loopback;
- WU-003: deterministic motion trace tests + embedded motion-core smoke compile;
- WU-004: mapping/profile tests;
- WU-005: full reference firmware build and adapter tests;
- WU-006+: integration, compatibility and release checks.

## Merge evidence

Every implementation PR should state tests run, firmware environments built, CI run/status, warnings observed, hardware validation performed or explicitly unavailable, and documentation reconciliation.

A green automated gate authorises merge under the repository workflow, but must never be described as physical validation when it is not.