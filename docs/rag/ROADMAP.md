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
**Status:** accepted. Squash-merged to `main` as `864242b056830060607592c092c9bce1a9cebdbe`; issue #1 closed completed.

Delivered the portable protocol library, exact explicit 44-byte Audio Sync V2 encoder, strict decoder, golden packet fixtures, pinned toolchain, secret-safe configuration posture, native tests and ESP32-S3 protocol-smoke build.

---

## WU-002 — Host Audio Sync probe and interoperability harness

**Issue:** #2  
**Implementation PR:** #9  
**Status:** accepted. Squash-merged to `main` as `181d1a108a8887bc74008b545b94368fe10725b5`; issue #2 closed completed.

Delivered deterministic Audio Sync patterns, `send`/`listen`/`decode` host tooling, configurable multicast/rate, exact packet hex inspection, POSIX/Winsock UDP, local exact-byte loopback, diagnostics and stock-WLED receive documentation.

---

## WU-003 — Deterministic IMU motion feature core

**Issue:** #3  
**Implementation PR:** #10  
**Status:** accepted. Squash-merged to `main` as `8ef17a102624fff5c9aa26d372d19d4fd04a9638`; issue #3 closed completed.

Delivered portable deterministic motion extraction, calibration primitives, gravity separation, jerk/angular energy, stillness, bounded impact semantics and nine reusable trace families.

---

## WU-004 — Motion-to-synthetic-audio mapper

**Issue:** #4  
**Implementation PR:** #11  
**Status:** accepted. Squash-merged to `main` as `75a1e4f988d08905574001e8a6378a0f1120be3e`; issue #4 closed completed.

Delivered portable **Balanced-v1** mapping, four semantic band groups, orientation shaping without false activity, one-shot packet peak semantics, synthetic magnitude/major peak, deterministic mapper→encoder regressions and native mapper inspection tooling.

---

## WU-005 — ESP32-S3 + QMI8658/QMI8658C reference sender firmware

**Issue:** #5  
**Implementation PR:** #12  
**Status:** accepted. Final PR head `280ee103cb63a905c487808b2360cd17b5c8b519` passed CI run `35161820781` with **57/57 native tests**, host/mapping probe gates and ESP32-S3 firmware build green. PR #12 merged to `main` as `41574f11b71f82ee8a2a9dd91f88bc3f12b81c14`; issue #5 closed completed.

### Delivered implementation

- explicit Waveshare ESP32-S3-Matrix board profile and QMI8658(C) adapter;
- ±8 g / ±1024 dps reference profile and 224.2 Hz 6-DoF configuration;
- stationary startup/recalibration qualification;
- monotonic timestamp extension and fixed-rate scheduling;
- accepted WU-003 motion + WU-004 Balanced-v1 + WU-001 encoder unchanged;
- station-mode Wi-Fi, bounded reconnect and multicast Audio Sync V2 at configurable/default `239.0.0.1:11988`, 50 Hz;
- strict live-send eligibility and sensor-failure re-probe/recalibration behavior;
- explicit sensor-independent diagnostic mode;
- one-second valid-sample/successful-send counters;
- no sender-LED dependency;
- ignored local credential/config workflow.

### Evidence boundary

WU-005 proves deterministic portable policy and compilation of the real Arduino sender runtime. It does not prove board-axis signs, physical sensor noise/clipping, achieved rates, RF/multicast reliability or stock-WLED visual response.

---

## WU-006 — End-to-end stock-WLED integration and mapping validation

**Issue:** #6  
**Implementation PR:** #13

### Delivered architecture

WU-006 completes the source/host integration layer without inventing unavailable hardware evidence:

- re-verifies WLED compatibility against unchanged `main` commit `06ae26db67107cb3f6a3d107a92340035991a063` and stable v16.0.1;
- retains the accepted Audio Sync V2 ABI/defaults and Balanced-v1 constants;
- adds explicit firmware/protocol/mapping identity to runtime diagnostics;
- expands bounded 1 Hz status to show IMU/calibration/feature state, motion energy, mapped level/peak/spectrum, generated/sent rates, Wi-Fi/local IP/target and error/reconnect counters;
- separates frame generation from network emission so Wi-Fi outage does not freeze mapper state;
- proves by regression that reconnect cannot replay an impact peak processed while offline;
- documents receiver absence, UDP loss/ordering posture and multi-receiver expectations without adding a proprietary reliability layer;
- provides a current stock-WLED setup/troubleshooting sequence and exact manual physical-validation checklist;
- records physical reference-board/receiver/effect/network rows as **pending** because that hardware is unavailable to the implementation environment;
- reconciles stale architecture/CI/provenance documentation, including the accepted Arduino `WiFiUDP::beginPacket(multicast_ip, port)` sender path.

### Mapping decision

The representative deterministic corpus covers stillness, tilted stillness, sway, roll, spin, shake, tap/impact, motion→stillness recovery and malformed input. Those regressions remain semantically coherent and byte-deterministic, so WU-006 does **not** retune Balanced-v1 from source/CI evidence alone. Perceptual multi-effect tuning remains a physical evidence item.

### Acceptance evidence rule

PR #13 may merge only after its exact final head passes the complete inherited formatting/native/tool/ESP32-S3 CI gate. The exact final head/run and merge result are recorded on PR #13 and issue #6 after they exist; this roadmap intentionally does not guess mutable merge evidence.

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

A new user can go from clean checkout to a running sender and stock-WLED receiver using repository documentation alone, while any still-unavailable physical compatibility rows remain labelled accurately rather than fabricated.

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
