# WLEDIMUUDP — Motion Feature and Synthetic-Audio Mapping Contract

## Purpose

This document separates two concerns that must remain independently tunable:

1. **motion feature extraction** — what physically happened to the sender;
2. **synthetic-audio mapping** — how that motion should be represented to WLED Audio Reactive effects.

The project must never collapse these into one opaque chain of magic constants.

## Input sample contract

The host-testable motion core consumes timestamped samples containing at least:

- acceleration X/Y/Z in a stable physical unit;
- gyro X/Y/Z in a stable angular-rate unit;
- monotonic timestamp or elapsed time;
- validity flag / sensor status where relevant.

Hardware-specific axis swaps, sign corrections and scale conversion belong in the IMU adapter/configuration layer, not in generic feature extraction.

## Calibration

Initial calibration should support:

- gyro zero/bias estimate while stationary;
- accelerometer bias/scale assumptions documented per reference sensor;
- configurable board-axis transform;
- a stationary-noise estimate used to establish sensible deadbands.

Calibration must never silently depend on the board’s LEDs or display hardware.

## Motion feature extraction

The first implementation should expose a versioned `MotionFeatures` structure with at least:

- estimated gravity vector;
- gravity magnitude/confidence;
- dynamic/linear acceleration vector (`accel - gravity`);
- dynamic acceleration magnitude;
- jerk or rate-of-change magnitude;
- gyro/angular-velocity vector;
- angular-energy magnitude;
- normalised tilt/orientation components suitable for shaping the spectrum;
- motion-energy instantaneous value;
- smoothed motion-energy value;
- impact/tap candidate with hysteresis/latch semantics;
- stillness/confidence state.

## Filtering baseline

The exact constants are implementation work, but the initial design should use simple, explainable fixed-state filters:

- low-pass estimate for gravity;
- derived high-pass/dynamic acceleration from the gravity subtraction;
- EWMA or equivalent for smoothed motion energy;
- threshold + hysteresis/refractory interval for impacts;
- explicit decay toward zero during stillness.

Avoid heavyweight DSP until profiling or receiver behavior demonstrates a need.

All filters must use elapsed time or a well-defined fixed cadence and be deterministic under host fixtures.

## Stillness behavior

Stillness is a first-class product state.

Expected behavior:

- static orientation alone does not create continuous volume;
- small sensor noise is suppressed by calibration/noise-floor logic;
- after movement stops, `sampleRaw` falls quickly and `sampleSmth` decays predictably;
- synthetic bands decay to near-zero rather than freezing at their previous shape;
- `samplePeak` cannot remain latched indefinitely;
- the output never generates NaN/Inf values.

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

The exact band weights are not frozen by this planning document. They become a versioned contract when implementation fixtures demonstrate useful behavior across stock WLED effects.

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

Host tests should include deterministic traces for at least:

- stationary level orientation;
- stationary tilted orientation;
- slow roll;
- slow translational sway;
- constant-rate spin;
- short tap/impact;
- broadband shake;
- motion followed by long stillness;
- malformed/non-finite sample sanitisation.

Tests should check both qualitative invariants and exact deterministic outputs where appropriate.

Examples of useful invariants:

- stationary tilt changes orientation features but leaves motion energy near zero;
- spin produces more rotational-band energy than a matched translational sway;
- shake spreads energy more broadly than a slow roll;
- tap asserts exactly bounded peak frames;
- stillness decays output toward silence;
- no output band exceeds `254`.

## Tuning discipline

A mapping change that “looks better” on one effect may hurt other WLED audio-reactive effects. Therefore tuning PRs should:

- preserve trace fixtures;
- add evidence for any changed semantic;
- test multiple stock effects when hardware is available;
- avoid coupling effect-specific hacks into the generic motion core;
- version or document materially changed profile behavior.

The project optimises for expressive stock-WLED interoperability, not acoustic correctness.