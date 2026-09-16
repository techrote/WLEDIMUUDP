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
- expose a stable `MotionFeatures` struct plus explicit configuration/calibration state.

Must not know about UDP, WLED packet bytes, Arduino or a specific IMU.

WU-003 concretely implements this boundary as `lib/WledImuUdpMotion`. Its public input is `ImuSample{timestamp_us, accel_g, gyro_dps, valid}`. Its output includes gravity/confidence, linear acceleration, jerk, calibrated gyro/angular speed, normalized gravity orientation, instantaneous/smoothed motion energy, bounded impact state, stillness/confidence and input-valid state.

The motion library owns no hardware axis transform and no synthetic-spectrum policy. Hardware adapters must convert raw sensor values into the public calibrated units first; WU-004 consumes the resulting features afterward.

Invalid/non-finite or non-monotonic samples are rejected without advancing filter state. The last accepted feature snapshot is returned with `input_valid=false`, making upstream data-quality failures observable without injecting false motion.

### `core/mapping`

Responsibilities:

- convert `MotionFeatures` into `SyntheticAudioFrame`;
- synthesize 16 meaningful WLED bands;
- handle stillness decay and peak latching where packet-level semantics require it;
- map orientation into spectrum shape rather than false level;
- support versioned/tunable mapping profiles without changing protocol code.

WU-003 does not implement this layer. WU-004 must reuse the accepted motion-feature fixtures and semantics rather than deriving a second hidden motion model.

### `platform/imu`

Responsibilities:

- initialise a physical sensor;
- read accel/gyro data;
- expose scale/rate/range metadata;
- apply hardware-specific register/API behavior and axis transform;
- feed calibrated `ImuSample` values into the motion core;
- own the policy that determines when startup calibration samples are trustworthy/stationary.

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

WU-001 supplied a generic ESP32-S3 compile/smoke firmware for protocol encoding. WU-003 extends that smoke target only enough to instantiate the portable motion core with a synthetic stationary sample. It still intentionally does not join Wi-Fi, read a live IMU or initialise LEDs; those platform responsibilities remain deferred.

### `tools`

Host utilities are part of the compatibility strategy, not optional developer toys.

WU-002 concretely implements this boundary as:

- `lib/WledImuUdpHost`: deterministic pattern generation, CLI parsing, exact packet hex conversion, IPv4 handling and a thin native UDP socket adapter;
- `src/host_probe.cpp`: the native `host_probe` command with `send`, `listen` and `decode` modes;
- the existing `WledImuUdpCore` remains the only Audio Sync serializer/decoder.

The host layer depends on the protocol core; the protocol core does not depend on the host/network layer. `host_probe` is excluded from the ESP32-S3 source filter, so native socket code cannot leak into firmware.

WU-003 adds reusable synthetic motion-trace fixtures for native regression work. They are test assets, not a runtime dependency of firmware or the host packet tool.

## Timing model

Product target baseline:

- IMU acquisition: about 200 Hz where the hardware permits;
- motion feature update: every accepted IMU sample;
- WLED synthetic-audio frame generation: 50 Hz baseline;
- UDP send: one V2 packet per generated frame.

WU-002 locks the host probe to `1..50 Hz`, default 50 Hz, following current WLED Sound Sync guidance not to exceed the approximately 20 ms external-sender cadence.

WU-003 does not assume a perfect IMU scheduler. Motion state uses accepted monotonic timestamp deltas. Gravity and energy filters use `alpha = dt / (tau + dt)` and cap the filter step at the configured `max_dt_s` (default `0.10 s`) so long scheduler gaps do not cause unbounded state jumps. Non-positive timestamp deltas are rejected rather than coerced.

## Determinism

For an identical initial configuration and identical timestamped IMU trace, the motion feature and synthetic-audio outputs must be repeatable on the host test target.

WU-001 enforces deterministic encoding and a directly reviewable golden packet. WU-002 extends that contract: every named probe pattern is a pure function of `(pattern, frame_index)`, the 160-frame scripted sequence has locked order/cycle behavior, and tests verify the resulting packet equals the canonical WU-001 encoder output.

WU-003 extends determinism to signal processing. The extractor owns all state explicitly; fixed timestamped traces replay identically; invalid/non-monotonic inputs do not advance state; filter evolution depends on supplied elapsed time rather than scheduler call count. The shared nine-family trace corpus is intended to become the stable motion input for WU-004 mapping regressions.

Network delivery/timing remains outside the deterministic signal-processing core. Localhost UDP is tested only for exact byte preservation across the transport seam.

## Configuration model

Configuration remains separated into:

- Wi-Fi/network settings;
- sensor calibration/range/rate;
- motion thresholds/noise floors;
- synthetic-audio mapping profile;
- packet rate and multicast destination.

Real credentials must never be committed. WU-001 provides `config/wifi.example.hpp` and ignores `config/wifi.local.hpp`; the example is intentionally unused until transport work begins.

WU-002's host probe takes destination, port, packet rate and diagnostic limits from explicit CLI options. Those host options do not become firmware configuration implicitly.

WU-003 makes the motion boundary explicit with `MotionCalibration` and `MotionConfig`. Default filter constants/thresholds are documented in `MOTION_MAPPING.md` and `WU003_IMPLEMENTATION.md`; they are feature-extraction defaults, not WLED effect knobs.

## Failure behavior

- Invalid/non-finite sensor values are rejected before they can alter motion state.
- Non-monotonic timestamps are rejected without state advance.
- If the sensor fails, firmware must fail visibly through serial diagnostics and must not emit arbitrary high-energy packets.
- If Wi-Fi is lost, motion processing may continue but sends are skipped; reconnection must not reset calibration unless explicitly requested.
- Stillness and sender shutdown must not leave WLED with permanently asserted activity. Feature-level motion energy decays predictably; WU-004 must add packet-level silence/peak semantics deliberately.

At the protocol boundary, WU-001 sanitises malformed semantic values before encoding and the strict decoder rejects malformed wire packets. WU-002 exposes those decoder errors through host tooling instead of coercing malformed captures into apparently valid frames. Socket/bind/send/receive failures are observable and return non-zero status.

## Resource posture

The steady-state embedded hot path should be fixed-size and allocation-free where practical. The protocol payload is 44 bytes; motion state is fixed-size and uses no dynamic allocation in the extractor hot path. Feature/mapping state should remain small enough that the reference ESP32-S3 target has ample headroom.

Native host tooling and test fixtures may use standard-library strings/vectors for diagnostics and fixture construction because they are not part of the embedded runtime. No part of the architecture assumes the sender has addressable LEDs.