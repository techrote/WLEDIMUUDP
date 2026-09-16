# WU-005 — Reference sender implementation contract

## Scope and evidence boundary

WU-005 turns the accepted WU-001 through WU-004 pure-core chain into a reference ESP32-S3 sender for the Waveshare **ESP32-S3-Matrix** with its onboard QMI8658/QMI8658C IMU.

This implementation is deliberately narrow:

```text
QMI8658(C) -> calibrated ImuSample -> MotionFeatureExtractor -> Balanced-v1 mapper
            -> canonical Audio Sync V2 encoder -> Wi-Fi multicast -> stock WLED
```

The sender's 8x8 RGB matrix is not initialised, addressed, or required.

Automated evidence covers native logic, exact packet reuse, failure gating and an ESP32-S3 build. No physical board, RF, axis-orientation, sensor-noise, or stock-WLED visual-response claim is implied until that evidence is recorded explicitly.

## Reference board profile

Board profile: `waveshare-esp32-s3-matrix`.

Authoritative schematic wiring inspected for WU-005:

- QMI8658 I2C SDA: GPIO11;
- QMI8658 I2C SCL: GPIO12;
- QMI8658 INT1: GPIO10;
- QMI8658 INT2: GPIO13;
- I2C bus: 400 kHz Fast Mode;
- board-specific QMI8658 address: `0x6B`;
- `0x6A` is retained only as a conservative secondary probe for the generic QMI8658 family.

The board profile explicitly owns the axis transform. The initial reference transform is identity (`board X/Y/Z = sensor X/Y/Z`). That makes the sign/permutation convention reviewable rather than hidden in motion processing. Physical confirmation of which board edge should be considered positive X/Y remains a manual WU-006 validation item; a correction belongs in the board profile only, not in `WledImuUdpMotion`.

The firmware compiles through PlatformIO's `esp32-s3-devkitc-1` target with DIO flash mode while hardware pins and sensor behavior are supplied by the explicit reference-board profile.

## QMI8658 configuration

The adapter talks directly over Arduino `TwoWire`; no third-party sensor library is required.

Locked WU-005 register profile:

| Register | Value | Meaning |
| --- | ---: | --- |
| `WHO_AM_I` (`0x00`) | expect `0x05` | sensor identity |
| `CTRL1` (`0x02`) | `0x40` | address auto-increment, little-endian data, interrupts unused |
| `CTRL2` (`0x03`) | `0x25` | accelerometer +/-8 g, ODR code `0101` |
| `CTRL3` (`0x04`) | `0x65` | gyroscope +/-1024 dps, ODR code `0101` |
| `CTRL5` (`0x06`) | `0x00` | sensor LPFs disabled; accepted motion core owns filtering |
| `CTRL7` (`0x08`) | `0x03` | accelerometer + gyroscope enabled |

Conversion constants are 4096 LSB/g for +/-8 g and 32 LSB/(deg/s) for +/-1024 dps. A 12-byte auto-increment read beginning at `AX_L` (`0x35`) supplies accel XYZ followed by gyro XYZ as signed 16-bit values.

QST's 6-DoF table gives ODR code `0101` as **224.2 Hz effective** when accelerometer and gyroscope are both enabled. The firmware therefore uses a 4460 us acquisition period (~224.2 Hz), rather than describing the code as a physically measured 250 Hz stream. Packet mapping/transmission remains 50 Hz by default (20 ms period).

These choices prioritise enough headroom for deliberate shakes/flicks while retaining useful resolution. Physical clipping/noise measurements remain pending; the serial one-second counters expose achieved sample/send rates for that validation.

## Calibration lifecycle

`StartupCalibration` is portable and host-tested. The reference policy collects at least 256 valid samples and rejects a window if it sees either:

- instantaneous gyro magnitude above 5 deg/s; or
- accelerometer magnitude more than 0.08 g away from 1 g.

