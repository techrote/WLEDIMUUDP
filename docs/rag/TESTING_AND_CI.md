# WLEDIMUUDP — Testing, CI and Release-Evidence Contract

## Purpose

The project must remain diagnosable without conflating packet ABI, motion semantics, mapping, hardware integration, networking and packaging. Automated evidence is never promoted into a physical-hardware compatibility claim.

## Required automated gates

The release baseline requires, in order:

1. pinned development dependencies from `requirements-dev.txt`;
2. bootstrap dependency sanity (`scripts/bootstrap.py --check`);
3. release/configuration/credential hygiene (`scripts/release_check.py`);
4. deterministic C/C++ formatting check;
5. complete native protocol/host/motion/mapping/firmware-support tests;
6. warning-as-error host Audio Sync probe build plus real dry-run smoke command;
7. warning-as-error mapping probe build plus smoke command;
8. reference ESP32-S3 firmware build against the pinned Arduino Wi-Fi/Wire APIs;
9. deterministic release-bundle assembly;
10. GitHub Actions artifact upload with missing-artifact failure enabled.

An implementation or release PR must not weaken an inherited gate merely to merge.

## Pinned toolchain

Reference CI uses:

- Python `3.12`;
- PlatformIO Core `6.1.18`;
- clang-format `18.1.8`;
- `espressif32@6.7.0`;
- Arduino-ESP32 `2.0.16` from that platform;
- C++17 project code;
- native project warnings `-Wall -Wextra -Wpedantic -Werror`.

The embedded target suppresses only the documented third-party pedantic extension noise inherited from Arduino/ESP-IDF headers while retaining normal project warning coverage.

## Accepted baseline through WU-006

WU-001 established exact Audio Sync V2 protocol tests and the initial ESP32 compile gate. WU-002 added deterministic host patterns/CLI/decode and real localhost exact-byte UDP coverage. WU-003 added deterministic motion-feature traces. WU-004 added Balanced-v1 mapper tests and the mapping probe. WU-005 added board/sensor/calibration/scheduling/network-state tests and upgraded the embedded gate to compile the real sender. WU-006 added end-to-end observability helpers and reconnect-state safety regressions.

The authoritative WU-006 merge candidate was PR #13 head `065fe53f8d9fd6bb09c8db6cdecf9155d4a13276`. CI run `35170559063` completed successfully with:

- formatting green;
- **61 native test cases: 61 succeeded, 0 failed**;
- host probe build/smoke green;
- mapping probe build/smoke green;
- ESP32-S3 reference firmware build green;
- 24,772 bytes reported RAM use;
- 374,837 bytes reported flash program use.

PR #13 merged as `9c86bb9b1d7cac3009534b6b2678ffd85796e767`; issue #6 closed completed.

The 61-test native baseline comprises:

- 10 protocol tests;
- 10 host/transport tests;
- 11 motion tests;
- 13 mapping tests;
- 17 firmware/integration-support tests.

## WU-007 release-hardening gates

WU-007 preserves all 61 native tests and every inherited host/embedded gate. It adds release checks outside the Unity count rather than manufacturing unit-test count for filesystem/packaging behavior.

### Bootstrap check

`scripts/bootstrap.py --check` must confirm the exact pinned PlatformIO and clang-format Python packages are installed. The same helper can create `config/wifi.local.hpp` from the safe template with `--init-config`, but must never overwrite an existing local file.

### Release and credential hygiene

`scripts/release_check.py` must verify:

- root `VERSION` uses `MAJOR.MINOR.PATCH` syntax;
- firmware project identity matches `VERSION`;
- `AudioSync-V2/00002` identity remains explicit;
- the committed configuration template still contains `CHANGE_ME` SSID/password sentinels and accepted network defaults;
- `config/wifi.local.hpp` is not tracked;
- `.pio/`, `dist/` and `.venv/` generated output is not tracked;
- `.gitignore` protects local credentials and generated release output;
- README/changelog/release/integration documents required by packaging exist.

This is a practical repository sanity check, not a general-purpose secret scanner. Real credentials remain forbidden by repository policy.

### Release bundle

After all builds succeed, `scripts/make_release_bundle.py` assembles a generated `dist/WLEDIMUUDP-<version>/` tree containing:

- ESP32-S3 `firmware.bin`;
- built host Audio Sync probe;
- built mapping probe;
- safe configuration template;
- README and changelog;
- release contract and WU-006 compatibility/evidence record;
- `MANIFEST.txt`.

