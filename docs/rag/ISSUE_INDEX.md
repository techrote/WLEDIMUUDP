# WLEDIMUUDP — Implementation Issue Index

This file binds the reviewed roadmap IDs to the GitHub issues created from it.

| Roadmap ID | GitHub issue | Purpose | Depends on | Milestone record |
|---|---:|---|---|---|
| WU-001 | #1 | Repository bootstrap + exact Audio Sync V2 protocol substrate | — | accepted/closed |
| WU-002 | #2 | Host Audio Sync probe + interoperability harness | #1 | accepted/closed |
| WU-003 | #3 | Deterministic IMU motion feature core | #1, #2 by serial roadmap | accepted/closed |
| WU-004 | #4 | Motion → synthetic-audio mapper + Balanced profile | #1–#3 | accepted/closed |
| WU-005 | #5 | ESP32-S3 + QMI8658/QMI8658C reference sender | #1–#4 | accepted/closed; PR #12 merged |
| WU-006 | #6 | End-to-end stock-WLED integration, diagnostics and tuning | #1–#5 | implementation PR #13; final acceptance evidence lives on PR/issue |
| WU-007 | #7 | Release hardening, setup UX and reproducible packaging | #1–#6 | next serial milestone after accepted #6 |

## Execution rule

Each issue body contains its own autonomous implementation prompt, acceptance criteria, required tests and authoritative-document references.

The default execution model is serial: complete, test, PR, repair CI, merge, verify `main`, and close one issue before beginning the next. This keeps every later issue grounded in an accepted baseline.

Mutable exact-head CI and merge evidence belongs on the implementation PR/issue once it exists; RAG documents must describe the finished architecture without guessing future squash/merge SHAs.

## Scope guard

The issue chain implements only the standalone IMU → WLED Audio Sync V2 UDP bridge. ESP-NOW, ESPsand, sender LED effects, realtime RGB output and custom WLED receiver builds remain deferred/non-goals unless a future issue explicitly changes the project contract.
