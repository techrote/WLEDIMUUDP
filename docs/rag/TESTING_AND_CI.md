# WLEDIMUUDP — Testing and CI Contract

## Purpose

The project must be debuggable without having to wonder simultaneously whether the packet format, network stack, IMU driver and motion mapping are all wrong. Testing is therefore layered deliberately.

## Required automated gates

The CI baseline grows with the roadmap and must include:

1. deterministic formatting/lint check;
2. host/native unit tests for protocol, motion, mapping and portable firmware-support code;
3. host utility builds/smoke tests where practical;
4. reference ESP32-S3 firmware build against real Arduino Wi-Fi/Wire APIs;
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

## WU-004 accepted mapper gate

WU-004 preserved the full WU-001/002/003 gate and added **13 named native mapping tests** plus a second warning-as-error native inspection executable, `mapping_probe`.

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

WU-004 was squash-merged through PR #11 as `75a1e4f988d08905574001e8a6378a0f1120be3e`. Its exact pre-merge gate was CI run `35123755319`; post-merge `main` CI run `35127128137` was also green. The accepted suite contains **44 named native tests**: 10 protocol + 10 host/transport + 11 motion + 13 mapping.

## WU-005 reference-sender gate

WU-005 preserves every accepted WU-001–004 gate and adds **13 named native firmware-support tests**. These tests keep board/sensor policy independently checkable even though Arduino `Wire` and Wi-Fi themselves are compiled only in the embedded target.

The WU-005 suite verifies:

- exact Waveshare ESP32-S3-Matrix SDA/SCL/INT pins, bus rate and reference QMI8658 address;
- explicit signed/permuted board-axis transform rather than hidden motion-core remapping;
- reviewable QMI8658 identity/configuration/range/scaling/effective-ODR constants;
- signed 12-byte raw accel/gyro decoding into g and deg/s plus board-axis transform;
- malformed sensor-block rejection without a valid `ImuSample`;
- stationary startup calibration acceptance and gyro-bias/gravity estimation;
- moving startup calibration rejection;
- invalid calibration-sample accounting without contaminating the accepted sample count;
- multicast defaults and live-versus-diagnostic packet-emission safety gates;
- bounded reconnect attempts and reset behavior after successful connection;
- monotonic 64-bit microsecond extension across the ESP32 32-bit `micros()` wrap;
- fixed-rate scheduling that skips missed periods without catch-up bursts;
- deterministic diagnostic-frame byte identity through the canonical WU-001 encoder, with bounded bands.

The expected combined native suite is **57 named tests**: the accepted 44 plus 13 WU-005 tests. The exact final count must still be taken from the merge-gating CI log; source arithmetic alone is not acceptance evidence.

The embedded CI target now compiles the actual WU-005 runtime rather than a pure-core-only smoke program. A clean checkout compiles:

- Arduino `Wire` QMI8658 adapter;
- startup/recalibration orchestration;
- accepted motion and Balanced-v1 mapper chain;
- canonical 44-byte Audio Sync encoder;
- Arduino `WiFi` / `WiFiUDP` station/multicast path;
- bounded reconnect and serial diagnostics;
- placeholder credential behavior;
- explicit sensor-independent diagnostic mode.

This remains compilation evidence only. CI has no physical QMI8658 bus, RF environment or WLED receiver.

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

Perceptual quality on real stock WLED effects remains a physical evidence layer.

## Firmware-support and hardware-adapter tests

`lib/WledImuUdpFirmware` deliberately hosts portable policy/data conversion so native CI can test sensor bytes, axis transforms, calibration qualification, timing and send/reconnect safety with no Arduino dependency.

`src/qmi8658_adapter.*` contains the actual `TwoWire` transactions and is therefore compile-gated in the ESP32-S3 environment. Native tests exercise its pure raw-decoding/config contract; embedded compilation catches Arduino API/type integration. Physical tests are still required to prove real bus transactions and axis orientation.

The runtime reports one-second valid IMU-sample and successful-packet counts so WU-006 can compare configured targets with measured hardware behavior. A configured 224.2 Hz QMI8658 6-DoF ODR and 50 Hz packet gate are not themselves proof of achieved physical cadence.

## Firmware build tests

CI compiles the reference ESP32-S3 environment from a clean checkout using documented dependencies and placeholder credentials.

WU-005 upgrades the former smoke target into the actual reference runtime: direct QMI8658 `Wire` adapter, calibration lifecycle, motion→mapping→encoder path, station Wi-Fi, multicast `WiFiUDP`, reconnect logic and serial diagnostic commands. No sender-LED dependency is compiled or initialised.

Firmware CI needs no real credentials. `CHANGE_ME` compiles but intentionally suppresses Wi-Fi connection attempts. Warnings from project code are defects unless documented and justified.

## Network behavior tests

Core CI cannot prove multicast delivery across arbitrary infrastructure. WU-002 tests the local host transport seam with a real localhost UDP datagram. WU-005 adds native state tests for multicast defaults, bounded reconnects and packet-emission eligibility, while the embedded target compiles the real Arduino multicast call.

Physical LAN delivery, packet loss, reconnect timing and receiver response remain WU-006 evidence. On firmware sensor failure, the live send gate becomes false immediately; on Wi-Fi loss, sends are skipped. Diagnostic mode is the explicit exception that permits a fixed known frame without live sensor input so transport/receiver faults can be isolated.

## Stock-WLED interoperability evidence

A real stock-WLED receiver test is a separate evidence layer. Before claiming a receiver version is validated, record exact WLED release/commit, receive/network configuration, sender revision, packet rate/destination, known-pattern result, representative effects, anomalies and network topology.

WU-001 through WU-005 automated gates establish source/byte/semantic/state/build conformance only unless a separate physical evidence record says otherwise. No live-QMI8658, RF or stock-WLED claim may be inferred from green CI.

## Performance and resource checks

For the reference firmware, track steady-state timing, dropped/invalid samples, UDP cadence, hot-path allocations, flash/RAM size and reconnect behavior. Hard budgets should follow measurement rather than assumption.

WU-003 motion, WU-004 mapping and WU-005 portable scheduling/calibration/decoder state are fixed-size and allocation-free in their processing hot paths. The live project-owned loop uses fixed QMI/packet buffers; Arduino networking/driver internals may manage their own resources. Synthetic fixture vectors and desktop tool strings remain host/test assets only.

## CI implementation sequence

- WU-001: formatting + native protocol tests + reference firmware compile;
- WU-002: host probe tests + native executable build + CLI smoke + local UDP loopback;
- WU-003: deterministic motion trace tests + embedded motion-core smoke;
- WU-004: mapping/profile tests + mapper inspection executable build/smoke + embedded full pure-core smoke;
- WU-005: board/sensor/calibration/scheduling/network-state native tests + actual reference sender build;
- WU-006+: integration, compatibility and release checks.

## Merge evidence

Every implementation PR must state tests run, firmware/tool environments built, CI run/status, warnings observed, hardware validation performed or explicitly unavailable, and documentation reconciliation.

A green automated gate authorises merge under the repository workflow, but must never be described as physical validation when it is not.