After the existing WU-003 `CalibrationAccumulator` computes calibration values, WU-005 additionally rejects accel noise floor above 0.035 g or gyro noise floor above 1.5 deg/s.

On success, gyro bias, gravity reference and noise floors are passed unchanged into the accepted `MotionFeatureExtractor`. On rejection the firmware prints the reason and starts another window. Serial `r` explicitly restarts calibration. A sensor read failure invalidates the live feature snapshot immediately, pauses live transmission, and requires successful re-probe plus fresh calibration before live packets resume.

The thresholds are a safe startup qualification baseline, not a physical characterization result.

## Wi-Fi and multicast transport

`config/wifi.example.hpp` is the canonical safe template. Copy it to ignored `config/wifi.local.hpp` and edit the local copy. A clean checkout intentionally compiles with `CHANGE_ME` credentials but does not attempt a connection.

Defaults:

- station mode;
- multicast `239.0.0.1:11988`;
- 50 packets/s;
- reconnect attempt no more often than every 5 s while disconnected.

The firmware uses `WiFiUDP::beginPacketMulticast()` and sends only packets produced by the accepted WU-001 encoder. Wi-Fi loss skips sends but does not mutate motion/calibration state. Reconnection itself cannot manufacture a motion packet.

Live packet emission requires all of: Wi-Fi connected, sensor healthy, calibration accepted, and a current valid motion feature snapshot. That gate is host-tested. A sensor failure therefore cannot leave a stale high-energy frame streaming.

## Diagnostic mode

Serial command `d` selects an explicit known synthetic diagnostic frame. It bypasses the live-sensor requirement but still uses the canonical Audio Sync V2 encoder and normal multicast transport. This isolates Wi-Fi/WLED troubleshooting from IMU/calibration problems.

Serial command `l` returns to live IMU mode. `r` requests recalibration. `?` prints the command summary.

The diagnostic frame is fixed and host-tested for deterministic byte identity and bounded band values. It is not an alternate packet format.

## Scheduling and time

The runtime uses fixed-rate gates with no catch-up bursts:

- IMU acquisition: 4460 us period (~224.2 Hz target);
- packet generation/transmission: configurable, default 20 ms / 50 Hz;
- bounded status diagnostics: 1 s.

A `MicrosExtender` converts the ESP32 32-bit `micros()` counter into monotonic 64-bit timestamps across wrap. These timestamps feed the existing elapsed-time-aware motion core. Project code performs no steady-state heap allocation in the sensor/mapping/encode/send path.

Serial status reports the actual count of valid IMU samples and successful packets in each one-second window, plus accumulated sensor/send/reconnect/skipped counters. Those observations, not the configured targets alone, are the correct basis for later physical-rate claims.

## Failure semantics

- wrong/missing `WHO_AM_I`: sensor unavailable, visible over serial, periodic re-probe;
- register/config/read failure: live sends stop immediately;
- moving/noisy startup window: calibration rejected with reason and retried;
- missing credentials: firmware still builds/runs sensor path but makes no Wi-Fi attempt;
- disconnected Wi-Fi: live processing may continue, packets are skipped, reconnect is bounded;
- diagnostic mode: intentionally permits the known synthetic frame without a healthy sensor, solely to isolate transport/receiver faults;
- no sender LED path exists.

## Automated gate added by WU-005

The portable firmware-support tests cover:

- exact board pins/address and explicit axis transform;
- QMI8658 register/scaling profile;
- signed raw conversion and unit/axis conversion;
- malformed read rejection;
- accepted stationary calibration and rejected moving calibration;
- invalid calibration-sample accounting;
- multicast defaults and live/diagnostic send-safety gate;
- bounded reconnect behavior;
- `micros()` wrap extension;
- fixed-rate no-burst scheduling;
- deterministic diagnostic-frame encoding through WU-001.

The ESP32-S3 CI target compiles the real `WiFi`, `WiFiUDP`, `Wire`, QMI8658 adapter and firmware runtime with placeholder credentials. Physical hardware validation remains pending by design.
