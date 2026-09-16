# WLEDIMUUDP

WLEDIMUUDP is a standalone **IMU → WLED Audio Sync V2 UDP bridge**.

Its purpose is deliberately narrow: an ESP32-class sender reads an IMU, converts motion into a synthetic audio-reactive control stream, and transmits packets that **stock WLED** can consume through its existing Audio Sync receive path. The sender does not require LEDs, ESPsand, a custom WLED build, or a WLED usermod.

## Current implementation state

WU-001 through WU-005 establish the complete reference-sender path:

- explicit host-testable WLED Audio Sync V2 encoder and strict decoder;
- direct-review 44-byte golden fixture;
- deterministic host Audio Sync patterns, send/listen/decode tooling and localhost UDP coverage;
- deterministic sensor-independent motion feature extraction with calibration primitives and reusable synthetic traces;
- versioned **Balanced-v1** motion→synthetic-audio mapping;
- four semantic spectrum groups for translation, rotation, shake/jerk and impacts;
- orientation-dependent spectral shaping without false activity at rest;
- native mapper inspection tooling with exact packet output;
- explicit Waveshare ESP32-S3-Matrix + QMI8658/QMI8658C reference-board profile;
- direct QMI8658 I2C adapter with startup calibration and failure re-probe;
- live motion→mapping→canonical 44-byte encoder→Wi-Fi multicast firmware path;
- secret-safe station-mode Wi-Fi configuration and bounded reconnect behavior;
- sensor-independent known-frame diagnostic mode for separating network/WLED faults from IMU faults;
- deterministic formatting, native tests and reference ESP32-S3 CI build.

Physical stock-WLED effect validation, board-axis confirmation and perceptual mapping refinement remain WU-006 evidence/tuning work. Green CI is not a physical hardware claim.

## Quick start

Use Python 3.12 or another version supported by the pinned PlatformIO release.

```powershell
python -m pip install -r requirements-dev.txt
python -m platformio test -e native
python -m platformio run -e host_probe
python -m platformio run -e mapping_probe
python -m platformio run -e esp32s3
```

### Configure the reference sender

Copy the safe template and edit only the ignored local copy:

```powershell
Copy-Item config\wifi.example.hpp config\wifi.local.hpp
notepad config\wifi.local.hpp
```

Set `kWifiSsid` and `kWifiPassword`. The default sender target is stock WLED Audio Sync multicast `239.0.0.1:11988` at 50 packets/s; those values are also configurable in the local header. Never commit `config/wifi.local.hpp`.

The WU-005 reference hardware is the Waveshare ESP32-S3-Matrix with onboard QMI8658/QMI8658C. The firmware uses GPIO11 SDA, GPIO12 SCL, 400 kHz I2C and board address `0x6B`. It does **not** initialise or depend on the onboard 8×8 RGB matrix.

### Build, flash and monitor

Replace `<PORT>` with the board's serial port:

```powershell
python -m platformio run -e esp32s3
python -m platformio run -e esp32s3 -t upload --upload-port <PORT>
python -m platformio device monitor -p <PORT> -b 115200
```

At startup the sender probes/configures the IMU and asks for a stationary calibration window. Keep the board still until serial reports `Calibration accepted`. The firmware reports one-second IMU sample and successful-packet counts so physical scheduling can be verified rather than assumed.

Runtime serial commands:

- `d` — diagnostic mode: transmit a fixed known synthetic Audio Sync frame without requiring a healthy/calibrated IMU;
- `l` — return to live IMU mode;
- `r` — restart stationary calibration;
- `?` — print the command summary.

Live transmission is automatically paused on sensor failure, invalid calibration, missing/currently invalid motion features, or Wi-Fi disconnection. Wi-Fi reconnect does not reset accepted motion/calibration state; a sensor failure does require re-probe and fresh calibration.

See [`docs/rag/WU005_IMPLEMENTATION.md`](docs/rag/WU005_IMPLEMENTATION.md) for the exact QMI8658 register/range/rate choices and evidence boundary.

## Host compatibility tools

On Windows, exercise the network host probe without sending packets:

```powershell
.\.pio\build\host_probe\program.exe send --dry-run --pattern single-band --frames 4
```

