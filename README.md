# WLEDIMUUDP

WLEDIMUUDP is a standalone **IMU → synthetic WLED Audio Sync V2 → stock WLED** bridge.

Current project release: **0.1.0**. The reference sender is a Waveshare ESP32-S3-Matrix with onboard QMI8658/QMI8658C. The sender does not require or initialise its RGB matrix, does not require a WLED fork/usermod, and does not capture microphone audio.

## What is implemented

The accepted WU-001 through WU-007 chain provides:

- explicit host-tested 44-byte WLED Audio Sync V2 (`00002\0`) encoder/decoder;
- deterministic host `send` / `listen` / `decode` probe tooling;
- deterministic IMU motion extraction with stationary calibration, stillness and impact semantics;
- versioned **Balanced-v1** motion→synthetic-spectrum mapping;
- Waveshare ESP32-S3-Matrix/QMI8658 reference adapter and board profile;
- Wi-Fi multicast output to configurable/default `239.0.0.1:11988` at 50 Hz;
- reconnect-safe mapper progression and explicit sensor-independent diagnostic mode;
- bounded 1 Hz end-to-end status across sensor, mapper and network stages;
- safe ignored local credential/configuration workflow;
- release/version convention, clean-checkout sanity tooling and reproducible CI artifacts.

WU-006 re-verified the WLED source contract on 2026-09-17 against `main` commit `06ae26db67107cb3f6a3d107a92340035991a063`; the latest stable release observed remained v16.0.1. Physical stock-WLED visual testing, board-axis confirmation, live sensor characterization and network-specific multicast behavior remain explicitly pending where recorded in `docs/rag/WU006_IMPLEMENTATION.md`.

## Clean checkout quick start

The reference development environment uses **Python 3.12**, **PlatformIO Core 6.1.18** and **clang-format 18.1.8**. PlatformIO pins Espressif32 `6.7.0` / Arduino-ESP32 `2.0.16` for the reference firmware.

Install the pinned development dependencies:

```powershell
python -m pip install -r requirements-dev.txt
python scripts/bootstrap.py --check
```

Create the local sender configuration without overwriting an existing one:

```powershell
python scripts/bootstrap.py --init-config
```

Edit `config/wifi.local.hpp`, not the committed template. At minimum replace `kWifiSsid` and `kWifiPassword`. The local file is ignored by Git.

Run the release/configuration sanity check and the complete build/test path:

```powershell
python scripts/release_check.py
python -m platformio test -e native
python -m platformio run -e host_probe
python -m platformio run -e mapping_probe
python -m platformio run -e esp32s3
```

## Flash and monitor the sender

Replace `<PORT>` with the board serial port:

```powershell
python -m platformio run -e esp32s3 -t upload --upload-port <PORT>
python -m platformio device monitor -p <PORT> -b 115200
```

At startup, keep the board stationary until calibration is accepted. Runtime serial commands are:

- `d` — diagnostic mode: generate/transmit a fixed known Audio Sync frame without live IMU input;
- `l` — return to live IMU mode;
- `r` — restart stationary calibration;
- `?` — print command help.

Runtime status reports `WLEDIMUUDP/0.1.0`, `AudioSync-V2/00002`, `Balanced-v1`, IMU/calibration state, motion/mapping summaries, generated/sent rates, Wi-Fi/local IP/target and error/reconnect counters. High-rate raw IMU logging is intentionally not part of the default release path.

## Configure stock WLED — prove the receiver first

Before debugging the IMU, prove the receiver/network path with the deterministic host probe.

Build the probe, then inspect a packet without sending anything:

```powershell
python -m platformio run -e host_probe
.\.pio\build\host_probe\program.exe send --dry-run --pattern single-band --frames 4
```

On Linux/macOS use `.pio/build/host_probe/program` instead of `program.exe`.

Configure stock WLED Audio Sync to **Receive**, normally on UDP port `11988`, on a LAN that permits multicast/client-to-client traffic. Then send known patterns:

```powershell
.\.pio\build\host_probe\program.exe send --pattern single-band --frames 320
.\.pio\build\host_probe\program.exe send --pattern peak-pulse --frames 80
.\.pio\build\host_probe\program.exe send --pattern ramp --frames 160
.\.pio\build\host_probe\program.exe send
```

