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

WU-006 makes a further runtime distinction between **frame generation** and **packet emission**. Valid live mapping continues at the fixed packet cadence while Wi-Fi is unavailable; only the network send is skipped. This keeps mapper state independent of reconnect events and prevents a stale one-shot peak from being delayed until reconnection.

WU-007 adds a release layer around this runtime without changing it: version/config sanity, clean-checkout bootstrap, deterministic packaging and exact artifact provenance remain outside the motion/network hot path.

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

Invalid/non-finite or non-monotonic samples are rejected without advancing motion filter state. The last accepted feature snapshot is returned with `input_valid=false`, making upstream data-quality failures observable without injecting false motion.

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

WU-006 reviews the representative deterministic motion-class evidence and retains Balanced-v1 unchanged. WU-007 preserves that mapping contract; release packaging is not a tuning event. Perceptual tuning against physical stock-WLED effects remains explicitly pending where hardware is unavailable.

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

WU-006 extends this portable seam with:

- explicit firmware/protocol runtime identity;
- separate `can_generate_frame(...)` and `can_emit_packet(...)` decisions;
- deterministic bounded spectrum summaries for diagnostics.

WU-007 stabilises the release identity as:

- project version `0.1.0`;
- release schema `1`;
- runtime firmware identity `WLEDIMUUDP/0.1.0`;
- protocol identity `AudioSync-V2/00002`.

Project version, protocol version and mapping version remain separate contracts. The exact source revision belongs in generated release provenance rather than a manually maintained firmware constant that could go stale after a squash merge.

This library has no Arduino/Wi-Fi/Wire dependency. It may depend on accepted protocol and motion data models because it is the integration-policy seam between platform adapters and firmware.

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

WU-005 uses Arduino `WiFi` station mode plus `WiFiUDP::beginPacket(multicast_ip, port)`, `write()` and `endPacket()`. In the pinned Arduino-ESP32 2.0.16 API used by this repository, multicast is selected by the destination IP; the implementation does **not** use a separate `beginPacketMulticast()` sender path. The platform layer never interprets Audio Sync fields: it receives the exact packet from `WledImuUdpCore` and writes those 44 bytes.

A reconnect attempt is made immediately when needed and then no more often than the configured interval (5 s default). While disconnected, motion processing and valid frame generation continue but packet sends are skipped. Reconnection itself does not reset calibration, reset mapper state, generate a catch-up burst or manufacture a frame.

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

WU-006 formalises two runtime gates:

```text
valid live sensor/calibration/features
    -> generate/update Balanced-v1 frame at packet cadence
    -> if Wi-Fi connected, encode/send current frame
       else skip network send without freezing mapper state
```

A sensor read error clears live feature/frame eligibility immediately, pauses live generation/transmission, schedules re-probe and requires fresh calibration after recovery. Network loss does not have that semantic effect on core state.

Serial commands are deliberately small:

- `d`: known synthetic diagnostic frame through canonical encoder/network path;
- `l`: live IMU mode;
- `r`: fresh stationary calibration;
- `?`: help.

Diagnostic mode is intentionally allowed to generate the known frame without a healthy sensor so network/WLED problems can be isolated. Actual packet emission still requires Wi-Fi connectivity. It is explicit user-selected behavior, not a fallback that hides sensor failure.

Status remains bounded at 1 Hz and exposes project/protocol/mapping identity, IMU/calibration/feature state, motion energies, mapped level/peak/spectrum summary, generated/sent rates, Wi-Fi/local IP/target and accumulated error/reconnect counters. High-rate raw IMU logging is not enabled by default.

### `tools`

Host utilities are part of the compatibility strategy, not optional developer toys.

WU-002 implements:

- `lib/WledImuUdpHost`: deterministic packet-pattern generation, CLI parsing, exact packet hex conversion, IPv4 handling and a thin native UDP socket adapter;
- `src/host_probe.cpp`: native `host_probe` with `send`, `listen` and `decode` modes.

WU-004 adds `src/mapping_probe.cpp`, a native inspection executable for representative `still`, `sway`, `spin`, `shake`, `impact`, `tilt-left` and `tilt-right` snapshots. It prints the Balanced-v1 semantic frame and exact packet emitted by the canonical WU-001 encoder.

`WledImuUdpCore` remains the only Audio Sync serializer/decoder. Native tool source files are excluded from the ESP32-S3 source filter so desktop socket/iostream code cannot leak into firmware.

WU-003's reusable synthetic motion traces remain test assets, not a runtime dependency of firmware or host tools.

### `release/package`

WU-007 adds release support without adding a runtime application layer:

- root `VERSION` is the canonical project-version text;
- `CHANGELOG.md` records release history and version semantics;
- `scripts/bootstrap.py` performs dependency inspection and safe non-overwriting local config creation;
- `scripts/release_check.py` enforces version/config/credential/generated-output repository sanity;
- `scripts/make_release_bundle.py` assembles already-built firmware/tools/docs and writes exact source/hash provenance;
- CI uploads the generated bundle only after all inherited verification/build gates pass;
- `dist/`, `.pio/`, virtual environments and local credentials are ignored generated state, never source.

