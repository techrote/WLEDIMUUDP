# WLEDIMUUDP — Architecture

## Runtime data flow

```text
QMI8658/QMI8658C hardware
    │ I2C register block
    ▼
Qmi8658Adapter + board profile
    │ calibrated-unit accel + gyro + monotonic timestamp
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
WiFiUDP multicast transport
    │ 239.0.0.1:11988 by default
    ▼
stock WLED Audio Sync receiver(s)
    │
    ▼
WLED audio-reactive effects
```

The project intentionally keeps those stages separable. A packet encoder is testable with no IMU. A motion mapper is testable with no Wi-Fi. Sensor conversion/calibration qualification and runtime scheduling have portable test seams, while Arduino `Wire`/`WiFi` code remains at the platform/firmware edge.

## Module boundaries

### `core/protocol`

Responsibilities:

- define the semantic `SyntheticAudioFrame` data model;
- encode WLED Audio Sync V2 exactly;
- validate/clamp values;
- expose protocol version, encoded size and destination defaults;
- provide host-side golden fixtures/decoder helpers for tests.

Must not depend on Arduino, Wi-Fi or a specific IMU.

WU-001 implements this boundary as `lib/WledImuUdpCore`. `SyntheticAudioFrame` is semantic state; `AudioSyncV2Packet` is the exact 44-byte wire representation. The protocol library contains no Arduino or network dependency. A strict decoder exists for tests and host tooling but is not part of the sender hot path.

### `core/motion`

Responsibilities:

- consume calibrated, timestamped accel/gyro samples;
- estimate gravity/orientation;
- separate gravity from dynamic acceleration;
- calculate jerk, angular velocity/energy and impact/tap candidates;
- maintain deterministic smoothing, thresholds and decay;
- expose a stable `MotionFeatures` struct plus explicit configuration/calibration state.

Must not know about UDP, WLED packet bytes, Arduino or a specific IMU.

WU-003 implements this boundary as `lib/WledImuUdpMotion`. Its public input is `ImuSample{timestamp_us, accel_g, gyro_dps, valid}`. Its output includes gravity/confidence, linear acceleration, jerk, calibrated gyro/angular speed, normalized gravity orientation, instantaneous/smoothed motion energy, bounded impact state, stillness/confidence and input-valid state.

The motion library owns no hardware axis transform and no synthetic-spectrum policy. Hardware adapters convert raw sensor values into the public calibrated units first; the mapping layer consumes the resulting features afterward.

Invalid/non-finite or non-monotonic samples are rejected without advancing filter state. The last accepted feature snapshot is returned with `input_valid=false`, making upstream data-quality failures observable without injecting false motion.

### `core/mapping`

Responsibilities:

- convert `MotionFeatures` into `SyntheticAudioFrame`;
- synthesize 16 meaningful WLED bands;
- translate the WU-003 impact latch into packet-level one-shot peak semantics;
- map orientation into spectrum shape rather than false level;
- compute coherent synthetic magnitude and major-peak fields;
- support versioned/tunable mapping profiles without changing protocol code.

WU-004 implements this boundary as `lib/WledImuUdpMapping`. Its accepted MVP profile is `Balanced-v1`:

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

### `firmware/support`

WU-005 adds portable `lib/WledImuUdpFirmware` for hardware-adjacent policy that still benefits from native tests:

- explicit reference `BoardProfile` and signed/permuted axis transform;
- reviewable QMI8658 register/scaling constants and pure 12-byte raw decoder;
- startup calibration qualification around the accepted WU-003 accumulator;
- multicast defaults and live/diagnostic send-safety gate;
- bounded reconnect timing policy;
- 32-bit `micros()` wrap extension to monotonic 64-bit time;
- fixed-rate gates that skip backlog bursts instead of emitting catch-up packets;
- a deterministic known diagnostic `SyntheticAudioFrame`.

This library has no Arduino/Wi-Fi/Wire dependency. It may depend on the accepted protocol and motion data models because it is the integration-policy seam between platform adapters and firmware.

### `platform/imu`

Responsibilities:

- initialise a physical sensor;
- read accel/gyro data;
- expose scale/rate/range metadata;
- apply hardware-specific register/API behavior and board-axis transform;
- feed calibrated-unit `ImuSample` values into the motion core;
- expose hardware errors without inventing motion.

WU-005 implements `src/qmi8658_adapter.*` as the first Arduino adapter. It uses `TwoWire`, probes the reference `0x6B` address with `0x6A` as a secondary family fallback, verifies `WHO_AM_I=0x05`, writes the locked accel/gyro profile, reads the 12-byte motion block, converts ±8 g and ±1024 dps raw values, and applies the board transform before producing `ImuSample`.

The adapter contains no synthetic-audio mapping, UDP code or sender LED code.

### `platform/network`

Responsibilities:

- join configured Wi-Fi;
- send already-encoded frames to the configured multicast address/port;
- expose connection/send diagnostics;
- recover from temporary disconnection without corrupting core state.

WU-005 uses Arduino `WiFi` station mode plus `WiFiUDP::beginPacketMulticast()`. The platform layer never interprets Audio Sync fields: it receives the exact packet from `WledImuUdpCore` and writes those 44 bytes.

A reconnect attempt is made immediately when needed and then no more often than the configured interval (5 s default). While disconnected, motion processing may continue but packet sends are skipped. A reconnect does not manufacture a frame or reset calibration.

### `firmware`

Responsibilities:

