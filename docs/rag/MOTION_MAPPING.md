# WLEDIMUUDP — Motion Feature and Synthetic-Audio Mapping Contract

## Purpose

This document separates two concerns that must remain independently tunable:

1. **motion feature extraction** — what physically happened to the sender;
2. **synthetic-audio mapping** — how that motion should be represented to WLED Audio Reactive effects.

The project must never collapse these into one opaque chain of magic constants.

WU-003 now locks the first concrete motion-feature baseline. Exact public fields, default constants, elapsed-time semantics and acceptance evidence are recorded in `docs/rag/WU003_IMPLEMENTATION.md`.

## Input sample contract

The host-testable motion core consumes `ImuSample` values containing:

- `timestamp_us`: strictly increasing monotonic microseconds;
- `accel_g`: calibrated acceleration X/Y/Z in g;
- `gyro_dps`: calibrated gyro X/Y/Z in degrees/second;
- `valid`: upstream sample-validity flag.

Hardware-specific axis swaps, sign corrections, scale conversion, register behavior and sensor range/rate selection belong in the IMU adapter/configuration layer, not in generic feature extraction.

Malformed/non-finite or non-monotonic samples are rejected without advancing motion filter state. The returned snapshot preserves the last accepted features with `input_valid=false`.

## Calibration

WU-003 provides `MotionCalibration` plus a stationary `CalibrationAccumulator` capable of estimating:

- gyro zero/bias vector;
- gravity-reference magnitude;
- acceleration-magnitude noise floor;
- gyro residual noise floor.

The default accumulator requires 64 valid finite samples. WU-003 deliberately does not decide how later firmware proves the board is stationary before accepting startup calibration samples.

Calibration must never silently depend on the board’s LEDs or display hardware.

## Motion feature extraction

`MotionFeatures` exposes:

- estimated gravity vector;
- gravity magnitude/confidence;
- dynamic/linear acceleration vector (`accel - gravity`);
- deadbanded dynamic acceleration magnitude;
- jerk/rate-of-change magnitude;
- gyro/angular-velocity vector after calibrated bias removal;
- deadbanded angular-speed magnitude;
- normalized gravity orientation vector;
- instantaneous composite motion energy;
- smoothed motion energy;
- bounded impact/tap latch state;
- stillness state/confidence;
- accepted timestamp and input-validity state.

The core lives in `lib/WledImuUdpMotion` and has no Arduino, Wi-Fi, UDP, WLED-packet or QMI8658 dependency.

## Locked WU-003 filtering baseline

WU-003 uses simple fixed-state elapsed-time filters. The first-order smoothing coefficient is:

`alpha = dt / (tau + dt)`

with positive accepted `dt` capped at `0.10 s` before state updates.

Default configuration:

- gravity low-pass time constant: `0.35 s`;
- motion-energy smoothing time constant: `0.18 s`;
- gravity-confidence tolerance: `0.25 g`;
- acceleration deadband: `2.5 ×` calibrated acceleration noise floor;
- gyro deadband: `2.5 ×` calibrated gyro noise floor;
- jerk deadband: `0.50 g/s`;
- composite energy weights: acceleration `1.0`, jerk `0.08`, angular speed `0.0125`;
- stillness enter/exit thresholds: `0.050` / `0.080`;
- stillness hold: `0.45 s`;
- impact thresholds: `0.65 g` linear acceleration and `8.0 g/s` jerk;
- impact release acceleration: `0.20 g`;
- impact latch: `0.08 s`;
- impact refractory interval: `0.22 s`.

The first accepted sample initializes gravity directly from measured acceleration and therefore does not fabricate startup dynamic acceleration or jerk.

These values are the WU-003 deterministic baseline, not claims of final physical-QMI8658 tuning. Changes require trace evidence and documentation reconciliation.

## Stillness behavior

Stillness is a first-class product state.

Current behavior:

- static orientation alone does not create continuous motion energy;
- calibrated noise floors define acceleration/gyro deadbands;
- energy at or below the enter threshold accumulates quiet elapsed time;
- energy at or above the exit threshold resets that timer;
- `still=true` after `0.45 s` of accumulated quiet time;
- stillness confidence is the bounded fraction of that hold interval;
- smoothed energy decays predictably during quiet input;
- invalid samples do not poison or advance state;
- outputs remain finite for the deterministic fixture corpus.

Synthetic bands, `sampleRaw`, `sampleSmth` and `samplePeak` remain WU-004 responsibilities.

## Synthetic-audio frame

The mapper consumes `MotionFeatures` and produces:

- `sampleRaw` — immediate composite motion energy;
- `sampleSmth` — perceptually useful smoothed energy;
- `samplePeak` — short peak/impact event;
- `fftResult[16]` — synthetic 16-band spectrum;
- `FFT_Magnitude` — overall spectral/activity magnitude;
- `FFT_MajorPeak` — frequency-like descriptor derived from the synthetic spectrum.

The mapping is intentionally not a physical acoustic FFT.

## Planned 16-band semantics

The initial reviewed mapping uses broad motion semantics instead of one-to-one axes:

- **bands 0–3:** slow/sweeping/translational movement energy;
- **bands 4–7:** rotation/roll/yaw-dominant movement;
- **bands 8–11:** shake/jerk/high-frequency movement;
- **bands 12–15:** impacts, flicks and sharp transients.

Within those groups:

- tilt/orientation may bias where energy sits left-to-right across nearby bands;
- direction may shape the spectrum but should not create energy when the sender is motionless;
- fast rotation should look different from linear shake even when total energy is similar;
- impacts should produce a broadband/high-band transient and assert `samplePeak` briefly.

The exact band weights are not frozen by WU-003. They become a versioned WU-004 contract when implementation fixtures demonstrate useful behavior across stock WLED effects.

## Major-peak semantics

`FFT_MajorPeak` is consumed by some WLED effects as a frequency-like quantity. Because the spectrum is synthetic, WLEDIMUUDP should derive it from a documented canonical center table or weighted centroid rather than inventing arbitrary per-packet values.

Requirements:

- deterministic for identical frames;
- finite and non-negative;
- stable during quiet output;
- moves coherently as the synthetic spectrum moves;
- center-table values and quiet value must be locked by tests once chosen.

## Mapping profiles

The architecture should allow multiple mapping profiles without duplicating the protocol/network stack. Candidate profiles after MVP include:

- **Balanced** — default mixture of translation, rotation, shake and impact;
- **Flow** — emphasises tilt-shaped sweeps and slow movement;
- **Percussive** — emphasises jerk/taps/impacts;
- **Spin** — emphasises angular motion and spectral movement.

Only one conservative default profile is required for the MVP. Profiles must be data/configuration over the same tested feature primitives, not unrelated implementations.

## Synthetic trace fixtures

WU-003 provides reusable deterministic traces for:

- stationary level orientation;
- stationary tilted orientation;
- slow roll;
- slow translational sway;
- constant-rate spin;
- short tap/impact;
- broadband shake;
- motion followed by long stillness;
- malformed/non-finite sample sanitisation.

Every generated trace contains at least 400 timestamped samples. WU-004 should reuse this corpus rather than introducing an unrelated motion language.

WU-003 tests lock the important feature invariants: tilt without false motion, rotation/translation distinction, bounded impact assertion, predictable stillness decay, malformed/non-monotonic rejection, deterministic replay, calibration behavior and finite convergence under deterministic 4.5/5.5 ms timing jitter.

## Tuning discipline

A mapping change that “looks better” on one effect may hurt other WLED audio-reactive effects. Therefore tuning PRs should:

- preserve trace fixtures;
- add evidence for any changed semantic;
- test multiple stock effects when hardware is available;
- avoid coupling effect-specific hacks into the generic motion core;
- version or document materially changed profile behavior.

Motion-core constants are likewise not effect knobs: change them only when feature-level evidence justifies the semantic change.

The project optimises for expressive stock-WLED interoperability, not acoustic correctness.