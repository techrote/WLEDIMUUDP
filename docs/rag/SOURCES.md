# WLEDIMUUDP — Upstream Source Provenance

This file records the external sources used to establish and re-verify compatibility and hardware contracts so future implementation agents can check assumptions rather than relying on conversational history.

## WLED Audio Reactive source

Originally verified on 2026-09-15 and re-verified for WU-002 on **2026-09-16** against WLED `main` commit:

`06ae26db67107cb3f6a3d107a92340035991a063`

Primary implementation source:

- https://github.com/wled/WLED/blob/06ae26db67107cb3f6a3d107a92340035991a063/usermods/audioreactive/audio_reactive.cpp

Facts used by this repository include:

- Audio Sync V2 packed payload is 44 bytes;
- V2 header is `00002` plus terminating zero in the six-byte header field;
- field offsets documented in `WLED_AUDIO_SYNC_V2.md` match the upstream packed structure;
- WLED’s transmitter constrains the 16 `fftResult` bytes to `0..254`;
- current transmitter sends the packed structure directly as bytes;
- default Audio Sync UDP port is `11988`;
- ESP32 receive path joins multicast `239.0.0.1`;
- Audio Sync receive mode disables local sound processing in the relevant receive configuration;
- WLED includes a network-only sound/input mode.

## Release/documentation snapshot for WU-002

On 2026-09-16 the latest stable GitHub release observed was **WLED v16.0.1**. This is a documentation/source compatibility reference, **not** a physical receiver-validation claim.

Official WLED documentation consulted:

- Audio Reactive overview: https://kno.wled.ge/advanced/audio-reactive/
- UDP Realtime / Sound Sync: https://kno.wled.ge/interfaces/udp-realtime/
- Main WLED repository: https://github.com/wled/WLED

The Sound Sync documentation states that external senders may send more slowly than the approximately 20 ms cadence but should not send faster. WU-002 therefore locks the host probe to a configurable `1..50 Hz` range with 50 Hz default/max.

## WU-005 reference sender hardware provenance

Re-verified on **2026-09-16** for the first live sender implementation.

Primary Waveshare sources:

- ESP32-S3-Matrix product/wiki documentation: https://docs.waveshare.com/ESP32-S3-Matrix
- Waveshare resources/downloads page: https://docs.waveshare.com/ESP32-S3-Matrix/Resources-And-Documents
- official ESP32-S3-Matrix schematic: https://files.waveshare.com/wiki/ESP32-S3-Matrix/ESP32-S3-Matrix-Sch.pdf
- Waveshare-hosted QMI8658A datasheet: https://files.waveshare.com/wiki/common/QMI8658A_Datasheet_Rev_A.pdf
- Waveshare-hosted QMI8658C datasheet: https://files.waveshare.com/wiki/common/QMI8658C.pdf

The schematic establishes the reference board wiring used by `kWaveshareEsp32S3Matrix`:

- IMU SDA GPIO11;
- IMU SCL GPIO12;
- IMU INT1 GPIO10;
- IMU INT2 GPIO13;
- onboard device marked QMI8658C.

Board-specific example ecosystems consistently use QMI8658 address `0x6B`; the WU-005 profile locks `0x6B` and retains `0x6A` only as a secondary generic-family probe. The committed software contract does not depend on sender LEDs.

QMI8658 register facts used by WU-005 are derived from the manufacturer datasheet family and cross-checked against established open-source drivers:

- `WHO_AM_I` register `0x00`, expected value `0x05`;
- `CTRL1` bit 6 enables address auto-increment and bit 5 controls output endian;
- `CTRL2` full-scale code `010` selects ±8 g;
- `CTRL3` full-scale code `110` selects ±1024 dps;
- ODR code `0101` is 224.2 Hz effective in 6-DoF mode on current A/C-family documentation;
- `CTRL7` bits 1:0 enable gyro + accel;
- accel/gyro output block begins at `AX_L` `0x35` and spans 12 bytes through `GZ_H`;
- sensitivities used are 4096 LSB/g at ±8 g and 32 LSB/(deg/s) at ±1024 dps.

Useful implementation cross-checks inspected during WU-005 include Zephyr's QST QMI8658 driver and Lewis He/SensorLib QMI8658 support. These are secondary implementation references; Waveshare/QST documentation remains the hardware contract source.

The selected 4460 us acquisition scheduler corresponds to the documented 224.2 Hz 6-DoF rate. This is a configured target, not a measured physical-rate claim. Runtime one-second counters exist specifically so later physical evidence can record achieved rates.

## Physical evidence boundary

No physical stock-WLED receiver validation was available during WU-001 through WU-004. WU-005 likewise distinguishes automated firmware compilation/native adapter tests from physical sender validation unless an explicit evidence record is added.

Automated byte conformance, deterministic pattern tests, host logic tests and ESP32 compilation cannot establish:

- actual board-axis sign/orientation;
- live QMI8658 noise/clipping;
- achieved sample/packet rates;
- RF/multicast reliability;
- stock-WLED visual response.

Those claims require named physical hardware, firmware commit, network topology, receiver version/settings and observations.

## Re-verification rule

Before changing packet ABI, compatibility claims, destination defaults, receiver setup instructions, send cadence, board pins/address, QMI8658 register profile or release compatibility, inspect the applicable upstream source/docs again and update:

- `WLED_AUDIO_SYNC_V2.md` and `HOST_PROBE.md` for WLED protocol/receiver changes;
- `HARDWARE.md` and `WU005_IMPLEMENTATION.md` for board/sensor changes;
- this provenance file;
- relevant tests/fixtures;
- compatibility notes/release evidence.

Do not silently treat any snapshot as immutable protocol or hardware law if upstream changes.
