# WLEDIMUUDP — Testing and CI Contract

## Purpose

The project must be debuggable without having to wonder simultaneously whether the packet format, network stack, IMU driver and motion mapping are all wrong. Testing is therefore layered deliberately, and automated evidence is never promoted into a physical-hardware claim.

## Required automated gates

The CI baseline grows with the roadmap and must include:

1. deterministic formatting/lint check;
2. host/native unit tests for protocol, motion, mapping and portable firmware-support code;
3. host utility builds/smoke tests where practical;
4. reference ESP32-S3 firmware build against real Arduino Wi-Fi/Wire APIs;
5. warning audit for project code;
6. documentation/link/config sanity checks where cheap and deterministic.

An implementation issue must not weaken existing gates merely to merge.

## Accepted automated baseline through WU-005

WU-001 established Python 3.12, PlatformIO Core `6.1.18`, clang-format `18.1.8`, strict formatting, C++17 native Unity tests with `-Wall -Wextra -Wpedantic -Werror`, and the ESP32-S3 reference compile/smoke build.

WU-002 preserved those gates and added deterministic host probe patterns, CLI validation, exact packet hex/decode coverage, localhost exact-byte UDP loopback, a warning-as-error `host_probe` build and real dry-run smoke invocation.

WU-003 added the deterministic motion-feature corpus and motion-core tests.

WU-004 added Balanced-v1 mapping/profile tests plus the warning-as-error `mapping_probe` build/smoke target.

WU-005 added the portable hardware/runtime-support tests and upgraded the embedded gate to compile the actual QMI8658 + calibration + motion + mapping + encoder + Arduino Wi-Fi sender runtime.

The exact accepted WU-005 PR head was `280ee103cb63a905c487808b2360cd17b5c8b519`. CI run **35161820781** completed successfully on 2026-09-16 with:

- formatting green;
- **57 native test cases: 57 succeeded, 0 failed**;
- host probe build/smoke green;
- mapping probe build/smoke green;
- ESP32-S3 firmware build green;
- firmware size reported by that gate: 23,492 bytes RAM usage and 372,717 bytes flash program usage in the pinned build environment.

PR #12 was subsequently merged to `main` as `41574f11b71f82ee8a2a9dd91f88bc3f12b81c14`, and issue #5 was closed completed.

The accepted 57 native tests comprise:

- 10 protocol tests;
- 10 host/transport tests;
- 11 motion tests;
- 13 mapping tests;
- 13 WU-005 firmware-support tests.

## WU-006 integration gate

WU-006 preserves every inherited gate and adds four portable firmware/integration tests, bringing the expected native suite to **61 tests** before exact merge-gating confirmation.

The new checks lock:

- explicit runtime firmware/protocol identity;
- frame-generation eligibility independent of Wi-Fi versus packet-emission eligibility requiring Wi-Fi;
- mapper progression across an offline impact/clear sequence so reconnect does not replay a stale one-shot `samplePeak`;
- deterministic bounded spectrum summary diagnostics, including clamping values to the protocol band maximum.

WU-006 also changes the real firmware compile path, so the ESP32-S3 gate must compile the expanded status surface and the generation/send split against the pinned Arduino-ESP32 APIs. Documentation-only evidence is insufficient if that embedded build fails.

The final authoritative WU-006 test count, exact head and CI run belong on PR #13 / issue #6 after the exact merge candidate passes. Do not substitute source arithmetic for that final evidence.

## Protocol tests

Audio Sync V2 tests include exact 44-byte size/header/reserved layout, known binary32 offsets, peak and band offsets, `0..254` band clamp, deterministic NaN/Inf sanitisation, no uninitialised output bytes, decoder/golden round trip, repeated byte identity and malformed decoder rejection.

The golden fixture is directly reviewable in source and reused by host tooling.

## Host packet tooling tests

The WU-002 host sender/decoder keeps packet ABI logic in `WledImuUdpCore` and verifies deterministic known patterns, encode/decode agreement, destination/port/rate parsing, invalid arguments, local UDP exact-byte preservation, malformed-packet rejection, scripted order/count and offline diagnostics.

The host probe is also the first physical receiver/network diagnostic seam: known deterministic traffic should be proved before tuning live IMU behavior.

WU-004's mapper probe is separate from the network probe so mapping inspection does not depend on socket/network behavior.

## Motion feature trace tests

Pure motion processing uses deterministic synthetic traces for stationary level/tilted, slow roll, translational sway, constant spin, tap/impact, shake, motion-to-stillness decay and malformed/non-finite samples. Fixtures live in `test/fixtures/motion_traces.hpp` and are shared directly with mapping validation.

Feature-level invariants include static tilt without false activity, rotation/translation separation, bounded impact latch/refractory behavior, stillness decay, invalid/stale rejection, finite timing-jitter behavior and deterministic replay. Physical sensor-noise/tap-threshold validation is not implied.

## Mapping tests

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

WU-006 reviews these tests as the deterministic representative motion-class matrix and deliberately retains Balanced-v1 unchanged because no automated regression or semantic mismatch justifies a tuning change. Perceptual quality on real stock WLED effects remains a separate physical evidence layer.

