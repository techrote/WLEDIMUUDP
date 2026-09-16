# WLEDIMUUDP — Reviewed Implementation Roadmap

## Planning objective

Build the smallest trustworthy path from physical IMU motion to expressive **stock WLED Audio Reactive behavior** over WLED Audio Sync V2 UDP, while keeping packet compatibility, motion semantics and hardware integration independently testable.

## Plan review findings

The reviewed roadmap deliberately proves one boundary at a time:

1. protocol truth before hardware;
2. host interoperability tooling as an early product asset;
3. motion extraction separated from fake-spectrum design;
4. static tilt shapes response but never creates activity;
5. sender LEDs are irrelevant to the product;
6. UDP Audio Sync V2 is the product transport/receiver contract;
7. automated conformance is not physical hardware validation.

## Dependency chain

```text
WU-001 protocol/core/CI bootstrap
    │
    ▼
WU-002 host Audio Sync probe + stock-WLED conformance tooling
    │
    ▼
WU-003 deterministic motion feature core
    │
    ▼
WU-004 motion → synthetic-spectrum mapper
    │
    ▼
WU-005 ESP32-S3 + QMI8658 reference firmware
    │
    ▼
WU-006 end-to-end stock-WLED integration/tuning
    │
    ▼
WU-007 release hardening and packaging
```

The implementation sequence is serial by default so each later issue inherits an accepted `main` baseline.

---

## WU-001 — Repository bootstrap and Audio Sync V2 protocol substrate

**Issue:** #1  
**Implementation PR:** #8  
**Status:** accepted. Squash-merged to `main` as `864242b056830060607592c092c9bce1a9cebdbe`; issue #1 closed completed; post-merge CI green.

Delivered the portable protocol library, exact explicit 44-byte Audio Sync V2 encoder, strict decoder, golden packet fixtures, pinned toolchain, secret-safe configuration posture, native tests and ESP32-S3 protocol-smoke build.

**Accepted evidence:** 10 named native protocol tests plus embedded compilation. No physical WLED/live-IMU claim.

---

## WU-002 — Host Audio Sync probe and interoperability harness

**Issue:** #2  
**Implementation PR:** #9  
**Status:** accepted. Squash-merged to `main` as `181d1a108a8887bc74008b545b94368fe10725b5`; issue #2 closed completed; post-merge CI green.

Delivered deterministic Audio Sync patterns, `send`/`listen`/`decode` host tooling, configurable multicast/rate, exact packet hex inspection, POSIX/Winsock UDP, local exact-byte loopback, source/cadence diagnostics and stock-WLED receive documentation.

**Accepted evidence:** final head `5af138acb1e0addc05094c5afa441971a59b9aeb`, CI run `35037828899`, 20 named native tests, host executable/CLI smoke and ESP32-S3 build. WLED source provenance was recorded, but no physical receiver claim was made.

---

## WU-003 — Deterministic IMU motion feature core

**Issue:** #3  
**Implementation PR:** #10  
**Status:** accepted. Squash-merged to `main` as `8ef17a102624fff5c9aa26d372d19d4fd04a9638`; issue #3 closed completed; post-merge CI run `35041126571` green.

### Delivered implementation

- portable `lib/WledImuUdpMotion` with no network/WLED/Arduino/QMI8658 dependency;
- calibrated timestamped `ImuSample` contract;
- gravity/orientation estimation and gravity-separated linear acceleration;
- jerk, calibrated gyro/angular speed and composite motion energy;
- elapsed-time smoothing with long-step cap;
- bounded impact latch/refractory state;
- hysteretic stillness state/confidence;
- explicit `MotionConfig` / `MotionCalibration`;
- stationary calibration accumulator for bias/reference/noise floors;
- nine reusable deterministic trace families;
- embedded smoke use of the portable motion library.

### Accepted evidence

Final documented head `f9a250470e3fcefb66f4881b7c176ea00254b3ca` passed CI run `35040965261`; squash `main` passed post-merge CI. The accepted suite contains 31 named native tests: 10 protocol + 10 host/transport + 11 motion.

### Evidence boundary

No physical QMI8658 noise/threshold, board-axis, live scheduling, mapping usefulness, Wi-Fi or receiver-interoperability claim.

---

## WU-004 — Motion-to-synthetic-audio mapper

**Issue:** #4  
**Implementation PR:** #11  
**Status:** accepted. Squash-merged to `main` as `75a1e4f988d08905574001e8a6378a0f1120be3e`; issue #4 closed completed.

### Delivered implementation

- portable `lib/WledImuUdpMapping` from `MotionFeatures` to accepted `SyntheticAudioFrame`;
- explicit **Balanced-v1** profile/version;
- four semantic four-band groups: translation, rotation, shake/jerk and impact/transients;
- configuration-driven normalisation/gains separate from motion extraction;
- orientation-biased within-group spectral shaping that cannot manufacture energy at rest;
- packet-level one-shot `samplePeak` from the longer WU-003 impact latch;
- `sampleRaw` / `sampleSmth` from instant/smoothed WU-003 motion energy;
- RMS synthetic `FFT_Magnitude`;
- deterministic weighted-centroid `FFT_MajorPeak` using a locked 16-entry center table;
- quiet spectrum/major-peak behavior and invalid-feature state safety;
- reuse of all nine accepted WU-003 motion fixture families;
- deterministic full-chain mapper→WU-001 encoder regressions;
- native `mapping_probe` inspection tool with still/sway/spin/shake/impact/tilt presets and exact packet output;
- CI mapper probe build/smoke and ESP32-S3 pure-core chain compilation.

