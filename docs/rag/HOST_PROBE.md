# WLEDIMUUDP — Host Audio Sync Probe

## Purpose

WU-002 provides a native host-side interoperability harness for WLED Audio Sync V2. It lets protocol, multicast, receiver configuration and captured packets be tested independently of the future ESP32-S3/QMI8658 sender.

The host tool always uses the canonical `WledImuUdpCore` encoder/decoder. It does not contain a second packet implementation.

## Build

Install the pinned development dependencies and build the native probe:

```powershell
python -m pip install -r requirements-dev.txt
python -m platformio run -e host_probe
```

On Windows the executable is `.pio\build\host_probe\program.exe`. On Linux/macOS the PlatformIO native path is `.pio/build/host_probe/program`.

## Commands

### Send deterministic Audio Sync V2 traffic

The default command sends one complete scripted cycle to stock WLED's default Audio Sync multicast destination:

```powershell
.\.pio\build\host_probe\program.exe send
```

Defaults are `239.0.0.1:11988`, 50 packets/second, 160 packets and the `scripted` pattern.

Useful probes:

```powershell
.\.pio\build\host_probe\program.exe send --pattern single-band --frames 320
.\.pio\build\host_probe\program.exe send --pattern peak-pulse --rate 25 --frames 80
.\.pio\build\host_probe\program.exe send --dry-run --pattern two-band --frames 4
```

`--rate` accepts `1..50` Hz. WU-002 deliberately refuses faster rates because current WLED UDP Sound Sync documentation says external senders may be slower than the approximately 20 ms cadence but should not send faster than it.

`--dry-run` prints the exact encoded bytes and the decoded semantic fields without opening a network socket.

### Listen to Audio Sync multicast

The listener joins the selected IPv4 multicast group, validates each datagram with the canonical decoder, and reports packet timing, source and semantic fields:

```powershell
.\.pio\build\host_probe\program.exe listen --count 20
```

Defaults are group `239.0.0.1`, port `11988`, 10 packets and a 3000 ms receive timeout. Example:

```powershell
.\.pio\build\host_probe\program.exe listen --group 239.0.0.1 --port 11988 --count 100 --timeout-ms 5000
```

A receive timeout or socket error returns non-zero. Malformed V2 packets are reported rather than silently interpreted. Binding the same multicast port alongside another local application is OS/network-stack dependent, so packet capture software remains useful when local port sharing is unsuitable.

### Decode captured/raw packets

The committed WU-001 golden packet can be decoded directly:

```powershell
.\.pio\build\host_probe\program.exe decode --hex "30303030320000000000803f000020400100000102030405060708090a0b0c0d0e0f0000000080410000dc43"
```

That string is generated from the repository's exact 44-byte golden fixture, not a separate documentation-only packet. Whitespace, `:` and `-` separators are accepted; after separators are removed there must be exactly 44 bytes.

A binary file may contain one or more concatenated 44-byte packets:

```powershell
.\.pio\build\host_probe\program.exe decode --file .\capture.bin
```

Every packet is passed through the same strict decoder used by native tests.

## Deterministic probe patterns

| Pattern | Intended diagnostic signal |
|---|---|
| `silence` | quiet/decay-safe baseline |
| `low` | constant low level and spectrum |
| `medium` | constant medium level and spectrum |
| `high` | constant high level and spectrum |
| `ramp` | increasing level/spectrum and major peak |
| `single-band` | one moving GEQ band |
| `two-band` | opposed moving spectral pair |
| `broadband-pulse` | periodic broadband activity burst |
| `peak-pulse` | bounded one-frame `samplePeak` pulse with high-band transient |
| `major-peak-sweep` | moving spectral locus with sweeping major peak |
| `scripted` | fixed sequence containing all ten patterns above |

The scripted cycle is exactly ten 16-frame segments in the table order above: silence → low → medium → high → ramp → single-band → two-band → broadband-pulse → peak-pulse → major-peak-sweep. It repeats after 160 frames. Pattern generation depends only on `(pattern, frame_index)` and is byte-deterministic.

## Stock WLED receiver setup

This procedure was source/documentation-verified on **2026-09-16** against:

- WLED `main` commit `06ae26db67107cb3f6a3d107a92340035991a063`;
- latest stable GitHub release observed on that date: **v16.0.1**;
- official Audio Reactive documentation: https://kno.wled.ge/advanced/audio-reactive/;
- official UDP Realtime / Sound Sync documentation: https://kno.wled.ge/interfaces/udp-realtime/.

No physical WLED receiver was available to the implementation environment, so **v16.0.1 is not yet a physically validated WLEDIMUUDP compatibility claim**.

For a stock WLED receiver:

1. Put the WLED device and the computer running the probe on a LAN where multicast can pass between them.
2. Open WLED's Audio Reactive settings.
3. Configure Audio Sync to **Receive** rather than Send. Surrounding UI wording can vary by release/build, so follow the receive option presented by the installed version rather than relying on a screenshot from another version.
4. Keep the Audio Sync UDP port at `11988` unless intentionally testing a custom port.
5. Select an audio-reactive effect on the receiver.
6. Start with `single-band`, then try `peak-pulse`, `ramp`, and the full `scripted` sequence.

The canonical default multicast group is `239.0.0.1`; current WLED source joins that group for Audio Sync reception.

## Troubleshooting order

If WLED does not react:

1. Run `send --dry-run` first to prove deterministic pattern generation and exact packet encoding without networking.
2. Confirm the WLED unit is configured to receive Audio Sync and sender/receiver use the same UDP port.
3. Check host firewall rules for outbound UDP and, when using `listen`, inbound UDP on the selected port.
4. Check AP/router settings that may suppress multicast, client-to-client traffic, IGMP membership or wireless isolation.
5. Use `listen` on a suitable second host or a packet capture to confirm `239.0.0.1:11988` traffic is present on the LAN.
6. Decode captured 44-byte payloads with `decode --hex` or `decode --file` before suspecting the future IMU/motion stack.

This order intentionally isolates packet/network/receiver faults before sensor code exists.

## Automated evidence

WU-002 native coverage includes deterministic generation for every named pattern; locked scripted ordering/cycle; canonical-encoder equivalence; CLI defaults, overrides and rejection; hex round-trip and malformed rejection; and a real localhost UDP datagram whose received bytes must equal the canonical packet exactly.

CI additionally builds the host executable, runs a real CLI dry-run smoke command and preserves the ESP32-S3 compile gate.

## Evidence boundary

WU-002 proves deterministic packet generation, host CLI behavior, local UDP transport and source-level compatibility with the inspected WLED implementation. It does **not** prove that a particular physical WLED device, Wi-Fi network or effect was tested. Record such evidence only after an actual receiver test, planned primarily for WU-006.
