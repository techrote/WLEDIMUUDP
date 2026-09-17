# WLEDIMUUDP — Implementation Issue Index

This file binds the reviewed roadmap IDs to the GitHub issues created from it.

| Roadmap ID | GitHub issue | Purpose | Depends on | Milestone record |
|---|---:|---|---|---|
| WU-001 | #1 | Repository bootstrap + exact Audio Sync V2 protocol substrate | — | accepted/closed |
| WU-002 | #2 | Host Audio Sync probe + interoperability harness | #1 | accepted/closed |
| WU-003 | #3 | Deterministic IMU motion feature core | #1, #2 by serial roadmap | accepted/closed |
| WU-004 | #4 | Motion → synthetic-audio mapper + Balanced profile | #1–#3 | accepted/closed |
| WU-005 | #5 | ESP32-S3 + QMI8658/QMI8658C reference sender | #1–#4 | accepted/closed; PR #12 merged |
| WU-006 | #6 | End-to-end stock-WLED integration, diagnostics and tuning | #1–#5 | accepted/closed; PR #13 merged as `9c86bb9b1d7cac3009534b6b2678ffd85796e767` |
| WU-007 | #7 | Release hardening, setup UX and reproducible packaging | #1–#6 | implementation PR #14; exact final acceptance evidence belongs on PR/issue |

## Execution rule

Each issue body contains its own autonomous implementation prompt, acceptance criteria, required tests and authoritative-document references.

The default execution model is serial: complete, test, PR, repair CI, merge, verify `main`, and close one issue before beginning the next. WU-007 is the final issue in the initial reviewed chain.

Mutable exact-head CI and merge evidence belongs on the implementation PR/issue once it exists; RAG documents describe the finished architecture without guessing future squash/merge SHAs.

## Scope guard

The initial issue chain implements only the standalone IMU → WLED Audio Sync V2 UDP bridge. ESP-NOW, ESPsand, sender LED effects, realtime RGB output and custom WLED receiver builds remain deferred/non-goals unless a future issue explicitly changes the project contract.
