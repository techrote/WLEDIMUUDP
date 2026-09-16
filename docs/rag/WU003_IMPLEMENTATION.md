# WU-003 — Deterministic Motion Core Implementation Contract

## Purpose

WU-003 establishes the pure, sensor-independent motion feature layer used by later WLED mapping and hardware-integration work. The implementation lives in `lib/WledImuUdpMotion` and has no Arduino, Wi-Fi, UDP, WLED packet, or QMI8658 dependency.

The evidence in this document is deterministic host-trace and compile evidence. It is not physical IMU validation.

## Public data contract

`ImuSample` contains:

- `timestamp_us`: strictly increasing monotonic timestamp in microseconds;
- `accel_g`: calibrated acceleration in g;
- `gyro_dps`: calibrated angular rate in degrees/second;
- `valid`: upstream sample-validity flag.

Hardware-specific axis transforms, register handling, rate/range configuration, and raw-unit conversion remain the responsibility of the future IMU adapter.

`MotionFeatures` exposes:

- gravity vector, magnitude, and confidence;
- linear acceleration vector and deadbanded magnitude;
- jerk magnitude;
- gyro vector with calibrated bias removed;
- deadbanded angular-speed magnitude;
- normalized gravity orientation vector;
- instantaneous and smoothed composite motion energy;
- bounded impact latch state;
- stillness state and confidence;
- input-valid flag and accepted timestamp.

Rejected samples return the last accepted feature state with `input_valid=false`; they do not advance filter timestamps or internal state.

## Default calibration state

`MotionCalibration` defaults are:

- gyro bias: `(0, 0, 0)` dps;
- gravity reference: `1.0 g`;
- acceleration noise floor: `0.003 g`;
- gyro noise floor: `0.15 dps`.

`CalibrationAccumulator` accepts finite valid stationary samples and, after at least 64 samples by default, estimates:

- mean gyro bias vector;
- mean acceleration magnitude as gravity reference;
- acceleration-magnitude noise floor, clamped to at least `0.0005 g`;
- gyro residual noise floor, clamped to at least `0.01 dps`.

The accumulator is a primitive for later hardware startup calibration; WU-003 does not decide how the firmware proves the board is stationary.

## Locked default filter/configuration baseline

`MotionConfig` defaults are part of the WU-003 baseline:

| Parameter | Default |
|---|---:|
| gravity low-pass time constant | `0.35 s` |
| energy smoothing time constant | `0.18 s` |
| gravity-confidence tolerance | `0.25 g` |
| acceleration deadband multiplier | `2.5 × calibrated noise floor` |
| gyro deadband multiplier | `2.5 × calibrated noise floor` |
| jerk deadband | `0.50 g/s` |
| acceleration energy weight | `1.0` |
| jerk energy weight | `0.08` |
| angular energy weight | `0.0125` |
| stillness enter threshold | `0.050` |
| stillness exit threshold | `0.080` |
| stillness hold | `0.45 s` |
| impact acceleration threshold | `0.65 g` |
| impact jerk threshold | `8.0 g/s` |
| impact release acceleration | `0.20 g` |
| impact latch | `0.08 s` |
| impact refractory interval | `0.22 s` |
| maximum accepted filter step | `0.10 s` |

These are mapping-independent feature-extraction defaults. Later tuning may change them only with explicit regression evidence and corresponding RAG reconciliation.

## Elapsed-time semantics

All state evolution is driven by accepted timestamp deltas.

The gravity and energy filters use the first-order coefficient:

`alpha = dt / (tau + dt)`

where `dt` is the accepted elapsed time and `tau` is the configured time constant. Positive `dt` is capped by `max_dt_s` before filter updates so long scheduling gaps do not cause uncontrolled state jumps.

The first accepted sample initializes gravity directly from acceleration and therefore does not fabricate startup dynamic acceleration or jerk.

A sample is rejected without state advance when:

- `valid == false`;
- any acceleration or gyro component is NaN/Inf;
- its timestamp is not strictly greater than the previous accepted timestamp.

## Feature semantics

Gravity is an elapsed-time low-pass estimate of acceleration. Linear acceleration is `accel - gravity`.

Acceleration and angular magnitudes are deadbanded using calibrated noise floors. Jerk is the magnitude of the change in linear acceleration divided by elapsed time, followed by the configured jerk deadband.

Instantaneous motion energy is the non-negative weighted sum:

`linear_accel * 1.0 + jerk * 0.08 + angular_speed * 0.0125`

using the configurable weights above. Smoothed energy is a separate elapsed-time low-pass value.

Orientation is the normalized gravity vector. Static tilt therefore changes orientation without requiring persistent motion energy.

Stillness uses hysteresis:

- energy at or below `stillness_enter_threshold` accumulates quiet elapsed time;
- energy at or above `stillness_exit_threshold` resets that quiet timer;
- `still=true` after the quiet timer reaches `stillness_hold_s`;
- confidence is the bounded fraction of the hold interval already accumulated.

Impact requires both the configured linear-acceleration and jerk thresholds while outside the refractory window. A trigger asserts the latch for `impact_latch_s` and prevents retriggering for `impact_refractory_s`.

## Deterministic fixture corpus

Reusable fixtures are in `test/fixtures/motion_traces.hpp` and cover nine required families:

1. stationary level;
2. stationary tilted;
3. slow roll;
4. translational sway;
5. constant-rate spin;
6. tap/impact;
7. shake;
8. motion followed by long stillness;
9. malformed/non-finite sample injection.

Each generated trace contains at least 400 timestamped samples and is intended for reuse by WU-004 mapping regressions.

## Automated evidence

WU-003 adds 11 named native tests covering:

- complete/reusable fixture corpus;
- stationary convergence;
- tilted stillness without false motion;
- rotation/translation distinction;
- bounded impact latch/refractory behavior;
- predictable motion-to-stillness decay;
- malformed sample rejection without state poisoning;
- non-monotonic timestamp rejection without state advance;
- calibration estimates;
- exact repeatability for a fixed timestamped trace;
- finite/still behavior under deterministic 4.5/5.5 ms timing jitter.

Combined with accepted WU-001 and WU-002 suites, the WU-003 branch contains 31 named native tests.

Implementation head `24ff0e4be7b01fb406aedb0c5baabc4f5790b3d3` passed GitHub Actions run `35039697092`: strict formatting, all native tests, host-probe build, real host CLI smoke execution, and the ESP32-S3 reference firmware build.

A later documentation-only head must pass the same complete gate before merge.

## Evidence boundary and non-goals

WU-003 proves deterministic feature semantics on synthetic traces and portable compilation. It does not prove QMI8658 noise characteristics, physical tap thresholds, board-axis orientation, real sampling jitter, WLED visual usefulness, Wi-Fi behavior, or stock-WLED interoperability.

Those remain respectively for WU-005/WU-006 and later tuning work. WU-004 may consume `MotionFeatures`, but must not move synthetic-spectrum policy back into this library.