The release layer may inspect source/build outputs but must not duplicate packet encoding, mapping, IMU or network logic.

## Timing model

Accepted reference timing:

- QMI8658 accel + gyro ODR code `0101`: 224.2 Hz effective in 6-DoF mode;
- acquisition gate: 4460 us (~224 Hz target);
- motion feature update: every accepted IMU sample;
- WLED synthetic-audio frame generation: configurable, 50 Hz / 20 ms default when live state is valid, independent of Wi-Fi state;
- UDP emission attempt: same packet gate, but only while Wi-Fi is connected;
- bounded serial status: 1 Hz.

WU-003 does not assume a perfect IMU scheduler. Motion state uses supplied monotonic timestamp deltas. WU-005's `MicrosExtender` converts the ESP32 32-bit `micros()` counter to monotonic 64-bit time across wrap before samples reach that core.

Fixed-rate gates advance over missed deadlines without issuing a burst of catch-up work. Runtime one-second counters report actual valid sample, generated-frame and successful-packet counts; configured target cadence must not be misreported as physical measurement.

## Determinism

For identical initial configuration and identical timestamped IMU input, portable motion features, synthetic-audio frames and encoded packets remain repeatable on the host target.

WU-001 enforces deterministic encoding and a directly reviewable golden packet. WU-002 extends that contract to deterministic host patterns and exact-byte transport seams. WU-003 extends it to signal processing with explicit state and timestamp-driven filters. WU-004 completes the deterministic feature→mapping→packet chain.

WU-005 does not make Wi-Fi delivery itself deterministic. It adds deterministic/testable seams around the non-deterministic hardware edge: raw register decode, axis conversion, calibration qualification, timing gates, reconnect eligibility and send safety. WU-006 further proves that network eligibility does not control mapper progression and that an impact peak occurring while offline is not replayed merely because Wi-Fi reconnects.

WU-007 makes release assembly deterministic with respect to an already-built source revision: the bundle declares that revision and hashes executable payloads. It does not claim byte-identical toolchain output across arbitrary operating systems/toolchains outside the pinned CI environment.

Physical I2C/Wi-Fi behavior remains observable runtime evidence rather than something CI can assert.

## Configuration model

Configuration remains separated into:

- user-local Wi-Fi/network settings;
- fixed reference board/sensor profile;
- accepted sensor calibration/range/rate policy;
- accepted motion thresholds/noise floors;
- Balanced-v1 mapping profile/configuration;
- packet cadence/destination;
- bounded diagnostic mode/status behavior.

`config/wifi.example.hpp` is the committed template. `config/wifi.local.hpp` is ignored and may contain real credentials. The local header exposes Wi-Fi credentials, multicast address/port, packet rate, reconnect interval and diagnostic-on-boot flag so a clean checkout compiles without secrets.

If the SSID remains `CHANGE_ME`, firmware intentionally makes no Wi-Fi connection attempt. This keeps CI credential-free without creating a second compile-only firmware path.

Release 0.1.0 intentionally does **not** move board axes, motion thresholds or mapping constants into the credential file. Those are semantic/source contracts that require tests and documentation when changed.

## Failure behavior

- Invalid/non-finite sensor values never alter accepted motion state.
- Non-monotonic timestamps are rejected without state advance.
- Missing/wrong QMI8658 identity or configuration failure remains visible over serial and is periodically retried.
- Any runtime sensor-read failure disables live frame/send eligibility immediately; recovery requires re-probe and fresh calibration.
- Moving/noisy calibration windows are rejected with an explicit reason and retried.
- Wi-Fi loss skips sends but does not freeze valid mapper progression; reconnection does not reset calibration or manufacture/replay a stale peak.
- Live mode cannot generate unless sensor/calibration/current-feature gates are all true and cannot emit unless Wi-Fi is also connected.
- Diagnostic mode can intentionally generate the fixed known frame with no sensor, but packet emission still requires Wi-Fi and the mode must be selected explicitly.
- Stillness maps toward zero activity through the accepted motion/mapping semantics.
- Receiver absence has no sender-side session state: multicast sends can continue safely with no acknowledgements.
- Missing local credentials do not break compilation; firmware simply does not attempt Wi-Fi connection.
- Existing local config is never overwritten by the bootstrap helper.
- Missing release inputs cause packaging/CI failure rather than a partial artifact.

At the protocol boundary, WU-001 sanitises semantic values before encoding and the strict decoder rejects malformed wire packets. WU-002 exposes decoder/network errors through host tooling rather than coercing malformed captures.

## Resource posture

The embedded processing path uses fixed-size state and no project-owned steady-state heap allocation:

- 12-byte QMI motion read buffer;
- fixed motion/calibration/mapping state;
- one fixed current synthetic frame plus a 44-byte Audio Sync packet;
- fixed network/config values.

Arduino networking/driver internals may manage their own resources, but WLEDIMUUDP does not allocate strings/vectors/containers per sensor or packet tick. The sender's RGB matrix is absent from the runtime dependency graph.

Release scripts and host tools may use normal desktop filesystem/string allocation because they are not embedded hot-path dependencies.