If these fail, investigate receiver configuration, firewall/AP isolation, multicast/IGMP forwarding and packet presence **before** tuning motion behavior. Once host patterns work, use sender serial `d` to prove the firmware/network path, then `l` for live motion.

See `docs/rag/HOST_PROBE.md` for the full sender/listener/decoder and troubleshooting procedure.

## Configuration model

`config/wifi.example.hpp` is the safe committed template; `config/wifi.local.hpp` is the ignored local copy.

The normal user-local settings are:

- Wi-Fi SSID/password;
- multicast IPv4 address, default `239.0.0.1`;
- UDP port, default `11988`;
- packet rate, default/max intended `50 Hz`;
- reconnect interval, default `5000 ms`;
- diagnostic mode on boot, default off.

The first release deliberately keeps other semantics out of the credential file: **Balanced-v1** is the mapping default, WU-003 owns the accepted motion defaults, the reference board/axis transform is explicit in `kWaveshareEsp32S3Matrix`, calibration is automatic plus serial `r`, and diagnostics are a bounded 1 Hz summary. Porting the board/axes or changing mapping semantics requires corresponding tests/documentation rather than a hidden local tweak.

## Host inspection tools

Inspect representative Balanced-v1 output and the exact canonical packet:

```powershell
python -m platformio run -e mapping_probe
.\.pio\build\mapping_probe\program.exe spin
.\.pio\build\mapping_probe\program.exe tilt-left
.\.pio\build\mapping_probe\program.exe impact
```

Mapper presets are `still`, `sway`, `spin`, `shake`, `impact`, `tilt-left`, and `tilt-right`.

Decode the committed WU-001 golden packet:

```powershell
.\.pio\build\host_probe\program.exe decode --hex "30303030320000000000803f000020400100000102030405060708090a0b0c0d0e0f0000000080410000dc43"
```

## Protocol and mapping baseline

The project targets stock WLED Audio Sync V2:

- 44-byte payload;
- header `00002\0`;
- explicit little-endian IEEE-754 binary32 floats;
- `samplePeak` at offset 16;
- 16 GEQ bytes at offsets 18–33, each clamped to `0..254`;
- magnitude at offset 36;
- major peak at offset 40;
- default multicast `239.0.0.1:11988`;
- default/max intended cadence `50 Hz`.

The canonical encoder writes the byte layout explicitly. Motion/mapping code never duplicates the packet ABI. Balanced-v1 keeps translation, rotation, shake/jerk and impact/transient energy in four semantic four-band groups; orientation shapes existing energy without creating activity at rest.

## Release artifacts

CI preserves the full format/native/host-tool/ESP32 build gate, runs `scripts/release_check.py`, assembles a generated `dist/WLEDIMUUDP-0.1.0` bundle and uploads it as a GitHub Actions artifact.

The bundle contains the ESP32-S3 `firmware.bin`, Linux host/mapping probe binaries, safe configuration template, README/changelog/release docs and `MANIFEST.txt`. The manifest records the exact source revision, project/protocol/mapping/board identities and SHA-256 payload hashes. Generated `.pio/`, `dist/`, local credentials and virtual environments are never source-controlled.

See `docs/rag/RELEASE.md` for the version convention, bundle contract and release checklist.

## Authoritative documentation

The RAG pack under `docs/rag/` is the implementation authority below the active issue and `AGENTS.md`:

- `PROJECT.md` — scope and non-goals;
- `ARCHITECTURE.md` — boundaries/runtime data flow;
- `WLED_AUDIO_SYNC_V2.md` — packet compatibility contract;
- `HOST_PROBE.md` — stock-WLED host probe and troubleshooting;
- `MOTION_MAPPING.md` — motion + Balanced-v1 semantics;
- `HARDWARE.md` — reference hardware/profile;
- `TESTING_AND_CI.md` — automated/physical evidence rules;
- `SOURCES.md` — upstream source provenance;
- `WU005_IMPLEMENTATION.md` — sender hardware/runtime contract;
- `WU006_IMPLEMENTATION.md` — integration diagnostics and physical compatibility matrix;
- `RELEASE.md` — 0.1.0 setup, packaging, versioning and release checklist;
- `ROADMAP.md` / `ISSUE_INDEX.md` — accepted implementation campaign.

`CHANGELOG.md` records project release history. `VERSION` is the canonical project-version text consumed by release checks and packaging.
