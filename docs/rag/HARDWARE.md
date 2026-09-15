# WLEDIMUUDP — Hardware Contract

## Reference sender

The first reference hardware target is an **ESP32-S3 with a QMI8658/QMI8658C IMU**.

The project originated from experimentation with compact ESP32-S3 boards that may also carry an 8×8 RGB matrix, but those LEDs are explicitly irrelevant to WLEDIMUUDP. A reference board may physically contain LEDs without the firmware initialising or depending on them.

## Required sender capabilities

A sender needs only:

- ESP32-class Wi-Fi capable MCU suitable for UDP multicast;
- 3-axis accelerometer;
- 3-axis gyroscope;
- stable monotonic timing;
- a way to provision Wi-Fi credentials;
- USB/serial strongly recommended for development and diagnostics.

No display, microphone, LED strip or WLED firmware is required on the sender.

## First IMU adapter: QMI8658/QMI8658C

The first hardware adapter should support the QMI8658 family used on the reference ESP32-S3 board.

Adapter responsibilities:

- sensor presence/probe;
- bus initialisation;
- sample-rate/range configuration;
- acceleration and gyro reads;
- conversion to documented physical units;
- board-axis transform supplied by configuration/board profile;
- error/status reporting.

The adapter must not contain synthetic-audio mapping or UDP logic.

## Board abstraction

Board-specific definitions belong in a narrow board profile:

- I2C/SPI pins;
- IMU interrupt pin if used;
- axis transform/sign convention;
- optional status-button or status-LED pins if a board has them.

A status LED is optional convenience only. Build correctness must not depend on one being present.

## Sampling baseline

Planning baseline for the QMI8658 reference target:

- accel + gyro enabled;
- approximately 200 Hz acquisition where stable;
- ranges selected to preserve ordinary hand motion while tolerating deliberate shakes/flicks;
- no reliance on sensor fusion firmware for the MVP.

Exact range/rate values become authoritative only after the implementation issue measures noise, clipping and responsiveness and records the chosen settings.

## Calibration lifecycle

The reference firmware should support a short stationary calibration phase at startup or on explicit command.

At minimum:

- estimate gyro bias;
- estimate stationary accelerometer magnitude/noise;
- verify data are finite and plausible;
- record enough diagnostics to identify wrong axes or a moving calibration surface.

A future persistent calibration store is allowed but should not be a prerequisite for the first end-to-end prototype.

## Wi-Fi and credentials

The sender must join the same IPv4 LAN as the target WLED receiver(s).

Rules:

- no credentials committed to Git;
- include a documented template such as `secrets.example.h`, environment variables, or equivalent;
- serial diagnostics must clearly report connection attempts, local IP, multicast target and send health;
- failure to join Wi-Fi must not generate fake high-energy motion packets.

Captive-portal or web provisioning is a later UX improvement, not required for core protocol validation.

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

Automated CI can prove compilation and host behavior; it cannot prove physical IMU orientation, RF reliability or stock-WLED visual response.

When physical validation is performed, record:

- sender board and IMU revision;
- firmware commit;
- WLED receiver version/device;
- network topology/channel if relevant;
- observed stillness noise;
- axis/orientation correctness;
- representative motions tested;
- any packet-loss/reconnect behavior.

Never replace missing physical evidence with assumptions.