The manifest records the exact Git source revision supplied by CI, project/release schema, protocol, mapping, board/IMU, default network target, explicit physical-evidence warning and SHA-256 hashes of executable/binary payloads.

GitHub Actions uploads the complete bundle through `actions/upload-artifact`; missing bundle content is a CI failure. The canonical CI host tools are Linux binaries because the release workflow runs on Ubuntu. PlatformIO remains the portable source/build route for other host operating systems.

## Protocol tests

Audio Sync V2 coverage locks exact 44-byte size/header/reserved layout, binary32 offsets, peak/band offsets, `0..254` band clamp, deterministic non-finite sanitisation, no uninitialised output bytes, golden round trip, repeated byte identity and malformed decoder rejection.

`WledImuUdpCore` remains the only canonical serializer/decoder.

## Host packet tooling tests

Host coverage locks deterministic patterns, encode/decode agreement, destination/port/rate parsing, malformed arguments, exact packet hex conversion, scripted ordering and a real localhost UDP datagram with exact canonical bytes.

The host probe is also the first physical receiver/network troubleshooting seam: deterministic traffic is proved before involving live IMU state.

## Motion and mapping tests

The shared deterministic corpus covers stationary level/tilted, slow roll, translational sway, constant spin, tap/impact, shake, motion→stillness and malformed/non-finite inputs.

Motion invariants include static tilt without false activity, rotation/translation separation, bounded impact/refractory behavior, stillness decay, stale/invalid rejection, calibration, deterministic replay and timing-jitter resilience.

Balanced-v1 invariants include near-silent stillness, orientation shaping without false energy, semantic band-group separation, raw/smoothed level behavior, one-shot peak semantics, bounded bands, coherent magnitude/major peak, deterministic configuration and exact encoder interoperability.

WU-006 retained Balanced-v1 unchanged because no deterministic evidence justified retuning without physical stock-WLED effect observations.

## Firmware and integration tests

Portable firmware-support coverage locks:

- release/protocol identity;
- Waveshare board pins/bus/address and explicit axis transform;
- QMI8658 configuration/scaling and raw decode;
- startup calibration acceptance/rejection;
- multicast defaults and packet eligibility;
- frame generation independent from Wi-Fi emission;
- offline mapper progression preventing stale peak replay on reconnect;
- bounded reconnect scheduling;
- `micros()` wrap extension;
- fixed-rate scheduling without catch-up bursts;
- bounded spectrum summaries;
- deterministic canonical diagnostic-frame encoding.

`src/qmi8658_adapter.*` contains real Arduino `TwoWire` transactions and remains compile-gated by the ESP32-S3 target. Native tests validate its pure register/decode contract, not a physical bus.

## Firmware build evidence

The embedded CI target compiles the complete reference sender from a clean checkout with placeholder credentials:

- QMI8658 adapter;
- calibration lifecycle;
- motion + Balanced-v1 mapping;
- canonical V2 encoder;
- station Wi-Fi + multicast UDP;
- reconnect-safe frame/send behavior;
- bounded serial diagnostics;
- diagnostic mode;
- no sender-LED dependency.

`CHANGE_ME` credentials intentionally suppress connection attempts but compile the same firmware path.

## Network and stock-WLED evidence

Core CI cannot prove multicast delivery across arbitrary LAN/AP infrastructure. Audio Sync V2 has no WLEDIMUUDP-owned acknowledgement, retransmission or reordering layer. Receiver absence safely leaves sender state running; loss reduces temporal sampling; multiple receivers require only multicast forwarding from the network, not sender-side sessions.

WU-006 re-verified source compatibility on 2026-09-17 against WLED `main` `06ae26db67107cb3f6a3d107a92340035991a063` and stable v16.0.1. The implementation environment had no physical receiver/reference sender, so the detailed matrix in `WU006_IMPLEMENTATION.md` remains explicitly pending.

Release 0.1.0 packaging does not change those evidence statuses.

## Generated-output rule

Do not commit:

- `.pio/` build trees;
- `dist/` release bundles;
- `.venv/` environments;
- `config/wifi.local.hpp` or other real credentials;
- downloaded/generated binaries that CI can reproduce.

The CI artifact is a delivery product, not source-of-truth source code.

## Merge evidence

Every implementation/release PR must state the exact final head, CI run, native count, host-tool gates, ESP32 firmware gate, release-sanity/bundle result, warnings observed, artifact inventory, physical validation performed or explicitly unavailable, and documentation reconciliation.

A green automated gate authorises merge under the repository workflow but must never be described as physical stock-WLED/QMI8658/network validation when it is not.
