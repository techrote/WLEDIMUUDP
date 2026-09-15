# WLED Audio Sync V2 Compatibility Contract

## Why this document exists

WLEDIMUUDP succeeds only if stock WLED accepts its packets as ordinary network audio-sync data. This file is therefore a protocol contract, not a loose design note.

## Upstream source of truth

Protocol details were re-verified against current WLED source on 2026-09-15 at commit:

`06ae26db67107cb3f6a3d107a92340035991a063`

Primary source:

- https://github.com/wled/WLED/blob/06ae26db67107cb3f6a3d107a92340035991a063/usermods/audioreactive/audio_reactive.cpp

Stock WLED Audio Reactive documentation:

- https://kno.wled.ge/advanced/audio-reactive/

Future implementation work must re-check upstream before intentionally changing compatibility assumptions.

## V2 payload

Current WLED declares its new Audio Sync V2 structure as a packed **44-byte** payload and identifies it with header `00002`.

The exact byte layout is:

| Offset | Size | Field | WLED type | WLEDIMUUDP rule |
|---:|---:|---|---|---|
| 0 | 6 | `header` | `char[6]` | ASCII `00002` followed by `\0` |
| 6 | 2 | `reserved1` | `uint8_t[2]` | zero |
| 8 | 4 | `sampleRaw` | `float` | finite IEEE-754 binary32, little-endian |
| 12 | 4 | `sampleSmth` | `float` | finite IEEE-754 binary32, little-endian |
| 16 | 1 | `samplePeak` | `uint8_t` | `0` or non-zero peak flag; project uses `0/1` |
| 17 | 1 | `reserved2` | `uint8_t` | zero |
| 18 | 16 | `fftResult` | `uint8_t[16]` | synthetic bands, clamp to `0..254` |
| 34 | 2 | `reserved3` | `uint16_t` | zero |
| 36 | 4 | `FFT_Magnitude` | `float` | finite binary32, little-endian |
| 40 | 4 | `FFT_MajorPeak` | `float` | finite binary32, little-endian |

WLED’s own transmitter constrains the 16 GEQ bytes to `0..254` and sends the packed structure directly as bytes. WLEDIMUUDP deliberately reproduces the observed wire format with an explicit encoder rather than depending on compiler packing.

## Endianness and float representation

Current WLED targets transmit native packed `float` fields directly. The relevant ESP32/ESP8266 receivers are little-endian and use 32-bit IEEE-754 floats.

WLEDIMUUDP therefore defines the V2 wire contract as explicit **little-endian IEEE-754 binary32** for all four-byte float fields. Host golden tests must verify exact bytes for known values.

If upstream WLED ever changes this ABI, that is a protocol-version event and must not be hidden inside a mapping tweak.

## Network destination

Current stock WLED defaults Audio Sync to:

- multicast group: `239.0.0.1`
- UDP port: `11988`

The WLED Audio Reactive code joins that multicast group when Audio Sync is enabled. The default project sender must target the same address/port, while allowing explicit configuration for advanced setups.

## Receiver configuration

The intended receiver is **stock WLED** with Audio Reactive / Audio Sync configured to receive network audio data. Current WLED source represents receive mode with the receive bit in `audioSyncEnabled` and includes a network-only digital-mic/input type.

The project must provide exact user-facing setup steps once the first interoperability issue has been validated against a stock WLED release. Do not guess UI wording across WLED versions; compatibility docs must name the tested release/source revision.

## Send cadence

Audio Sync V2 is a streaming control protocol, not a request/response protocol. WLED’s implementation is designed for frequent small datagrams. WLEDIMUUDP plans a **50 Hz** default synthetic-audio frame rate, with IMU acquisition at a higher rate.

The 50 Hz default is a project choice, not a claim that the protocol requires exactly 50 Hz. It must remain configurable and be validated for receiver smoothness and network behavior.

## Project semantic ranges

The wire format itself does not make IMU semantics meaningful. Until later tuning locks stronger contracts, WLEDIMUUDP uses these conservative project ranges:

- `sampleRaw`: finite `0..255` motion-energy scale;
- `sampleSmth`: finite `0..255` smoothed motion-energy scale;
- `samplePeak`: `0` or `1`;
- `fftResult[16]`: `0..254`;
- `FFT_Magnitude`: finite non-negative synthetic magnitude, initially normalised around `0..255`;
- `FFT_MajorPeak`: finite positive synthetic frequency-like value; quiet output must use a safe stable value rather than NaN/Inf.

Mapping/tuning may revise those project ranges only with tests and documentation. The packet offsets and encoded size remain protocol invariants.

## Encoder requirements

The canonical encoder must:

- always emit exactly 44 bytes;
- write the six-byte header exactly;
- zero all reserved bytes;
- sanitise non-finite floats;
- clamp all 16 band bytes to `0..254`;
- encode floats explicitly as little-endian binary32;
- never leak struct padding or uninitialised memory;
- produce deterministic golden bytes on host CI.

A decoder used by host tooling/tests should mirror the same layout but is not part of the sender’s runtime hot path.

## WU-001 locked implementation semantics

WU-001 implements the protocol layer in `lib/WledImuUdpCore` and separates the semantic `SyntheticAudioFrame` from the exact 44-byte wire packet.

The canonical encoder now has these deterministic sanitisation rules:

- non-finite or non-positive `sampleRaw` / `sampleSmth` become `0`; positive values clamp to `255`;
- all 16 input band values clamp to `254`;
- non-finite or non-positive `FFT_Magnitude` becomes `0`; positive values clamp to `255`;
- non-finite or non-positive `FFT_MajorPeak` becomes `1.0`;
- `samplePeak` is emitted as exactly `0` or `1`;
- the packet begins zero-initialised, so reserved bytes remain zero unless explicitly changed by a future protocol revision.

The implementation statically requires 8-bit bytes and IEEE-754 32-bit `float`, converts each float to its binary32 bit pattern, and writes that pattern explicitly in little-endian order. A blind packed-struct `memcpy` is not the canonical sender representation.

WU-001 also provides a strict reference decoder for tests and later host tooling. It rejects null input, wrong length/header, non-zero reserved bytes, non-finite floats, and non-positive major-peak values. It intentionally does not apply sender-side level clamps to otherwise valid received packets.

The committed golden fixture encodes `sampleRaw=1.0`, `sampleSmth=2.5`, peak asserted, bands `0..15`, magnitude `16.0`, and major peak `440.0`, and is compared byte-for-byte in native CI.

## Multicast behavior

Because the product uses multicast, one WLEDIMUUDP sender may drive multiple stock WLED receivers on the same LAN without pairing each receiver individually.

The protocol is local-network UDP:

- delivery is best-effort;
- packets may be lost, duplicated or reordered;
- there is no authentication or encryption;
- the project should be used on trusted local networks;
- sender behavior must tolerate Wi-Fi reconnects without corrupting signal-processing state.

## Compatibility policy

Initial implementation supports Audio Sync **V2 only**. Do not add V1 fallback until a real compatibility requirement justifies the maintenance burden.

Before a release claims stock-WLED compatibility, record:

- WLED release/version tested;
- receiver Audio Sync settings used;
- sender packet rate and network topology;
- at least one known-pattern packet capture or decoder trace;
- at least several stock audio-reactive effects that visibly respond.

Automated byte-level conformance is necessary but not a substitute for clearly labelled physical/receiver validation.