Inspect representative Balanced-v1 mapper output and the exact 44-byte packet without IMU hardware:

```powershell
.\.pio\build\mapping_probe\program.exe spin
.\.pio\build\mapping_probe\program.exe tilt-left
.\.pio\build\mapping_probe\program.exe impact
```

Mapper presets are `still`, `sway`, `spin`, `shake`, `impact`, `tilt-left`, and `tilt-right`.

Send the default 160-frame scripted Audio Sync sequence to stock WLED's default multicast destination:

```powershell
.\.pio\build\host_probe\program.exe send
```

Decode the committed golden packet:

```powershell
.\.pio\build\host_probe\program.exe decode --hex "30303030320000000000803f000020400100000102030405060708090a0b0c0d0e0f0000000080410000dc43"
```

See [`docs/rag/HOST_PROBE.md`](docs/rag/HOST_PROBE.md) for network probe patterns/listening/receiver setup, and [`docs/rag/WU004_IMPLEMENTATION.md`](docs/rag/WU004_IMPLEMENTATION.md) for the locked Balanced-v1 mapping contract.

## Protocol baseline

The project targets WLED Audio Sync V2 compatibility:

- 44-byte payload;
- header `00002\0`;
- explicit little-endian IEEE-754 binary32 floats;
- `samplePeak` at offset 16;
- 16 GEQ bytes at offsets 18–33, clamped to `0..254`;
- magnitude at offset 36;
- major peak at offset 40;
- default multicast destination `239.0.0.1:11988`;
- default/max intended sender cadence 50 Hz.

The canonical encoder writes the wire layout explicitly; it does not transmit a native packed C++ struct. Motion/mapping code produces semantic frames and does not duplicate the packet ABI.

## Credentials

`config/wifi.example.hpp` is the committed configuration template. Copy it to `config/wifi.local.hpp`; that local file is ignored by Git. A clean checkout deliberately compiles with placeholder credentials but makes no Wi-Fi connection attempt until a real local SSID is configured.

## Authoritative documentation

The project RAG pack lives under [`docs/rag/`](docs/rag/):

- [`PROJECT.md`](docs/rag/PROJECT.md) — scope, goals, non-goals, terminology.
- [`ARCHITECTURE.md`](docs/rag/ARCHITECTURE.md) — module boundaries and runtime data flow.
- [`WLED_AUDIO_SYNC_V2.md`](docs/rag/WLED_AUDIO_SYNC_V2.md) — protocol compatibility contract.
- [`HOST_PROBE.md`](docs/rag/HOST_PROBE.md) — WU-002 host sender/listener/decoder and receiver setup.
- [`MOTION_MAPPING.md`](docs/rag/MOTION_MAPPING.md) — accepted motion and Balanced-v1 synthetic-audio semantics.
- [`HARDWARE.md`](docs/rag/HARDWARE.md) — reference hardware and portability rules.
- [`TESTING_AND_CI.md`](docs/rag/TESTING_AND_CI.md) — automated verification and evidence rules.
- [`ROADMAP.md`](docs/rag/ROADMAP.md) — reviewed implementation sequence and accepted milestones.
- [`SOURCES.md`](docs/rag/SOURCES.md) — pinned upstream WLED and hardware provenance.
- [`ISSUE_INDEX.md`](docs/rag/ISSUE_INDEX.md) — implementation issue/dependency index.
- [`WU001_IMPLEMENTATION.md`](docs/rag/WU001_IMPLEMENTATION.md) — WU-001 bootstrap/protocol choices.
- [`WU003_IMPLEMENTATION.md`](docs/rag/WU003_IMPLEMENTATION.md) — WU-003 motion-core contract.
- [`WU004_IMPLEMENTATION.md`](docs/rag/WU004_IMPLEMENTATION.md) — WU-004 Balanced-v1 mapping contract.
- [`WU005_IMPLEMENTATION.md`](docs/rag/WU005_IMPLEMENTATION.md) — WU-005 board, sensor, scheduling and transport contract.

[`AGENTS.md`](AGENTS.md) defines the repository-wide rules for autonomous implementation work.
