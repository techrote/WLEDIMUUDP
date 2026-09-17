# Changelog

WLEDIMUUDP uses semantic project versions (`MAJOR.MINOR.PATCH`) independently from the WLED Audio Sync protocol version and the motion-mapping profile version.

- **MAJOR**: incompatible project-level configuration/runtime contract change.
- **MINOR**: backward-compatible functionality, supported hardware/profile expansion, or materially new release capability.
- **PATCH**: backward-compatible fixes, documentation, diagnostics, packaging, or tuning that does not change the declared protocol/profile contract.

Protocol and mapper identities are reported separately because a project release can change without changing `AudioSync-V2/00002` or `Balanced-v1`.

## 0.1.0 — 2026-09-17

Initial hardened release baseline.

- exact, host-tested WLED Audio Sync V2 encoder/decoder and deterministic host probe;
- deterministic IMU motion feature core and Balanced-v1 synthetic spectrum mapping;
- Waveshare ESP32-S3-Matrix + QMI8658/QMI8658C reference sender;
- Wi-Fi multicast sender with reconnect-safe mapper progression and bounded diagnostics;
- safe ignored local credential/configuration workflow;
- stock-WLED receiver/troubleshooting procedure and explicit pending physical compatibility matrix;
- clean-checkout bootstrap/release checks;
- reproducible CI release bundle containing firmware, Linux host tools, safe configuration template, manifest and release documentation.

Physical stock-WLED effect testing, live sensor characterization, board-axis confirmation and network-specific multicast observations remain pending evidence where marked in `docs/rag/WU006_IMPLEMENTATION.md`; they are not implied by this release version.
