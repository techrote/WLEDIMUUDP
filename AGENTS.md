# AGENTS.md

## Authority

For implementation work in this repository, use the following authority order:

1. the active GitHub issue and its acceptance criteria;
2. this `AGENTS.md`;
3. the current files under `docs/rag/`;
4. accepted implementations already merged to `main`;
5. upstream WLED source/documentation explicitly referenced by the RAG pack.

If two documents conflict, reconcile the conflict in the same PR rather than silently choosing one.

## Project invariant

WLEDIMUUDP is a standalone **IMU → synthetic WLED Audio Sync V2 → stock WLED** bridge.

Do not turn it into:

- an ESPsand component;
- a WLED fork or custom WLED receiver build;
- an ESP-NOW transport project;
- an LED driver for the sender board;
- a microphone/audio-capture project;
- a generic IoT framework.

The sender's own LEDs, if any, are irrelevant to correctness.

## Architecture rules

- Keep protocol encoding independent from IMU acquisition and motion mapping.
- Keep motion feature extraction independent from synthetic-audio mapping.
- Prefer pure, deterministic, host-testable C++ for protocol and signal-processing logic.
- Isolate Arduino/ESP-IDF/PlatformIO/Wi-Fi/IMU dependencies behind narrow adapters.
- Do not use hidden global randomness in mapping or tests.
- Do not allocate dynamically in the steady-state motion/packet hot path unless profiling and documentation justify it.
- Do not transmit native C/C++ structs by blind `reinterpret_cast`/`memcpy` as the canonical encoder. Encode the documented V2 byte layout explicitly so offsets, endianness and reserved bytes are reviewable and host-testable.
- Never commit Wi-Fi credentials or other secrets. Provide examples/templates only.

## WLED compatibility rules

- Stock WLED receive compatibility is a core acceptance requirement.
- The authoritative V2 packet layout is documented in `docs/rag/WLED_AUDIO_SYNC_V2.md` and must be backed by a pinned upstream WLED source reference.
- Reserved packet bytes must be zero unless a future, explicitly documented WLED protocol revision says otherwise.
- Any compatibility claim must identify the WLED source/release tested or inspected.
- Hardware/manual observations must be labelled as such; never fabricate physical validation.

## Development workflow

For every implementation issue:

1. read the issue, this file, and every RAG document referenced by the issue;
2. inspect current `main` and accepted prerequisite work;
3. implement the complete issue rather than only scaffolding;
4. add or update automated tests and documentation;
5. run the full required local/native/firmware verification available to you;
6. open an issue-linked PR with a concise implementation and evidence summary;
7. repair CI failures rather than bypassing checks;
8. merge only after all required automated checks pass;
9. verify the merge commit actually landed on `main`;
10. close the issue only when its acceptance criteria are genuinely satisfied.

The user has authorised autonomous PR creation and merge after required automated checks are green. Do not claim hardware acceptance when only compilation/simulation was possible.

## CI and quality expectations

- Treat warnings in project code as defects unless explicitly justified.
- Keep formatting deterministic and CI-enforced.
- Protocol tests must include exact golden bytes, field offsets/size, malformed-input behavior and deterministic repeatability.
- Motion tests must use fixed synthetic traces such as stillness, tilt, shake, tap, slow roll and fast spin.
- Network tests must not depend on public internet access.
- Firmware builds must remain reproducible from repository instructions.

## Documentation discipline

Update the relevant RAG documents in the same PR whenever implementation changes a contract, packet interpretation, mapping semantic, configuration default, hardware assumption, test gate or roadmap state.