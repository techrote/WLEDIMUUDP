# WLEDIMUUDP — Project Contract

## One-sentence purpose

WLEDIMUUDP turns **physical motion from an IMU** into **synthetic WLED Audio Sync V2 UDP data** so that unmodified stock WLED receivers can react to movement using their existing audio-reactive effects.

## Product boundary

The sender is conceptually a motion instrument, not a light controller. It may have LEDs physically attached, but it does not need them and they are not part of the product contract.

The receiver is stock WLED configured to consume network audio-sync data. No WLED source modification, custom usermod, custom build, or receiver-side companion firmware is required.

## Primary user experience

A powered sender joins the same local Wi-Fi network as one or more WLED devices. Moving, tilting, spinning, flicking or shaking the sender produces a synthetic 16-band “audio” control stream. WLED’s own audio-reactive effects interpret that stream and animate normally.

The bridge should feel transparent: the WLED device should behave as though it were receiving external audio-analysis data, except the analysis was generated from motion rather than sound.

## MVP success criteria

The MVP is successful when all of the following are true:

- a stock WLED receiver in network/audio-sync receive mode responds to WLEDIMUUDP without a custom WLED build;
- stillness decays reliably toward silence rather than leaving stuck activity;
- tilt/orientation can shape the synthetic spectrum without creating false “volume” by itself;
- translational motion, shake, rotation and impacts produce visibly distinct responses across multiple stock WLED audio-reactive effects;
- packet encoding is exact, deterministic and covered by host-side golden tests;
- motion processing is deterministic for fixed input traces;
- the reference ESP32-S3 + QMI8658/QMI8658C firmware builds reproducibly;
- the sender does not require an LED output path;
- real Wi-Fi credentials are never committed.

## Non-goals

The initial project does not include:

- ESPsand integration;
- ESP-NOW transport;
- direct WLED realtime-RGB streaming;
- microphone or line-in audio capture;
- a custom WLED build, fork or usermod;
- controlling the sender board’s LEDs;
- cloud services, accounts or internet dependency;
- physically meaningful acoustic FFT reconstruction.

The 16-bin spectrum is intentionally synthetic. Its job is to drive WLED effects expressively and predictably, not to pretend that IMU motion is sound.

## Design principles

### Protocol truth before aesthetics

Exact stock-WLED compatibility is foundational. Motion mapping can evolve freely only if the packet encoder remains boring, explicit and testable.

### Motion semantics before fake frequencies

Feature extraction answers “what did the object do?” Synthetic-audio mapping answers “how should WLED perceive that motion?” These are separate modules.

### Stillness must be first-class

A motion-driven lighting bridge becomes annoying if it chatters while resting. Bias calibration, noise floors, hysteresis and decay are product behavior, not implementation details.

### Multiple receivers should just work

Audio Sync is multicast. WLEDIMUUDP should naturally support multiple stock WLED receivers on the same LAN without per-receiver pairing.

### Hardware is replaceable

QMI8658/QMI8658C on ESP32-S3 is the first reference target, not an architectural dependency.

## Terminology

- **IMU sample** — raw/calibrated accelerometer and gyroscope sample.
- **Motion features** — gravity estimate, linear acceleration, jerk, rotational energy, impact state, orientation and temporal measures derived from IMU samples.
- **Synthetic audio frame** — semantic level/peak/16-band/magnitude/major-peak values before byte encoding.
- **Audio Sync V2 packet** — the 44-byte stock-WLED-compatible UDP payload documented in `WLED_AUDIO_SYNC_V2.md`.
- **Reference sender** — the ESP32-S3 + QMI8658/QMI8658C firmware.
- **Stock receiver** — an unmodified WLED device configured for network audio-sync receive.