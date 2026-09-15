# WLEDIMUUDP — Reviewed Implementation Roadmap

## Planning objective

Build the smallest trustworthy path from physical IMU motion to expressive **stock WLED Audio Reactive behavior** over WLED Audio Sync V2 UDP, while keeping packet compatibility, motion semantics and hardware integration independently testable.

## Initial plan considered

The obvious first plan was:

1. bring up QMI8658 on the ESP32-S3;
2. convert accelerometer magnitude to WLED volume/bands;
3. multicast packets;
4. tune until the lights look good.

That path is fast to demonstrate but poor to debug. If the receiver does not react correctly, packet ABI, Wi-Fi multicast, sensor axes, calibration and mapping are all suspects at once.

## Plan review findings

The plan was revised around these findings:

### 1. Protocol truth must precede hardware

A host/reference sender can prove that stock WLED accepts our exact 44-byte packets before any IMU code exists.

### 2. Interoperability tooling should be an early product asset

A PC sender/decoder is useful not only for tests but for future compatibility investigations, packet capture and mapping experiments. It belongs near the start, not as cleanup.

### 3. Motion extraction and fake-spectrum design are different problems

The project needs a stable vocabulary for gravity, linear acceleration, jerk, rotation and impacts before deciding how WLED should perceive them.

### 4. Static tilt should shape response, not create permanent activity

Orientation is useful as a spectral bias/control dimension, but stillness must decay toward silence regardless of orientation.

### 5. Sender LEDs are a distraction

The sender may be a board that happens to contain RGB LEDs, but no implementation issue may make them a dependency. Serial diagnostics are the baseline.

### 6. UDP Audio Sync V2 is the project, not one transport option

ESP-NOW, realtime RGB, ESPsand coupling and custom WLED builds were removed from scope. This keeps the bridge seamless for stock WLED.

### 7. Hardware validation cannot be manufactured by CI

Automated source/byte conformance is required, but stock-WLED and physical-motion claims must be labelled with actual tested hardware/release evidence.

## Revised dependency chain

