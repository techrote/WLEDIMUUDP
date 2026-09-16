# WLEDIMUUDP — Testing and CI Contract

## Purpose

The project must be debuggable without having to wonder simultaneously whether the packet format, network stack, IMU driver and motion mapping are all wrong. Testing is therefore layered deliberately.

## Required automated gates

The CI baseline grows with the roadmap and must include:

1. deterministic formatting/lint check;
2. host/native unit tests for protocol, motion and mapping code as those modules exist;
3. host utility builds/smoke tests where practical;
4. reference ESP32-S3 firmware build;
5. warning audit for project code;
6. documentation/link/config sanity checks where cheap and deterministic.

An implementation issue must not weaken existing gates merely to merge.

## WU-001 accepted gate

WU-001 established Python 3.12, PlatformIO Core `6.1.18`, clang-format `18.1.8`, strict formatting, C++17 native Unity tests with `-Wall -Wextra -Wpedantic -Werror`, and the ESP32-S3 reference compile/smoke build. Its accepted suite contains 10 named protocol tests.

## WU-002 accepted host-probe gate

WU-002 preserved every WU-001 gate and added 10 host-probe/transport tests, deterministic packet patterns, exact ABI reuse, CLI validation, packet hex/decode coverage, localhost exact-byte UDP loopback, a warning-as-error native `host_probe` build, real dry-run CLI execution and preserved ESP32-S3 compilation.

WU-002 was squash-merged through PR #9 as `181d1a108a8887bc74008b545b94368fe10725b5`. The accepted baseline therefore contained 20 named native tests before WU-003.

## WU-003 accepted motion-core gate

WU-003 added 11 named native motion tests and an ESP32-S3 smoke use of the portable motion library. The suite verifies the reusable nine-family trace corpus, stationary convergence, tilt without false motion, spin/translation distinction, bounded impact behavior, motion→stillness decay, malformed/stale rejection, calibration, exact replay and deterministic timing-jitter resilience.

WU-003 was squash-merged through PR #10 as `8ef17a102624fff5c9aa26d372d19d4fd04a9638`. The accepted baseline contains 31 named native tests before WU-004.

## WU-004 mapper gate

WU-004 preserves the full WU-001/002/003 gate and adds **13 named native mapping tests** plus a second warning-as-error native inspection executable, `mapping_probe`.

The mapper suite verifies:

- Balanced-v1 profile identity, configuration defaults and monotonic 16-entry center table;
- stationary level converges to zero spectrum/level/magnitude and quiet major peak;
- stationary tilt creates no false level or spectrum;
- opposite tilts shape existing band energy while preserving identical level;
- spin and translational sway occupy distinct semantic groups;
- shake produces stronger higher/transient-band activity than slow roll;
- tap/impact produces exactly one `samplePeak` rising-edge event and transient spectrum energy;
- motion followed by stillness clears raw/smoothed activity, spectrum, magnitude and peak;
- all nine WU-003 trace families remain finite and bounded, with every band `<=254`;
- `FFT_MajorPeak` tracks the locked synthetic-spectrum centroid;
- explicit mapping configuration changes are deterministic;
- two independent fixed trace→motion→mapper chains produce byte-identical WU-001 packets;
- invalid feature snapshots do not advance mapper peak state or retrigger a peak.

The native `mapping_probe` target exposes representative `still`, `sway`, `spin`, `shake`, `impact`, `tilt-left` and `tilt-right` snapshots. CI builds it with `-Werror` and executes the `spin` preset. This is host inspection evidence, not physical WLED validation.

The resulting expected native suite size is **44 named tests**: 10 protocol + 10 host/transport + 11 motion + 13 mapping. The exact count must be confirmed from the final CI log before merge rather than inferred solely from source.

## Protocol tests

Audio Sync V2 tests include exact 44-byte size/header/reserved layout, known binary32 offsets, peak and band offsets, `0..254` band clamp, deterministic NaN/Inf sanitisation, no uninitialised output bytes, decoder/golden round trip, repeated byte identity and malformed decoder rejection.

The golden fixture is directly reviewable in source and reused by host tooling.

