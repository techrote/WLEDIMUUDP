# WLEDIMUUDP — Architecture

## Runtime data flow

```text
IMU hardware
    │
    ▼
ImuSource adapter
    │ calibrated accel + gyro + monotonic timestamp
    ▼
MotionFeatureExtractor
    │ gravity, linear accel, jerk, angular energy, tilt/orientation, impact state
    ▼
SyntheticAudioMapper
    │ sampleRaw, sampleSmth, samplePeak, fft[16], magnitude, majorPeak
    ▼
WledAudioSyncV2Encoder
    │ exact 44-byte V2 payload
    ▼
UdpAudioSyncTransport
    │ multicast UDP
    ▼
stock WLED Audio Sync receiver(s)
    │
    ▼
WLED audio-reactive effects
```

The project intentionally keeps those stages separable. A packet encoder must be testable with no IMU. A motion mapper must be testable with no Wi-Fi. An IMU adapter must not know anything about WLED packet fields.

## Module boundaries

### `core/protocol`

Responsibilities:

- define the semantic `SyntheticAudioFrame` data model;
- encode WLED Audio Sync V2 exactly;
- validate/clamp values;
- expose protocol version, encoded size and destination defaults;
- provide host-side golden fixtures/decoder helpers for tests.

Must not depend on Arduino, Wi-Fi or a specific IMU.

### `core/motion`

Responsibilities:

- consume calibrated, timestamped accel/gyro samples;
- estimate gravity/orientation;
- separate gravity from dynamic acceleration;
- calculate jerk, angular velocity/energy and impact/tap candidates;
- maintain deterministic smoothing, thresholds and decay;
- expose a versioned `MotionFeatures` struct.

Must not know about UDP or WLED packet bytes.

### `core/mapping`

Responsibilities:

- convert `MotionFeatures` into `SyntheticAudioFrame`;
- synthesize 16 meaningful WLED bands;
- handle stillness decay and peak latching;
- map orientation into spectrum shape rather than false level;
- support versioned/tunable mapping profiles without changing protocol code.

### `platform/imu`

Responsibilities:

- initialise a physical sensor;
- read accel/gyro data;
- expose scale/rate/range metadata;
- apply hardware-specific register/API behavior only.

The first adapter is QMI8658/QMI8658C. Future sensors implement the same source contract.

### `platform/network`

Responsibilities:

- join configured Wi-Fi;
- send encoded frames to the configured multicast address/port;
- expose connection/send diagnostics;
- recover from temporary disconnection without corrupting core state.

No protocol interpretation belongs here.

### `firmware`

Responsibilities:

- fixed-rate scheduling;
- calibration lifecycle;
- configuration loading;
- wiring adapters to core modules;
- serial diagnostics/status;
- no required LED output path.

### `tools`

Host utilities should include, in roadmap order:

- a known-pattern Audio Sync V2 sender;
- a packet decoder/sniffer suitable for captured 44-byte payloads;
- fixture generators/replayers for motion traces.

These tools are part of the compatibility strategy, not optional developer toys.

## Timing model

Initial planning baseline:

- IMU acquisition: about 200 Hz where the hardware permits;
- motion feature update: every IMU sample;
- WLED synthetic-audio frame generation: 50 Hz baseline;
- UDP send: one V2 packet per generated frame.

These are defaults, not protocol requirements. Exact rates become implementation contracts only when locked by tests/docs. Core code must use elapsed monotonic time rather than assuming perfect scheduler cadence.

## Determinism

For an identical initial configuration and identical timestamped IMU trace, the motion feature and synthetic-audio outputs must be byte-for-byte repeatable on the host test target.

Network timing, packet loss and Wi-Fi reconnect behavior are outside the deterministic signal-processing core.

## Configuration model

Configuration is expected to separate:

- Wi-Fi/network settings;
- sensor calibration/range/rate;
- motion thresholds/noise floors;
- synthetic-audio mapping profile;
- packet rate and multicast destination.

Real credentials must never be committed. The first firmware may use a generated/local secrets header or environment-driven build configuration; later UX improvements may replace that without changing core APIs.

## Failure behavior

- Invalid/non-finite sensor values are rejected/sanitised before entering mapping logic.
- If the sensor fails, firmware must fail visibly through serial diagnostics and must not emit arbitrary high-energy packets.
- If Wi-Fi is lost, motion processing may continue but sends are skipped; reconnection must not reset calibration unless explicitly requested.
- Stillness and sender shutdown must not leave WLED with a permanently asserted peak or non-decaying activity. The mapper therefore owns explicit decay-to-silence semantics.

## Resource posture

The steady-state hot path should be fixed-size and allocation-free where practical. The protocol payload is 44 bytes; feature/mapping state should remain small enough that the reference ESP32-S3 target has ample headroom.

No part of the architecture assumes the sender has addressable LEDs.