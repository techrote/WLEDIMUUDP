# WU-006 — End-to-end integration and validation contract

## Scope and evidence boundary

WU-006 validates and hardens the complete accepted sender chain:

```text
QMI8658(C) -> MotionFeatures -> Balanced-v1 -> Audio Sync V2 -> multicast UDP -> stock WLED
```

The implementation environment used for WU-006 has no physical reference sender or stock-WLED receiver attached. Therefore this milestone records source-level compatibility, deterministic end-to-end regressions, firmware observability, failure semantics and an exact manual validation procedure. It does **not** claim physical QMI8658, RF, multicast, board-axis or stock-WLED effect validation.

Physical rows remain explicitly pending until someone records the named hardware, WLED version/settings, sender commit, network topology and observations.

## WLED compatibility re-verification

Re-verified on **2026-09-17**:

- WLED `main` remains commit `06ae26db67107cb3f6a3d107a92340035991a063`;
- the latest stable GitHub release remains **v16.0.1**;
- the previously inspected Audio Reactive V2 packet/receiver contract therefore has not changed since WU-002;
- WLEDIMUUDP retains the canonical 44-byte `00002\0` Audio Sync V2 payload, multicast group `239.0.0.1`, UDP port `11988` and intended maximum sender cadence of 50 Hz.

No packet ABI, destination default or mapping constant is changed by WU-006 merely to create activity.

## Runtime identity and bounded diagnostics

The reference firmware now identifies the integration contract at startup and in each bounded status record:

- firmware integration identity: `WLEDIMUUDP-WU006`;
- protocol identity: `AudioSync-V2/00002`;
- mapping identity: `Balanced-v1` / profile version `1`;
- reference board profile and QMI8658 configuration remain those locked by WU-005.

The default status cadence remains **1 Hz**. It intentionally does not print raw IMU samples at acquisition rate.

A status record exposes enough state to locate a failure boundary:

- IMU adapter health and accepted IMU samples during the last second;
- calibration state;
- feature validity, stillness/confidence and impact state;
- instantaneous/smoothed motion energy, linear acceleration magnitude, angular speed and jerk;
- mapped raw/smoothed level, one-shot peak, spectral magnitude and major peak;
- strongest synthetic band/value and total 16-band energy;
- generated and successfully sent packet rates plus lifetime totals;
- Wi-Fi state and local IP when connected;
- multicast destination and port;
- sensor, send, reconnect and skipped-send counters;
- firmware/protocol/mapping identities.

This makes the main stages independently visible without creating a second diagnostic signal pipeline.

## Reconnect-safe mapper progression

WU-005 correctly prevented transmission while Wi-Fi was unavailable, but frame generation and mapper state were previously also gated by Wi-Fi connectivity. That meant a one-shot mapper state transition occurring during an outage could be delayed until connectivity returned.

WU-006 separates two decisions:

1. `can_generate_frame(...)` depends on sensor/calibration/feature validity for live mode and intentionally permits the known diagnostic frame in diagnostic mode;
2. `can_emit_packet(...)` additionally requires Wi-Fi connectivity.

At the fixed packet cadence the live mapper therefore continues advancing while the network is offline. Packets are skipped, not queued, retried or burst later. A reconnect cannot itself manufacture or replay a stale impact peak. The fixed-rate gate still skips missed periods rather than catching up.

Sensor failure remains different from network loss: it immediately invalidates live feature/frame state, resets mapper state, pauses live generation/transmission and requires a successful re-probe plus fresh calibration.

## Balanced-v1 validation decision

WU-006 reviewed the deterministic trace/test evidence across the representative motion classes required by issue #6:

| Motion class | Deterministic evidence | Expected semantic result |
|---|---|---|
| stillness | stationary level trace | raw/smoothed level and spectrum decay to zero; no peak |
| tilted stillness | stationary tilted trace | orientation alone creates no energy |
| gentle/slow translation | translational sway trace | translation group dominates rotation |
| tilt while moving | orientation-shaping mapper regression with non-zero motion energy | tilt shifts existing within-group spectrum/major peak without changing raw/smoothed level |
| slow roll | slow-roll trace | controlled rotational activity, materially below shake transient energy |
| continuous spin | constant-spin trace | rotation group dominates translation |
| shake | shake trace | stronger shake/high transient groups than slow roll |
| flick/tap/impact | tap trace | exactly one `samplePeak` rising edge with transient energy |
| motion -> stillness | recovery trace | raw/smoothed activity, spectrum and peak clear |
| malformed input | malformed trace | finite/bounded output and no synthetic new peak |