- fixed-rate scheduling;
- startup/recalibration lifecycle;
- configuration loading;
- wiring adapters to accepted core modules;
- serial diagnostics/status;
- no required LED output path.

WU-005 replaces the earlier embedded smoke-only `src/main.cpp` with the reference sender runtime. Live mode executes:

```text
QMI read -> StartupCalibration / MotionFeatureExtractor -> Balanced-v1 -> encoder -> multicast
```

Live send eligibility requires Wi-Fi connected, sensor healthy, calibration accepted and a current valid `MotionFeatures` snapshot. A sensor read error clears live eligibility immediately, pauses live packets, schedules re-probe and requires fresh calibration after recovery.

Serial commands are deliberately small:

- `d`: known synthetic diagnostic frame through canonical encoder/network path;
- `l`: live IMU mode;
- `r`: fresh stationary calibration;
- `?`: help.

Diagnostic mode is intentionally allowed to operate without a healthy sensor so network/WLED problems can be isolated. It is explicit user-selected behavior, not a fallback that hides sensor failure.

### `tools`

Host utilities are part of the compatibility strategy, not optional developer toys.

WU-002 implements:

- `lib/WledImuUdpHost`: deterministic packet-pattern generation, CLI parsing, exact packet hex conversion, IPv4 handling and a thin native UDP socket adapter;
- `src/host_probe.cpp`: native `host_probe` with `send`, `listen` and `decode` modes.

WU-004 adds `src/mapping_probe.cpp`, a native inspection executable for representative `still`, `sway`, `spin`, `shake`, `impact`, `tilt-left` and `tilt-right` snapshots. It prints the Balanced-v1 semantic frame and exact packet emitted by the canonical WU-001 encoder.

`WledImuUdpCore` remains the only Audio Sync serializer/decoder. Native tool source files are excluded from the ESP32-S3 source filter so desktop socket/iostream code cannot leak into firmware.

WU-003's reusable synthetic motion traces remain test assets, not a runtime dependency of firmware or host tools.

## Timing model

Accepted WU-005 reference timing:

- QMI8658 accel + gyro ODR code `0101`: 224.2 Hz effective in 6-DoF mode;
- acquisition gate: 4460 us (~224 Hz target);
- motion feature update: every accepted IMU sample;
- WLED synthetic-audio frame generation/transmit: configurable, 50 Hz / 20 ms default;
- bounded serial status: 1 Hz.

WU-003 does not assume a perfect IMU scheduler. Motion state uses supplied monotonic timestamp deltas. WU-005's `MicrosExtender` converts the ESP32 32-bit `micros()` counter to monotonic 64-bit time across wrap before samples reach that core.

Fixed-rate gates advance over missed deadlines without issuing a burst of catch-up work. Runtime one-second counters report actual valid sample and successful packet counts; configured target cadence must not be misreported as physical measurement.

## Determinism

For identical initial configuration and identical timestamped IMU input, portable motion features, synthetic-audio frames and encoded packets remain repeatable on the host target.

WU-001 enforces deterministic encoding and a directly reviewable golden packet. WU-002 extends that contract to deterministic host patterns and exact-byte transport seams. WU-003 extends it to signal processing with explicit state and timestamp-driven filters. WU-004 completes the deterministic feature→mapping→packet chain.

WU-005 does not make Wi-Fi delivery itself deterministic. It adds deterministic/testable seams around the non-deterministic hardware edge: raw register decode, axis conversion, calibration qualification, timing gates, reconnect eligibility and send safety. Physical I2C/Wi-Fi behavior remains observable runtime evidence.

## Configuration model

Configuration remains separated into:

- board/sensor profile;
- Wi-Fi/network settings;
- sensor calibration/range/rate;
- motion thresholds/noise floors;
- synthetic-audio mapping profile;
- packet rate and multicast destination.

`config/wifi.example.hpp` is the committed template. `config/wifi.local.hpp` is ignored and may contain real credentials. The example also exposes multicast address/port, packet rate, reconnect interval and diagnostic-on-boot flag so a clean checkout compiles without secrets.

If the SSID remains `CHANGE_ME`, firmware intentionally makes no Wi-Fi connection attempt. This keeps CI credential-free without creating a second compile-only firmware path.

## Failure behavior

- Invalid/non-finite sensor values never alter accepted motion state.
- Non-monotonic timestamps are rejected without state advance.
- Missing/wrong QMI8658 identity or configuration failure remains visible over serial and is periodically retried.
- Any runtime sensor-read failure disables live packet eligibility immediately; recovery requires re-probe and fresh calibration.
- Moving/noisy calibration windows are rejected with an explicit reason and retried.
- Wi-Fi loss skips sends; motion processing may continue; reconnection does not reset calibration.
- Live mode cannot send unless sensor/calibration/current-feature gates are all true.
- Diagnostic mode can intentionally send the fixed known frame with no sensor, but only when selected explicitly.
- Stillness maps toward zero activity through the accepted motion/mapping semantics.

At the protocol boundary, WU-001 sanitises semantic values before encoding and the strict decoder rejects malformed wire packets. WU-002 exposes decoder/network errors through host tooling rather than coercing malformed captures.

## Resource posture

The embedded processing path uses fixed-size state and no project-owned steady-state heap allocation:

- 12-byte QMI motion read buffer;
- fixed motion/calibration/mapping state;
- 44-byte Audio Sync packet;
- fixed network/config values.

Arduino networking/driver internals may manage their own resources, but WLEDIMUUDP does not allocate strings/vectors/containers per sensor or packet tick. The sender's RGB matrix is absent from the runtime dependency graph.