### Accepted evidence

Final PR head `2fb2c5313c7523ad37f29d1f72820e2bc2f2d2b9` passed CI run `35123755319`: **44 named native tests, 0 failures, 0 ignored**, both host probes green and ESP32-S3 compilation green. Post-merge `main` CI run `35127128137` also completed successfully.

### Evidence boundary

WU-004 proves deterministic synthetic-trace semantics, host inspection and embedded compilation. It does not prove physical stock-WLED perceptual quality, live QMI8658 tuning, board axes, Wi-Fi transport or receiver interoperability.

---

## WU-005 — ESP32-S3 + QMI8658/QMI8658C reference sender firmware

**Issue:** #5  
**Implementation PR:** #12

### Goal

Put the accepted protocol + motion + mapping core on the reference hardware without changing their semantics.

### Implemented reference design

- explicit Waveshare ESP32-S3-Matrix board profile with QMI8658(C) I2C pins, address and axis-transform seam;
- direct Arduino `TwoWire` QMI8658 adapter with identity check, explicit register configuration, signed raw conversion and status/error reporting;
- ±8 g accelerometer and ±1024 dps gyroscope profile;
- QMI8658 6-DoF ODR code `0101`, documented as 224.2 Hz effective, paired with a 4460 us acquisition gate;
- startup stationary calibration with moving/noisy-window rejection, serial reason reporting and explicit recalibration command;
- 64-bit monotonic timestamp extension across 32-bit `micros()` wrap;
- accepted WU-003 motion core + WU-004 Balanced-v1 mapper + WU-001 canonical encoder unchanged;
- station-mode Wi-Fi join and bounded reconnect behavior;
- multicast Audio Sync V2 output to configurable/default `239.0.0.1:11988` at 50 Hz;
- strict live-send gate requiring connected Wi-Fi, healthy sensor, accepted calibration and current valid features;
- immediate live-send inhibition on sensor failure followed by periodic re-probe and mandatory fresh calibration;
- explicit diagnostic mode that sends a deterministic known frame through the canonical encoder/network path without requiring live IMU input;
- one-second serial counters for actual accepted sensor samples and successful packet sends;
- no sender-LED initialization or runtime dependency;
- ignored `wifi.local.hpp` credential/config workflow with a compile-safe committed template;
- 13 new portable firmware-support tests, bringing the expected native suite to 57 pending exact merge-gating CI confirmation.

### Locked implementation contract

The exact board pins, QMI8658 register/range/rate choices, calibration thresholds, network defaults, scheduling semantics, failure behavior and physical-evidence boundary are recorded in `HARDWARE.md`, `WU005_IMPLEMENTATION.md`, `ARCHITECTURE.md`, `TESTING_AND_CI.md` and `SOURCES.md`.

### Acceptance rule

PR #12 may merge only after its exact final head passes the full inherited formatting/native/tool/ESP32-S3 CI gate. Canonical acceptance/merge evidence belongs on PR #12 and issue #5 so this roadmap does not guess a future squash SHA.

### Evidence boundary

WU-005 automated evidence can establish deterministic register decoding/policy seams and compilation of the real Arduino sender runtime. It cannot establish physical board-axis signs, sensor noise/clipping, actual scheduler rates, RF/multicast reliability or stock-WLED visual response. Those observations remain explicit WU-006 work unless separately recorded.

---

## WU-006 — End-to-end stock-WLED integration and mapping validation

**Issue:** #6

### Goal

Turn technically correct packets into a convincing transparent motion→WLED experience.

### Deliverables

- receiver setup guide for stock WLED;
- interoperability evidence against named release(s) when hardware is available;
- tuning across representative stock Audio Reactive effects;
- stillness, shake, roll, spin, flick/tap and recovery validation;
- reconnect/packet-loss checks;
- diagnostics distinguishing sensor, mapper, network and receiver failures;
- mapping refinements backed by trace regressions.

### Exit condition

The default profile is useful across several stock WLED effects and all automated regressions remain green. Unavailable physical evidence is explicitly identified rather than inferred.

---

## WU-007 — Release hardening, setup UX and reproducible packaging

**Issue:** #7

### Goal

Make the project easy to build, flash, configure and troubleshoot without repository archaeology.

### Deliverables

- documented build/flash workflow;
- setup/bootstrap scripts where useful;
- stable configuration template and credential instructions;
- version/compatibility reporting;
- release checklist/changelog convention;
- host tools documented/packaged alongside firmware;
- CI release artifacts if appropriate;
- final RAG reconciliation and compatibility matrix.

### Exit condition

A new user can go from clean checkout to a running sender and stock-WLED receiver using repository documentation alone.

---

## Deferred backlog

Outside the initial chain unless a future issue changes scope:

- captive-portal/web provisioning;
- additional IMU adapters;
- battery/deep-sleep optimisation;
- sophisticated gesture classifiers/ML;
- extra mapping profiles beyond demonstrated need;
- WLED realtime-RGB output;
- ESP-NOW;
- ESPsand integration;
- WLED fork/usermod.

## Roadmap completion rule

Each WU issue is implemented autonomously from current `main` using its issue body plus the RAG pack. Every implementation issue must complete code, tests, documentation reconciliation, PR/CI repair, merge verification and issue closure rather than stopping at a plan or draft PR.
