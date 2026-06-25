# Roadmap

Feasible, not-yet-built features, roughly in priority order. Hardware limits that
put things permanently out of scope are at the bottom.

## Built, pending hardware verification

- **IMU expression** — velocity (accel tilt, latched per note-on), vibrato
  depth → CC1 (gyro/accel side-tilt), pitch bend (gyro twist). Replaced the
  redundant gx-amp / gy-volume pair.
- **1-pole low-pass filter** — runtime, cutoff on `,` / `.` keys, open by
  default. Tames bright saw/square; first step toward a cutoff-sweep mod source.
- **Per-waveform loudness matching** — the four shapes leveled by ear.

## Planned

- **Sample recording via mic** — the ADV has a MEMS mic + ES8311 ADC. Record a
  short sample on-device, store it, and play it back as the oscillator source
  (single-cycle wavetable or one-shot). Turns the synth into a rough sampler.
  Needs: mic capture path, sample storage (RAM/flash/SD), and an oscillator mode
  that reads the captured buffer instead of `osc()`.
- **Arpeggiator** — hold keys → auto-cycle them (up / down / up-down / random)
  at a clock rate. Cheap, very playable standalone. Pure event logic, no DSP
  cost.
- **Looper / step sequencer** — record note *events* (not audio) to RAM, loop
  them, play live on top. Cheap (events are tiny). Turns the synth into a
  self-contained jam box. Arp + looper share one internal clock — build the
  clock once, both ride it (a metronome falls out of the same clock).
- **2-op FM** (DX7-style) — feasible within the SR/CPU budget; adds metallic /
  bell timbres beyond the four basic waveforms.
- **User-remappable MIDI CC** — let the player assign which gyro/accel axis
  drives which CC, instead of the fixed mapping.
- **On-device parameter editing** — a settings/edit mode on the Cardputer to
  tune the values that are compile-time constants today (vibrato depth + LFO
  rate, per-waveform loudness gains, ADSR, filter range, IMU tilt sensitivity,
  pitch-bend range) and **persist** them across reboots via ESP32 NVS /
  `Preferences`. The natural home for everything currently dialed in by reflash.

## Permanently out of scope

ESP32-S3 has no SIMD and runs at 16 kHz here. Vital/Surge-class DSP — spectral
warping, large mod matrices, ZDF filters, polyphony > 2 — is not feasible. See
the host-side constraints in the project notes.

## Verifying on hardware

Each feature ships behind the on-device gate — see
[hardware-verification.md](hardware-verification.md). Nothing is "done" until
flashed and heard on a real Cardputer ADV.
