# WLEDIMUUDP — Release, Setup and Packaging Contract

## Release baseline

Project version: **0.1.0**  
Release schema: **1**  
Protocol identity: **AudioSync-V2/00002**  
Mapping identity: **Balanced-v1**  
Reference board: **waveshare-esp32-s3-matrix**  
Reference IMU: **QMI8658/QMI8658C**

The project version follows semantic `MAJOR.MINOR.PATCH` versioning and is intentionally independent from the wire-protocol and mapper-profile versions. `VERSION`, `kProjectVersion`, `kFirmwareIdentity` and release checks must agree.

A source revision is recorded in the generated release `MANIFEST.txt`. Runtime firmware reports the project/protocol/mapping identities; source revision remains artifact/build provenance rather than a manually maintained source constant that would become stale after squash merges.

## Clean-checkout path

The reference CI/toolchain baseline is:

- Python **3.12**;
- PlatformIO Core **6.1.18**;
- clang-format **18.1.8**;
- Espressif32 PlatformIO platform **6.7.0**;
- Arduino-ESP32 **2.0.16** through that pinned platform.

From a clean checkout:

```powershell
python -m pip install -r requirements-dev.txt
python scripts/bootstrap.py --check
python scripts/bootstrap.py --init-config
```

Edit only `config/wifi.local.hpp`. Then run the full local verification/build path:

```powershell
python scripts/release_check.py
python -m platformio test -e native
python -m platformio run -e host_probe
python -m platformio run -e mapping_probe
python -m platformio run -e esp32s3
```

Flash and monitor the reference board, replacing `<PORT>`:

```powershell
python -m platformio run -e esp32s3 -t upload --upload-port <PORT>
python -m platformio device monitor -p <PORT> -b 115200
```

The standard PlatformIO commands remain authoritative; helper scripts are deliberately small and optional.

## Configuration surface

`config/wifi.example.hpp` is the committed safe template. `config/wifi.local.hpp` is ignored by Git and is the only normal user-local configuration file in the first release.

| Concern | Release 0.1.0 surface |
|---|---|
| Wi-Fi SSID/password | edit `kWifiSsid` / `kWifiPassword` in ignored `wifi.local.hpp` |
| multicast group | `kMulticastAddress`, default `239.0.0.1` |
| UDP port | `kMulticastPort`, default `11988` |
| packet cadence | `kPacketRateHz`, default/max intended `50 Hz` |
| reconnect cadence | `kReconnectIntervalMs`, default `5000 ms` |
| diagnostic mode on boot | `kDiagnosticModeOnBoot`, default `false` |
| mapping profile | fixed release default `Balanced-v1`; tunable constants remain in `MappingConfig`, not the credential file |
| motion profile | accepted WU-003 defaults; not a routine end-user knob surface |
| board / axis transform | fixed reference `kWaveshareEsp32S3Matrix`; ports must change the explicit board profile plus tests, not generic motion logic |
| calibration | automatic at startup/re-probe; serial `r` requests recalibration |
| runtime diagnostic verbosity | bounded 1 Hz summary is the release default; high-rate raw IMU logging is intentionally absent |

Do not add captive-portal/web provisioning merely to replace this intentionally narrow configuration model.

## First receiver test and troubleshooting order

The host Audio Sync probe is the **first troubleshooting step** because it removes live IMU/calibration from the problem.

1. Configure stock WLED Audio Sync to **Receive** on port `11988` unless intentionally using a matching alternate port.
2. Put host/sender and WLED on a LAN that permits client-to-client multicast.
3. Build `host_probe` and first run a deterministic dry run.
4. Send `single-band`, `peak-pulse`, `ramp`, then `scripted` patterns to the receiver.
5. If those fail, investigate WLED receive configuration, firewall/AP isolation, multicast/IGMP behavior and packet presence before touching motion tuning.
6. Once host patterns work, flash the sender, finish stationary calibration, use serial `d` for the firmware known-frame path, then `l` for live IMU mode.
7. Use `r` whenever a fresh stationary calibration is required.

`HOST_PROBE.md` and `WU006_IMPLEMENTATION.md` contain the detailed receiver procedure and the physical compatibility matrix.

## Runtime identity

Status output exposes:

- `WLEDIMUUDP/0.1.0` firmware/project identity;
- `AudioSync-V2/00002` protocol identity;
- `Balanced-v1` mapping version;
- board/IMU profile at startup;
- configured multicast destination and packet rate;
- bounded sensor/mapping/network state and counters.

The release artifact manifest records the exact Git source revision used to produce the binary. This avoids embedding a mutable branch/PR SHA in source files while preserving reproducible artifact provenance.

## CI release artifacts

After all existing format/test/tool/firmware gates pass, CI runs the release sanity check and creates a generated bundle under `dist/` (ignored by Git). The bundle contains:

```text
WLEDIMUUDP-0.1.0/
  MANIFEST.txt
  README.md
  CHANGELOG.md
  firmware/firmware.bin
  tools/host_probe
  tools/mapping_probe
  config/wifi.example.hpp
  docs/RELEASE.md
  docs/WU006_IMPLEMENTATION.md
```

On a non-Linux local host the host executable suffix may differ; the canonical GitHub Actions artifact is produced on Ubuntu and therefore contains Linux host utilities. The firmware binary remains the ESP32-S3 reference build.

`MANIFEST.txt` records project/release/protocol/mapping/board identities, exact source revision, default network target, physical-evidence warning and SHA-256 hashes for the executable/binary payloads.

Generated build output, `dist/`, `.pio/`, local credentials and virtual environments must never be committed.

## Release checks

`scripts/release_check.py` fails when:

- `VERSION` is not simple semantic version syntax;
- firmware project/identity constants diverge from `VERSION`;
- the Audio Sync protocol identity changes unexpectedly;
- the committed safe template loses its `CHANGE_ME` credential sentinels or accepted network defaults;
- `config/wifi.local.hpp` becomes tracked;
- generated `.pio/`, `dist/` or `.venv/` debris becomes tracked;
- the required release documentation is missing;
- `.gitignore` stops protecting local credentials or release output.

`scripts/bootstrap.py` is dependency-free. `--init-config` copies the safe template without overwriting an existing local configuration. `--check` confirms the pinned PlatformIO and clang-format packages are installed.

## Release checklist

Before accepting a release-hardening PR:

1. confirm `VERSION`, firmware identity and changelog agree;
2. run `scripts/release_check.py`;
3. run the complete native suite;
4. build/smoke both host tools;
5. build the ESP32-S3 firmware from a clean checkout with placeholder credentials;
6. assemble the release bundle and inspect its manifest/content;
7. confirm no local credentials or generated output are tracked;
8. verify stock-WLED source/provenance documentation is still current or re-verify if compatibility assumptions changed;
9. keep unavailable physical compatibility rows explicitly pending;
10. merge only after the exact final PR head passes every required automated gate, then verify the merge on `main`.

## Physical compatibility boundary

Release packaging does not upgrade source/CI evidence into physical evidence. The current physical matrix remains in `WU006_IMPLEMENTATION.md`. Stock-WLED effect behavior, real QMI8658 rates/noise/axes, real reconnect behavior and multi-receiver multicast remain pending until observed and recorded against exact hardware/software revisions.