The existing mapper→encoder replay regression also proves byte-identical output for a fixed trace. WU-006 adds an offline/reconnect state regression around the same mapper semantics.

There is no deterministic evidence that justifies changing the Balanced-v1 gains/full-scale constants at this milestone, and no physical stock-WLED receiver is available to supply perceptual counter-evidence. **Balanced-v1 is therefore retained unchanged.** This is a conservative validation decision, not a claim that the profile has been physically tuned across effects.

## Receiver absence, packet loss and ordering

The sender uses connectionless multicast UDP and has no receiver acknowledgement or session state.

Consequences:

- receiver absence is safe: the sender continues generating state and attempts sends when Wi-Fi is connected;
- individual packets are complete current-state Audio Sync snapshots; there is no delta/retransmission protocol owned by WLEDIMUUDP;
- lost packets therefore reduce temporal sampling rather than corrupting sender core state;
- WLEDIMUUDP does not add sequence numbers or reorder buffering because those fields are not part of the accepted stock-WLED V2 ABI;
- multiple receivers may join the same multicast stream without sender-side per-receiver state, subject to the LAN/AP actually forwarding multicast.

Actual loss/reordering tolerance and multi-receiver behavior on a given network remain physical/network validation items.

## Stock WLED receiver validation procedure

Use the host probe first so receiver/network problems can be separated from live IMU problems.

1. Confirm the receiver is stock WLED and record its exact release/build.
2. Put sender/probe and WLED on a LAN that permits multicast/client-to-client traffic.
3. Configure WLED Audio Sync to **Receive** using UDP port `11988` unless deliberately testing a configured alternate port.
4. Select an audio-reactive effect.
5. Run the host probe `single-band`, `peak-pulse`, `ramp` and `scripted` patterns. Record whether each produces the expected changing response.
6. If host patterns fail, diagnose receiver settings, firewall/AP isolation, multicast/IGMP behavior and packet presence before involving the IMU.
7. Flash the reference sender, complete stationary calibration, and use serial command `d` to send the fixed known diagnostic frame through the firmware/network path.
8. Use `l` for live mode and exercise stillness, gentle sway, tilt while moving, slow roll, continuous spin, shake, tap/flick and motion-to-stillness recovery.
9. Repeat live motion on several materially different stock Audio Reactive effects; do not tune solely to one effect.
10. Disconnect/reconnect Wi-Fi or temporarily remove receiver reachability and verify there is no generated reconnect burst or stuck peak/activity.
11. Where practical, add a second stock-WLED receiver to the same multicast stream and record the result.

## Physical compatibility record

Current state from the WU-006 implementation environment:

| Validation item | Status | Required evidence before marking passed |
|---|---|---|
| Waveshare ESP32-S3-Matrix live QMI8658 sampling/rates | **PENDING — hardware unavailable** | board identity, sender commit, observed 1 Hz counters |
| board-axis sign/orientation | **PENDING — hardware unavailable** | named orientation procedure and observations |
| sensor noise/clipping / calibration thresholds | **PENDING — hardware unavailable** | measured stationary/motion observations |
| stock WLED v16.0.1 receiver compatibility | **PENDING — receiver unavailable** | device/build, receive settings, sender/probe revision, topology and observations |
| several stock Audio Reactive effects | **PENDING — receiver unavailable** | named effects and motion-class observations |
| Wi-Fi reconnect behavior on hardware | **PENDING — hardware unavailable** | disconnect/reconnect procedure and serial/network observations |
| multicast packet loss/reordering behavior | **PENDING — network/hardware unavailable** | capture/topology and observed behavior |
| multiple multicast receivers | **PENDING — receivers unavailable** | receiver versions/topology and simultaneous result |

These pending rows are not release failures under issue #6 because the issue explicitly permits unavailable physical evidence to remain pending when source/host integration, diagnostics, regressions and the manual checklist are complete.

## Automated WU-006 additions

WU-006 adds portable checks for:

- explicit firmware and protocol identity;
- network-independent frame-generation eligibility versus network-dependent transmission eligibility;
- deterministic bounded spectrum summarisation;
- mapper state progression during Wi-Fi loss and absence of stale `samplePeak` replay after reconnect.

All inherited protocol, host-tool, motion, mapping, firmware-support and ESP32-S3 build gates remain required.

## Non-goals retained

WU-006 does not add custom WLED code, ESP-NOW, realtime RGB streaming, sender LED effects, microphone input, retransmission/acknowledgement protocol or effect-specific packet formats.
