# WLEDIMUUDP — Reviewed Implementation Roadmap

## Planning objective

Build the smallest trustworthy path from physical IMU motion to expressive **stock WLED Audio Reactive behavior** over WLED Audio Sync V2 UDP, while keeping packet compatibility, motion semantics, hardware integration and release packaging independently testable.

## Plan review findings

The reviewed roadmap proves one boundary at a time:

1. protocol truth before hardware;
2. host interoperability tooling as an early product asset;
3. motion extraction separated from fake-spectrum design;
4. static tilt shapes response but never creates activity;
5. sender LEDs are irrelevant to the product;
6. UDP Audio Sync V2 is the product transport/receiver contract;
7. automated conformance is not physical hardware validation;
8. release convenience must not hide configuration, provenance or evidence boundaries.

## Dependency chain

```text
WU-001 protocol/core/CI bootstrap
    -> WU-002 host Audio Sync probe
    -> WU-003 deterministic motion core
    -> WU-004 Balanced-v1 mapper
    -> WU-005 ESP32-S3 + QMI8658 sender
    -> WU-006 integration/diagnostics/validation
    -> WU-007 release hardening/packaging
```

WU-007 completes the initial reviewed implementation chain. Future work should begin from the accepted release architecture rather than reopening earlier boundaries without evidence.

---

## WU-001 — Repository bootstrap and Audio Sync V2 protocol substrate

**Issue:** #1  
**Implementation PR:** #8  
**Status:** accepted/closed; merged as `864242b056830060607592c092c9bce1a9cebdbe`.

Delivered the portable explicit 44-byte Audio Sync V2 encoder/strict decoder, golden fixture, pinned toolchain, safe configuration posture and initial CI/native/ESP32 build substrate.

---

## WU-002 — Host Audio Sync probe and interoperability harness

**Issue:** #2  
**Implementation PR:** #9  
**Status:** accepted/closed; merged as `181d1a108a8887bc74008b545b94368fe10725b5`.

Delivered deterministic host patterns, `send`/`listen`/`decode`, configurable multicast/rate, exact-byte UDP coverage and stock-WLED receive/troubleshooting guidance.

---

## WU-003 — Deterministic IMU motion feature core

**Issue:** #3  
**Implementation PR:** #10  
**Status:** accepted/closed; merged as `8ef17a102624fff5c9aa26d372d19d4fd04a9638`.

Delivered calibrated timestamped motion processing, gravity separation, jerk/angular activity, stillness, bounded impact semantics and the reusable deterministic trace corpus.

---

## WU-004 — Motion-to-synthetic-audio mapper

**Issue:** #4  
**Implementation PR:** #11  
**Status:** accepted/closed; merged as `75a1e4f988d08905574001e8a6378a0f1120be3e`.

Delivered **Balanced-v1**, four semantic spectrum groups, orientation shaping without false activity, coherent magnitude/major peak, one-shot peak semantics, mapper→encoder regressions and mapper inspection tooling.

---

## WU-005 — ESP32-S3 + QMI8658/QMI8658C reference sender

**Issue:** #5  
**Implementation PR:** #12  
**Status:** accepted/closed; merged as `41574f11b71f82ee8a2a9dd91f88bc3f12b81c14`.

Delivered the explicit Waveshare ESP32-S3-Matrix/QMI8658 profile and adapter, startup calibration, monotonic scheduling, Wi-Fi multicast transport, bounded reconnect behavior, strict live gating, diagnostic mode, secret-safe local configuration and the real ESP32-S3 firmware build gate.

Accepted WU-005 exact-head CI contained **57/57 native tests** plus both host-tool gates and the ESP32-S3 build.

---

## WU-006 — End-to-end integration, diagnostics and mapping validation

**Issue:** #6  
**Implementation PR:** #13  
**Status:** accepted/closed; merged as `9c86bb9b1d7cac3009534b6b2678ffd85796e767`.

Delivered:

- source-level WLED compatibility re-verification against `main` `06ae26db67107cb3f6a3d107a92340035991a063` and stable v16.0.1;
- bounded end-to-end runtime observability across sensor, mapping and network boundaries;
- separate frame-generation and network-emission gates so Wi-Fi loss cannot freeze mapper state;
- regression proof that reconnect cannot replay a stale impact peak;
- receiver absence/loss/order/multicast assumptions documented without proprietary reliability extensions;
- current stock-WLED setup/troubleshooting and explicit manual physical-validation procedure;
- retained Balanced-v1 constants because deterministic evidence did not justify speculative retuning;
- physical board/receiver/effect/network rows explicitly left pending where hardware was unavailable.

Final PR head `065fe53f8d9fd6bb09c8db6cdecf9155d4a13276` passed CI run `35170559063`: formatting, **61/61 native tests**, both host-tool build/smoke gates and ESP32-S3 firmware build.

---

## WU-007 — Release hardening, setup UX and reproducible packaging

**Issue:** #7  
**Implementation PR:** #14

WU-007 turns the accepted engineering baseline into project release **0.1.0** without redesigning protocol, motion or mapping semantics.

### Delivered release architecture

- canonical root `VERSION` plus firmware `WLEDIMUUDP/0.1.0` identity and release-schema version;
- semantic project-version convention kept independent from `AudioSync-V2/00002` and `Balanced-v1`;
- dependency-free `scripts/bootstrap.py` for safe, non-overwriting local config initialization and pinned dependency checks;
- `scripts/release_check.py` for version/config/template/ignore/generated-debris sanity;
- clean-checkout README with exact install, test, build, flash, monitor and host-probe-first troubleshooting sequence;
- explicit first-release configuration surface and fixed-versus-user-configurable distinctions;
- `CHANGELOG.md` plus `docs/rag/RELEASE.md` release/package/checklist contract;
- deterministic release-bundle builder containing firmware, Linux host tools, safe config and documentation;
- manifest carrying exact source revision, independent protocol/mapping/board identities and SHA-256 payload hashes;
- CI release artifact upload only after the inherited format/native/tool/ESP32 build gates succeed;
- generated `.pio/` / `dist/` output and local credentials excluded from source control;
- physical compatibility boundaries retained exactly rather than promoted by packaging.

### Acceptance evidence rule

Exact final PR head, final CI run, artifact inventory/hash evidence, merge result and issue closure belong on PR #14 / issue #7 once the exact merge candidate passes. This roadmap deliberately avoids guessing a mutable squash SHA.

---

## Deferred backlog

Outside the initial release unless a future issue changes scope:

- captive-portal/web provisioning;
- additional IMU adapters;
- battery/deep-sleep optimisation;
- sophisticated gesture classifiers/ML;
- extra mapping profiles beyond demonstrated need;
- WLED realtime-RGB output;
- ESP-NOW;
- ESPsand integration;
- WLED fork/usermod.

## Post-roadmap rule

Any future change should preserve the established evidence hierarchy: deterministic host-testable core behavior, explicit platform adapters, stock-WLED protocol compatibility, safe local configuration, reproducible packaging, and a strict distinction between automated/source evidence and physical validation.
