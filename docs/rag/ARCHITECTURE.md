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

WU-001 concretely implements this boundary as the portable PlatformIO library `lib/WledImuUdpCore`. `SyntheticAudioFrame` is semantic state; `AudioSyncV2Packet` is the exact 44-byte wire representation. The protocol library contains no Arduino or network dependency. A strict decoder exists for tests and host tooling but is not part of the sender hot path.

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

WU-001 supplies only a generic ESP32-S3 compile/smoke firmware that exercises protocol encoding and serial reporting. It intentionally does not join Wi-Fi, read an IMU or initialise LEDs; those platform responsibilities remain deferred to later milestones.

### `tools`

Host utilities are part of the compatibility strategy, not optional developer toys.

WU-002 concretely implements this boundary as:

- `lib/WledImuUdpHost`: deterministic pattern generation, CLI parsing, exact packet hex conversion, IPv4 handling and a thin native UDP socket adapter;
- `src/host_probe.cpp`: the native `host_probe` command with `send`, `listen` and `decode` modes;
- the existing `WledImuUdpCore` remains the only Audio Sync serializer/decoder.

The host layer depends on the protocol core; the protocol core does not depend on the host/network layer. `host_probe` is excluded from the ESP32-S3 source filter, so native socket code cannot leak into firmware.

Later host tooling may add motion-trace fixture generators/replayers without changing this dependency direction.

## Timing model

Initial product baseline:

- IMU acquisition: about 200 Hz where the hardware permits;
- motion feature update: every IMU sample;
- WLED synthetic-audio frame generation: 50 Hz baseline;
- UDP send: one V2 packet per generated frame.

WU-002 locks the host probe to `1..50 Hz`, default 50 Hz, following current WLED Sound Sync guidance not to exceed the approximately 20 ms external-sender cadence. Core code must use elapsed monotonic time rather than assuming perfect scheduler cadence.

## Determinism

For an identical initial configuration and identical timestamped IMU trace, the motion feature and synthetic-audio outputs must be byte-for-byte repeatable on the host test target.

WU-001 enforces deterministic encoding and a directly reviewable golden packet. WU-002 extends that contract: every named probe pattern is a pure function of `(pattern, frame_index)`, the 160-frame scripted sequence has locked order/cycle behavior, and tests verify the resulting packet equals the canonical WU-001 encoder output.

Network delivery/timing remains outside the deterministic signal-processing core. Localhost UDP is tested only for exact byte preservation across the transport seam.

## Configuration model

Configuration is expected to separate:

- Wi-Fi/network settings;
- sensor calibration/range/rate;
- motion thresholds/noise floors;
- synthetic-audio mapping profile;
- packet rate and multicast destination.

Real credentials must never be committed. WU-001 provides `config/wifi.example.hpp` and ignores `config/wifi.local.hpp`; the example is intentionally unused by the protocol-smoke firmware until transport work begins.

WU-002's host probe takes destination, port, packet rate and diagnostic limits from explicit CLI options. Those host options do not become firmware configuration implicitly.

## Failure behavior

- Invalid/non-finite sensor values are rejected/sanitised before entering mapping logic.
- If the sensor fails, firmware must fail visibly through serial diagnostics and must not emit arbitrary high-energy packets.
- If Wi-Fi is lost, motion processing may continue but sends are skipped; reconnection must not reset calibration unless explicitly requested.
- Stillness and sender shutdown must not leave WLED with a permanently asserted peak or non-decaying activity. The mapper therefore owns explicit decay-to-silence semantics.

At the protocol boundary, WU-001 sanitises malformed semantic values before encoding and the strict decoder rejects malformed wire packets. WU-002 exposes those decoder errors through host tooling instead of coercing malformed captures into apparently valid frames. Socket/bind/send/receive failures are observable and return non-zero status.

## Resource posture

The steady-state embedded hot path should be fixed-size and allocation-free where practical. The protocol payload is 44 bytes; feature/mapping state should remain small enough that the reference ESP32-S3 target has ample headroom.

Native host tooling may use standard-library strings/vectors for diagnostics because it is not part of the embedded runtime. No part of the architecture assumes the sender has addressable LEDs.