## Host packet tooling tests

The WU-002 host sender/decoder keeps packet ABI logic in `WledImuUdpCore` and verifies deterministic known patterns, encode/decode agreement, destination/port/rate parsing, invalid arguments, local UDP exact-byte preservation, malformed-packet rejection, scripted order/count and offline diagnostics.

WU-004's mapper probe is separate from the network probe so mapping inspection does not depend on socket/network behavior.

## Motion feature trace tests

Pure motion processing uses deterministic synthetic traces for stationary level/tilted, slow roll, translational sway, constant spin, tap/impact, shake, motion-to-stillness decay and malformed/non-finite samples. Fixtures live in `test/fixtures/motion_traces.hpp` and are shared directly with WU-004.

Feature-level invariants include static tilt without false activity, rotation/translation separation, bounded impact latch/refractory behavior, stillness decay, invalid/stale rejection, finite timing-jitter behavior and deterministic replay. Physical sensor-noise/tap-threshold validation is not implied.

## Mapping tests

Motion→synthetic-audio testing keeps feature extraction and packet encoding as independent boundaries while exercising the full chain where byte determinism matters.

Balanced-v1 tests lock:

- near-silent stillness;
- orientation shaping without false energy;
- translation/rotation/shake/impact distribution differences;
- raw versus smoothed level behavior inherited from WU-003 energy fields;
- one-shot packet peak semantics over the longer WU-003 impact latch;
- all band/value bounds;
- RMS spectral magnitude;
- center-table/centroid major peak;
- deterministic configuration and mapper state;
- exact WU-001 encoder interoperability.

Perceptual quality on real stock WLED effects remains a later physical evidence layer.

## Firmware build tests

CI compiles the reference ESP32-S3 environment from a clean checkout using documented dependencies. Until WU-005, this remains a portable-core smoke target and deliberately omits Wi-Fi, live IMU and LED runtime dependencies.

WU-004's smoke firmware instantiates the accepted motion extractor and Balanced-v1 mapper, maps a synthetic stationary sample and passes the resulting frame through the canonical 44-byte encoder. This proves embedded compilation of the pure-core chain, not physical sensor/network behavior.

Firmware CI needs no real credentials. Warnings from project code are defects unless documented and justified.

## Network behavior tests

Core CI cannot prove multicast delivery across arbitrary infrastructure. WU-002 tests the local transport seam with a real localhost UDP datagram, while multicast delivery remains observable through the host tool and subject to later LAN/receiver validation.

The future firmware/network adapter needs seams for disconnected/reconnect state, send/error counters, no fake motion packet on failure and configurable destination.

## Stock-WLED interoperability evidence

A real stock-WLED receiver test is a separate evidence layer. Before claiming a receiver version is validated, record exact WLED release/commit, receive/network configuration, sender revision, packet rate/destination, known-pattern result, representative effects, anomalies and network topology.

WU-001 through WU-004 contain automated source/byte/semantic/build evidence only. No physical receiver or live-QMI8658 claim may be inferred from green CI.

## Performance and resource checks

For the reference firmware, track steady-state timing, dropped samples, UDP cadence, hot-path allocations, flash/RAM size and reconnect behavior. Hard budgets should follow measurement rather than assumption.

The WU-003 extractor and WU-004 mapper are fixed-state and allocation-free in their processing hot paths; synthetic fixture vectors and desktop tool strings are host/test assets only.

## CI implementation sequence

- WU-001: formatting + native protocol tests + reference firmware compile;
- WU-002: host probe tests + native executable build + CLI smoke + local UDP loopback;
- WU-003: deterministic motion trace tests + embedded motion-core smoke;
- WU-004: mapping/profile tests + mapper inspection executable build/smoke + embedded full pure-core smoke;
- WU-005: reference QMI8658 firmware and network adapter tests/build;
- WU-006+: integration, compatibility and release checks.

## Merge evidence

Every implementation PR must state tests run, firmware/tool environments built, CI run/status, warnings observed, hardware validation performed or explicitly unavailable, and documentation reconciliation.

A green automated gate authorises merge under the repository workflow, but must never be described as physical validation when it is not.