```text
WU-001 protocol/core/CI bootstrap
    │
    ▼
WU-002 host Audio Sync probe + stock-WLED conformance tooling
    │
    ├──────────────► WU-003 deterministic motion feature core
    │                         │
    │                         ▼
    └────────────────► WU-004 motion → synthetic-spectrum mapper
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

WU-002 and WU-003 are conceptually separable after WU-001, but the issue sequence remains serial by default so each agent inherits a clean accepted baseline.

---

## WU-001 — Repository bootstrap and Audio Sync V2 protocol substrate

### Goal

Create the build/test foundation and exact host-testable WLED Audio Sync V2 encoder before any real IMU integration.

### Deliverables

- PlatformIO/C++ repository structure with native-test and reference ESP32-S3 environments;
- pure protocol data model and explicit 44-byte encoder;
- reference decoder used by tests/tools;
- exact golden packet fixtures and malformed-input tests;
- formatting and CI workflow;
- secrets-safe configuration skeleton;
- initial build/run/test documentation.

### Exit condition

A clean checkout can run native protocol tests and compile a minimal reference firmware without credentials or hardware.

---

## WU-002 — Host Audio Sync probe and interoperability harness

### Goal

Prove and debug the WLED side independently of IMU hardware.

### Deliverables

- host command-line sender capable of transmitting known synthetic patterns to `239.0.0.1:11988` (configurable);
- packet decoder/dump tool;
- deterministic patterns such as silence, level ramp, moving single band, broadband pulse, peak pulse and sweeping major peak;
- loopback/encode-decode tests;
- stock-WLED setup/validation procedure naming tested WLED versions when evidence exists;
- packet-rate/configuration controls suitable for compatibility diagnosis.

### Exit condition

An agent or user can make stock WLED react using known generated control patterns without an IMU sender.

---

## WU-003 — Deterministic IMU motion feature core

### Goal

Create the sensor-independent signal-processing vocabulary for motion.

### Deliverables

- timestamped calibrated IMU sample contract;
- gravity/linear-acceleration separation;
- jerk, gyro/angular energy, orientation/tilt, impact detection and stillness state;
- deterministic filters using elapsed time;
- calibration/noise-floor model;
- synthetic trace fixture corpus and native tests;
- no WLED packet or Wi-Fi dependency in the core.

### Exit condition

Fixed host traces produce repeatable, semantically sensible motion features and decay cleanly to stillness.

---

## WU-004 — Motion-to-synthetic-audio mapper

### Goal

Turn stable motion semantics into expressive WLED Audio Reactive control data.

### Deliverables

- default Balanced mapping profile;
- 16-band synthesis separating slow translation, rotation, shake/jerk and impact energy;
- orientation-dependent spectral shaping that does not create false volume at rest;
- `sampleRaw`, `sampleSmth`, `samplePeak`, magnitude and major-peak semantics;
- deterministic profile/configuration model;
- host tests demonstrating distinguishable motion classes and bounded values;
- mapper output wired through the already accepted V2 encoder.

### Exit condition

Synthetic motion traces generate stable, visibly distinct Audio Sync frames suitable for WU-002’s sender path.

---

## WU-005 — ESP32-S3 + QMI8658/QMI8658C reference sender firmware

### Goal

Put the tested core on the reference hardware without changing protocol/mapping semantics.

### Deliverables

- QMI8658/QMI8658C adapter and board profile;
- documented accelerometer/gyro rate/range and axis transform;
- startup calibration and serial diagnostics;
- Wi-Fi join/reconnect and multicast transport;
- 200 Hz-class sampling / 50 Hz-class packet scheduling baseline where measured practical;
- no sender-LED dependency;
- credentials template with secrets excluded from Git;
- native tests preserved and firmware CI green.

### Exit condition

The reference firmware builds reproducibly and can emit the same known synthetic/control frames as host tools plus live IMU-derived frames.

---

## WU-006 — End-to-end stock-WLED integration and mapping validation

### Goal

Turn technically correct packets into a convincing transparent motion→WLED experience.

### Deliverables

- receiver setup guide for stock WLED;
- interoperability evidence against named WLED release(s) when hardware is available;
- tuning across multiple representative stock Audio Reactive effects;
- stillness, shake, roll, spin, flick/tap and recovery validation;
- reconnect/packet-loss behavior checks;
- diagnostics sufficient to distinguish sensor, mapper, network and receiver failures;
- mapping/profile refinements backed by trace regressions.

### Exit condition

The default profile is useful across several stock WLED effects and all automated regressions remain green. Any unavailable physical evidence is explicitly identified rather than inferred.

---

## WU-007 — Release hardening, setup UX and reproducible packaging

### Goal

Make the project easy to build, flash, configure and troubleshoot without repository archaeology.

### Deliverables

- one documented build/flash workflow for the reference board;
- setup/bootstrap scripts where useful;
- stable configuration template and credential instructions;
- version/compatibility reporting in firmware diagnostics;
- release checklist and changelog/version convention;
- host probe packaged/documented alongside firmware;
- CI artifact/release packaging if repository automation supports it;
- final RAG reconciliation and compatibility matrix.

### Exit condition

A new user can go from clean checkout to a running sender and stock-WLED receiver using repository documentation alone.

---

## Deferred backlog

These are intentionally not part of the initial issue chain:

- captive-portal/web provisioning;
- additional IMU adapters;
- battery/deep-sleep optimisation;
- sophisticated gesture classifiers/ML;
- effect-specific mapping profiles beyond demonstrated need;
- WLED realtime-RGB output;
- ESP-NOW;
- ESPsand integration;
- a WLED fork/usermod.

New work should be added only after the core transparent bridge is demonstrated and measured.

## Roadmap completion rule

Each WU issue is intended to be implemented autonomously from current `main` using its issue body plus the RAG pack. Every implementation issue must complete code, tests, documentation reconciliation, PR/CI repair, merge verification and issue closure rather than stopping at a plan or draft PR.