## Firmware-support and hardware-adapter tests

`lib/WledImuUdpFirmware` deliberately hosts portable policy/data conversion so native CI can test sensor bytes, axis transforms, calibration qualification, timing, frame/send eligibility and reconnect safety with no Arduino dependency.

WU-005 coverage locks:

- Waveshare ESP32-S3-Matrix pins/bus/address profile;
- explicit signed/permuted board-axis transform;
- reviewable QMI8658 identity/configuration/range/scaling/effective-ODR constants;
- signed 12-byte raw accel/gyro decode into g and deg/s;
- malformed sensor-block rejection;
- stationary calibration acceptance and moving/invalid-window behavior;
- multicast defaults and live/diagnostic send safety;
- bounded reconnect attempts;
- 64-bit extension across ESP32 `micros()` wrap;
- fixed-rate scheduling without catch-up bursts;
- deterministic canonical diagnostic-frame encoding.

WU-006 adds frame-generation/network separation, reconnect peak-state continuity, runtime identity and spectrum-summary checks.

`src/qmi8658_adapter.*` contains the actual `TwoWire` transactions and is compile-gated in the ESP32-S3 environment. Physical tests are still required to prove real bus transactions and axis orientation.

## Firmware build tests

CI compiles the reference ESP32-S3 environment from a clean checkout using the pinned toolchain and placeholder credentials.

The embedded target includes:

- Arduino `Wire` QMI8658 adapter;
- startup/recalibration orchestration;
- accepted motion and Balanced-v1 mapper chain;
- canonical 44-byte Audio Sync encoder;
- Arduino `WiFi` / `WiFiUDP` station/multicast path;
- bounded reconnect and 1 Hz serial diagnostics;
- placeholder credential behavior;
- explicit sensor-independent diagnostic mode.

WU-006 additionally requires the embedded compiler to accept the richer status output and continued frame generation during Wi-Fi outage. No sender-LED dependency is compiled or initialised.

Firmware CI needs no real credentials. `CHANGE_ME` compiles but intentionally suppresses Wi-Fi connection attempts. Warnings from project code are defects unless documented and justified.

## Network behavior tests

Core CI cannot prove multicast delivery across arbitrary infrastructure. WU-002 tests the local host transport seam with a real localhost UDP datagram. WU-005 tests multicast defaults, bounded reconnects and packet-emission eligibility while compiling the real Arduino UDP call.

WU-006 locks a key state invariant: network availability controls **emission**, not live mapper progression. During an outage, frames may continue to be generated from valid motion state but packets are skipped rather than queued/retried. Reconnect therefore cannot itself replay an impact peak or create a catch-up burst.

Physical LAN delivery, loss, ordering, AP multicast behavior and multi-receiver behavior remain physical/network evidence. Audio Sync V2 has no WLEDIMUUDP-owned acknowledgement, retransmission or sequence/reordering layer.

## Stock-WLED interoperability evidence

A real stock-WLED receiver test is a separate evidence layer. Before claiming a receiver version is validated, record:

- exact WLED release/build;
- receive/network configuration;
- sender/probe commit;
- packet rate/destination;
- host known-pattern results;
- firmware diagnostic-mode result;
- representative named effects and motion-class observations;
- anomalies and network topology.

WU-006 re-verified source compatibility on 2026-09-17 against WLED `main` `06ae26db67107cb3f6a3d107a92340035991a063` and stable v16.0.1. No physical receiver is available to the implementation environment, so the matrix in `WU006_IMPLEMENTATION.md` remains explicitly pending rather than inferred from green CI.

## Performance and resource checks

For the reference firmware, track steady-state timing, dropped/invalid samples, generated frame cadence, successful UDP cadence, hot-path allocations, flash/RAM size and reconnect behavior. Hard budgets should follow measurement rather than assumption.

Portable motion/mapping/firmware-support state is fixed-size and allocation-free in processing hot paths. The live project-owned loop uses fixed QMI/semantic-frame/packet storage; Arduino networking/driver internals may manage their own resources. Synthetic fixture vectors and desktop tool strings remain host/test assets only.

The bounded 1 Hz WU-006 status line reports actual accepted IMU samples, generated frames and successful packets per window so configured 224.2 Hz / 50 Hz targets can be compared with physical observations later.

## CI implementation sequence

- WU-001: formatting + native protocol tests + reference firmware compile;
- WU-002: host probe tests + native executable build + CLI smoke + local UDP loopback;
- WU-003: deterministic motion trace tests + embedded motion-core smoke;
- WU-004: mapping/profile tests + mapper inspection executable build/smoke + embedded full pure-core smoke;
- WU-005: board/sensor/calibration/scheduling/network-state native tests + actual reference sender build;
- WU-006: end-to-end observability, frame/send separation and reconnect-state regressions while preserving every inherited gate;
- WU-007: release/package/setup checks without weakening the integration baseline.

## Merge evidence

Every implementation PR must state tests run, firmware/tool environments built, CI run/status, warnings observed, hardware validation performed or explicitly unavailable, and documentation reconciliation.

A green automated gate authorises merge under the repository workflow, but must never be described as physical validation when it is not.
