# WLEDIMUUDP — Motion Feature and Synthetic-Audio Mapping Contract

## Purpose

This document separates two concerns that must remain independently tunable:

1. **motion feature extraction** — what physically happened to the sender;
2. **synthetic-audio mapping** — how that motion should be represented to WLED Audio Reactive effects.

The project must never collapse these into one opaque chain of magic constants.

WU-003 locks the first concrete motion-feature baseline. WU-004 now locks the first concrete mapping baseline, **Balanced-v1**. Exact mapping constants and evidence boundaries are also recorded in `docs/rag/WU004_IMPLEMENTATION.md`.

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

Current feature behavior:

- static orientation alone does not create continuous motion energy;
- calibrated noise floors define acceleration/gyro deadbands;
- energy at or below the enter threshold accumulates quiet elapsed time;
- energy at or above the exit threshold resets that timer;
- `still=true` after `0.45 s` of accumulated quiet time;
- stillness confidence is the bounded fraction of that hold interval;
- smoothed energy decays predictably during quiet input;
- invalid samples do not poison or advance state;
- outputs remain finite for the deterministic fixture corpus.

Balanced-v1 preserves these semantics at the synthetic-audio layer: stationary level and stationary tilted traces converge to zero raw level, zero spectrum, zero magnitude and quiet major peak; an invalid feature snapshot cannot manufacture a new peak.

## Synthetic-audio frame

`lib/WledImuUdpMapping` consumes `MotionFeatures` and produces the accepted protocol `SyntheticAudioFrame`:

- `sampleRaw` — `motion_energy_instant × 72`, bounded to `0..255`;
- `sampleSmth` — `motion_energy_smoothed × 72`, bounded to `0..255`;
- `samplePeak` — one-frame rising-edge event derived from the accepted WU-003 impact latch;
- `fftResult[16]` — synthetic 16-band spectrum, each value bounded to `0..254`;
- `FFT_Magnitude` — RMS amplitude of the 16 synthetic bands;
- `FFT_MajorPeak` — weighted centroid of the locked center table below, with quiet value `1.0`.

The mapping is intentionally not a physical acoustic FFT.

## Locked Balanced-v1 16-band semantics

Balanced-v1 uses broad motion semantics instead of one-to-one axes:

- **bands 0–3:** linear/translational acceleration, normalised against `0.22 g`, gain `1.00`;
- **bands 4–7:** rotation/angular speed, normalised against `120 dps`, gain `0.95`;
- **bands 8–11:** shake/jerk activity, normalised against `18 g/s`, gain `0.90`;
- **bands 12–15:** impact/transient activity, gain `1.00`, with `0.20 ×` normalised shake crossfeed.

Continuous sources are clamped to `0..1` before band synthesis. The high/transient group can therefore react to sharp broadband movement without asserting `samplePeak`; `samplePeak` is reserved for a rising edge of the actual WU-003 impact latch.

### Orientation shaping

Tilt/orientation biases **where existing energy sits within each four-band group**. It never creates energy by itself.

Balanced-v1 forms the signed shape control:

`orientation.x × 0.75 + orientation.y × 0.25`

clamps it to `-1..1`, multiplies it by the default orientation-bias strength `0.65`, and shifts a normalized triangular four-band distribution accordingly.

Tests lock that opposite tilts preserve the same raw/smoothed level while moving spectral energy and the resulting major peak coherently left/right. Stationary tilt remains silent.

## Locked major-peak semantics

The canonical Balanced-v1 center table is:

`[65, 92, 131, 185, 262, 370, 523, 740, 1047, 1480, 2093, 2960, 4186, 5920, 8372, 11840]`

For a non-zero spectrum:

`FFT_MajorPeak = sum(band[i] × center[i]) / sum(band[i])`

For a zero spectrum the default quiet value is `1.0`.

Requirements locked by tests:

- deterministic for identical frames;
- finite and positive;
- stable during quiet output;
- low-band energy produces a lower major peak than middle/high-band energy;
- exact single-band frames resolve to that band’s center value.

## Balanced-v1 configuration defaults

`MappingConfig` defaults are:

- profile: `kBalancedV1`;
- level gain: `72.0`;
- translation full scale: `0.22 g`;
- rotation full scale: `120 dps`;
- shake full scale: `18 g/s`;
- translation band gain: `1.00`;
- rotation band gain: `0.95`;
- shake band gain: `0.90`;
- impact band gain: `1.00`;
- shake→impact crossfeed: `0.20`;
- orientation bias strength: `0.65`;
- quiet major peak: `1.0`.

Configuration is explicit and deterministic. Changing it does not alter the WLED Audio Sync packet ABI.

## Mapping profiles

Balanced-v1 is the only implemented MVP profile. The architecture may later support additional profiles without duplicating the protocol/network stack. Candidate future profiles remain:

- **Flow** — emphasises tilt-shaped sweeps and slow movement;
- **Percussive** — emphasises jerk/taps/impacts;
- **Spin** — emphasises angular motion and spectral movement.

Any additional profile must be configuration/data over the same tested feature primitives rather than an unrelated signal pipeline.

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

WU-004 reuses this corpus directly. The mapper suite locks silence, spectral-class separation, bounded outputs, one-shot peaks, orientation shaping, deterministic configuration and byte-identical mapper→WU-001 encoder behavior.

## Host inspection

The native `mapping_probe` target accepts:

`still`, `sway`, `spin`, `shake`, `impact`, `tilt-left`, or `tilt-right`.

It prints the Balanced-v1 semantic frame, 16 bands and exact 44-byte Audio Sync V2 packet. This provides a concrete inspection seam before live IMU or network integration.

## Tuning discipline

A mapping change that “looks better” on one effect may hurt other WLED audio-reactive effects. Therefore tuning PRs should:

- preserve the WU-003 trace fixtures;
- add evidence for any changed semantic;
- test multiple stock effects when hardware is available;
- avoid coupling effect-specific hacks into the generic motion core;
- version or explicitly document materially changed profile behavior.

Motion-core constants are likewise not effect knobs: change them only when feature-level evidence justifies the semantic change.

Balanced-v1 is an automated deterministic baseline, not a claim of final physical perceptual tuning. WU-005/WU-006 own live sensor/network integration and physical stock-WLED evidence.

The project optimises for expressive stock-WLED interoperability, not acoustic correctness.
