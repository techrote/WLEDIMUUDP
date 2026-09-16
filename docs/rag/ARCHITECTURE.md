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

The motion library owns no hardware axis transform and no synthetic-spectrum policy. Hardware adapters must convert raw sensor values into the public calibrated units first; the mapping layer consumes the resulting features afterward.

Invalid/non-finite or non-monotonic samples are rejected without advancing filter state. The last accepted feature snapshot is returned with `input_valid=false`, making upstream data-quality failures observable without injecting false motion.

### `core/mapping`

Responsibilities:

- convert `MotionFeatures` into `SyntheticAudioFrame`;
- synthesize 16 meaningful WLED bands;
- translate the WU-003 impact latch into packet-level one-shot peak semantics;
- map orientation into spectrum shape rather than false level;
- compute coherent synthetic magnitude and major-peak fields;
- support versioned/tunable mapping profiles without changing protocol code.

WU-004 concretely implements this boundary as `lib/WledImuUdpMapping`. Its accepted MVP profile is `Balanced-v1`:

- bands 0–3: translation;
- bands 4–7: rotation;
- bands 8–11: shake/jerk;
- bands 12–15: impact/transient energy;
- orientation shifts existing energy within each four-band group but never creates energy;
- `sampleRaw`/`sampleSmth` derive from WU-003 instant/smoothed motion energy;
- `samplePeak` is a rising-edge event derived from the accepted impact latch;
- spectral magnitude is RMS over emitted bands;
- major peak is the weighted centroid of a locked 16-entry center table.

The exact defaults and center table are authoritative in `MOTION_MAPPING.md` and `WU004_IMPLEMENTATION.md`. The mapping library has no Arduino, Wi-Fi, UDP, QMI8658 or sender-LED dependency.

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

WU-001 supplied a generic ESP32-S3 compile/smoke firmware for protocol encoding. WU-003 added the portable motion core. WU-004 extends the same smoke target through motion→mapping→exact packet encoding using a synthetic stationary sample. It still intentionally does not join Wi-Fi, read a live IMU or initialise LEDs; those platform responsibilities remain deferred to WU-005.

### `tools`

Host utilities are part of the compatibility strategy, not optional developer toys.

WU-002 implements:

- `lib/WledImuUdpHost`: deterministic packet-pattern generation, CLI parsing, exact packet hex conversion, IPv4 handling and a thin native UDP socket adapter;
- `src/host_probe.cpp`: the native `host_probe` command with `send`, `listen` and `decode` modes.

WU-004 adds `src/mapping_probe.cpp`, a separate native inspection executable for representative `still`, `sway`, `spin`, `shake`, `impact`, `tilt-left` and `tilt-right` feature snapshots. It prints the Balanced-v1 semantic frame and exact packet emitted by the canonical WU-001 encoder. CI builds and executes this probe independently of the network host tool.

`WledImuUdpCore` remains the only Audio Sync serializer/decoder. Native tool source files are excluded from the ESP32-S3 source filter so desktop socket/iostream code cannot leak into firmware.

WU-003's reusable synthetic motion-trace fixtures remain test assets, not a runtime dependency of firmware or the host tools.

## Timing model

Product target baseline:

- IMU acquisition: about 200 Hz where the hardware permits;
- motion feature update: every accepted IMU sample;
- WLED synthetic-audio frame generation: 50 Hz baseline;
- UDP send: one V2 packet per generated frame.

WU-002 locks the host network probe to `1..50 Hz`, default 50 Hz, following current WLED Sound Sync guidance not to exceed the approximately 20 ms external-sender cadence.

WU-003 does not assume a perfect IMU scheduler. Motion state uses accepted monotonic timestamp deltas. Gravity and energy filters use `alpha = dt / (tau + dt)` and cap the filter step at the configured `max_dt_s` (default `0.10 s`) so long scheduler gaps do not cause unbounded state jumps. Non-positive timestamp deltas are rejected rather than coerced.

WU-004 mapping is a deterministic stateful transform over accepted feature snapshots. It does not own scheduler timing; WU-005 will decide when the 200 Hz-class feature stream is sampled/aggregated into the 50 Hz-class mapping/send cadence.

## Determinism

For an identical initial configuration and identical timestamped IMU trace, motion features, synthetic-audio frames and encoded packets must be repeatable on the host test target.

WU-001 enforces deterministic encoding and a directly reviewable golden packet. WU-002 extends that contract to deterministic host patterns and exact-byte transport seams. WU-003 extends it to signal processing with explicit state and timestamp-driven filters.

WU-004 completes the current pure-core chain: the shared nine-family WU-003 traces are mapped through Balanced-v1, tests compare semantic distributions and bounded behavior, and two independent extractor/mapper instances must produce byte-identical WU-001 packets for the same fixed trace. Invalid feature snapshots do not advance mapper state or retrigger a peak.

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

WU-003 exposes `MotionCalibration` and `MotionConfig`. WU-004 independently exposes `MappingConfig`, so perceptual/profile tuning does not silently mutate motion extraction. Balanced-v1 defaults are documented in `MOTION_MAPPING.md` and `WU004_IMPLEMENTATION.md`.

## Failure behavior

- Invalid/non-finite sensor values are rejected before they can alter motion state.
- Non-monotonic timestamps are rejected without state advance.
- An invalid `MotionFeatures` snapshot preserves the last accepted mapping frame but forces `samplePeak=false`; it does not advance peak-edge state.
- If the sensor fails, firmware must fail visibly through serial diagnostics and must not emit arbitrary high-energy packets.
- If Wi-Fi is lost, motion processing may continue but sends are skipped; reconnection must not reset calibration unless explicitly requested.
- Stillness and sender shutdown must not leave WLED with permanently asserted activity. WU-003 drives motion energy toward quiet; Balanced-v1 maps quiet features to zero spectrum/magnitude and one-frame-only impact peaks.

At the protocol boundary, WU-001 sanitises malformed semantic values before encoding and the strict decoder rejects malformed wire packets. WU-002 exposes those decoder errors through host tooling instead of coercing malformed captures into apparently valid frames. Socket/bind/send/receive failures are observable and return non-zero status.

## Resource posture

The steady-state embedded hot path should be fixed-size and allocation-free where practical. The protocol payload is 44 bytes; motion and Balanced-v1 mapping state are fixed-size and use no dynamic allocation in their hot paths. The reference ESP32-S3 compile gate exercises the complete pure-core chain.

Native host tooling and test fixtures may use standard-library strings/vectors for diagnostics and fixture construction because they are not part of the embedded runtime. No part of the architecture assumes the sender has addressable LEDs.
