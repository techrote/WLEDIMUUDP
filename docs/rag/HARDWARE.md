# WLEDIMUUDP — Hardware Contract

## Reference sender

The first reference hardware target is the **Waveshare ESP32-S3-Matrix** with onboard **QMI8658/QMI8658C** IMU.

The board also carries an 8×8 RGB matrix. Those LEDs are explicitly irrelevant to WLEDIMUUDP: WU-005 does not initialise, address, or depend on them.

## Locked WU-005 board profile

The profile is named `waveshare-esp32-s3-matrix` and owns all board-specific signal definitions:

- QMI8658 I2C SDA: GPIO11;
- QMI8658 I2C SCL: GPIO12;
- QMI8658 INT1: GPIO10;
- QMI8658 INT2: GPIO13;
- I2C bus rate: 400 kHz;
- QMI8658 board address: `0x6B`;
- axis transform: explicit identity permutation/sign convention for the initial reference profile.

The adapter may probe `0x6A` second as a generic-family recovery address, but `0x6B` is the reference-board contract.

The identity axis transform means `board X/Y/Z = sensor X/Y/Z` at the software boundary. This is intentionally explicit rather than baked into motion processing. Physical board-edge/sign confirmation remains a WU-006 manual validation item; if evidence shows the board convention should change, only the profile transform changes.

PlatformIO compiles the board through `esp32-s3-devkitc-1` with DIO flash mode; runtime pins and IMU behavior are supplied by the WLEDIMUUDP board profile rather than by a sender-LED board package.

## Required sender capabilities

A sender needs only:

- ESP32-class Wi-Fi capable MCU suitable for UDP multicast;
- 3-axis accelerometer;
- 3-axis gyroscope;
- stable monotonic timing;
- a way to provision Wi-Fi credentials;
- USB/serial strongly recommended for development and diagnostics.

No display, microphone, LED strip or WLED firmware is required on the sender.

## QMI8658/QMI8658C adapter

WU-005 implements the first physical adapter directly on Arduino `TwoWire` with no third-party sensor dependency.

Reference register profile:

- `WHO_AM_I` `0x00` must read `0x05`;
- `CTRL1` `0x02` = `0x40`: address auto-increment enabled, little-endian output, interrupts unused;
- `CTRL2` `0x03` = `0x25`: accelerometer ±8 g, ODR code `0101`;
- `CTRL3` `0x04` = `0x65`: gyroscope ±1024 dps, ODR code `0101`;
- `CTRL5` `0x06` = `0x00`: sensor LPFs disabled so accepted WU-003 filtering remains authoritative;
- `CTRL7` `0x08` = `0x03`: accelerometer and gyroscope enabled.

Output data are read as a 12-byte auto-increment block from `AX_L` `0x35`: accel XYZ followed by gyro XYZ, each signed 16-bit. Conversion constants are:

- ±8 g: 4096 LSB/g;
- ±1024 dps: 32 LSB/(deg/s).

The board transform is applied after unit conversion and before producing the accepted `ImuSample` contract. Synthetic-audio or UDP behavior is not present in the sensor adapter.

## Sampling baseline now selected

QMI8658 6-DoF ODR code `0101` is **224.2 Hz effective** when accelerometer and gyroscope are both enabled. WU-005 therefore uses:

- sensor configuration: 224.2 Hz effective 6-DoF ODR;
- firmware acquisition period: 4460 us, approximately 224 Hz;
- motion feature update: every accepted sample;
- mapping/UDP packet cadence: 50 Hz / 20 ms by default.

The ranges are deliberately wider than a still-orientation demo so deliberate hand shakes/flicks can be represented without immediately clipping.

These are configured targets selected from device documentation, not measured physical rates. Runtime serial diagnostics expose one-second valid-sample and successful-send counts so achieved rates can be recorded on real hardware later.

## Calibration lifecycle

The reference firmware performs a non-persistent stationary calibration at startup and on serial command `r`.

WU-005 qualifies a 256-valid-sample window before handing values to the accepted WU-003 calibration model. A window is rejected if it observes:

- gyro magnitude above 5 deg/s; or
- accelerometer magnitude more than 0.08 g away from 1 g.

After accumulation, calibration is also rejected if derived noise floors exceed:

- accelerometer: 0.035 g;
- gyroscope: 1.5 deg/s.

Accepted output contains gyro bias, gravity reference and noise floors and is passed into `MotionFeatureExtractor` without modifying WU-003 semantics.

A rejected window prints an explicit reason and restarts. Invalid sensor reads are counted separately. Any runtime sensor read failure pauses live sending immediately, triggers periodic re-probe, and requires a fresh successful calibration before live packets resume.

These qualification thresholds are an implementation baseline, not physical characterization evidence.

## Wi-Fi and credentials

The sender must join the same IPv4 LAN as the target WLED receiver(s).

Rules:

- no credentials committed to Git;
- committed template: `config/wifi.example.hpp`;
- local secrets file: `config/wifi.local.hpp`, ignored by Git;
- placeholder credentials compile in CI but intentionally make no Wi-Fi attempt;
- serial diagnostics report connection transitions, local IP, multicast target and counters;
- failure to join Wi-Fi cannot create a synthetic high-energy packet.

Default multicast transport is `239.0.0.1:11988` at 50 packets/s. The local configuration header can override multicast address, port, packet rate and reconnect interval.

While Wi-Fi is disconnected, sensor/motion processing may continue but sends are skipped. Reconnect attempts are bounded by the configured interval and reconnection does not reset calibration.

## Diagnostic mode

WU-005 provides an explicit known-frame transport diagnostic independent of live IMU input. Serial `d` selects it; `l` returns to live mode.

Diagnostic mode still uses the canonical WU-001 Audio Sync V2 encoder and normal multicast transport. It exists so a user can distinguish network/WLED faults from QMI8658/calibration faults. It is the only mode allowed to send while the sensor is unavailable, and that exception is explicit rather than an accidental stale-frame path.

## Network topology

Default topology:

```text
ESP32-S3 IMU sender ──Wi-Fi──┐
                             ├── local AP/router ── stock WLED receiver(s)
PC probe/debug tools ────────┘
```

The MVP assumes an ordinary local Wi-Fi network. The project does not require internet access.

## Power

No special battery/power architecture is required for the first firmware. USB power is sufficient for development.

Future battery operation must account for Wi-Fi and continuous IMU sampling; do not prematurely trade away responsiveness for deep-sleep behaviour in the MVP.

## Portability targets

The core architecture should make these future ports straightforward:

- another ESP32-S3 board with the same QMI8658 family;
- other 6-axis IMUs through a new adapter;
- sender boards with no LEDs at all;
- potentially ESP32/C3/S3 variants if memory/network/float support is adequate.

Portability does **not** require abstracting every hardware detail before the first reference implementation works.

## Hardware evidence rules

Automated CI can prove compilation, deterministic unit conversion, failure gating and host behavior. It cannot prove physical IMU orientation, RF reliability, actual scheduler cadence, sensor noise/clipping or stock-WLED visual response.

When physical validation is performed, record:

- sender board and IMU revision;
- firmware commit;
- WLED receiver version/device;
- network topology/channel if relevant;
- observed one-second sample/send counts;
- observed stillness noise and clipping behavior;
- axis/orientation correctness;
- representative motions tested;
- packet-loss/reconnect behavior.

Never replace missing physical evidence with assumptions.
