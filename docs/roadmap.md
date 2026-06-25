# Roadmap

Feasible, not-yet-built features, roughly in priority order. Hardware limits that
put things permanently out of scope are at the bottom.

## In progress

- **IMU expression redesign** — split params across the right sensor:
  velocity (accel tilt, latched per note-on), vibrato depth → CC1 (the #1
  mod-wheel use), pitch bend (gyro twist, done). Replaces the redundant
  gx-amp / gy-volume pair. *(design agreed; implementation pending)*

## Planned

- **Sample recording via mic** — the ADV has a MEMS mic + ES8311 ADC. Record a
  short sample on-device, store it, and play it back as the oscillator source
  (single-cycle wavetable or one-shot). Turns the synth into a rough sampler.
  Needs: mic capture path, sample storage (RAM/flash/SD), and an oscillator mode
  that reads the captured buffer instead of `osc()`.
- **1-pole lowpass filter** — cheap, feasible. Unlocks the #2 mod-wheel use
  (CC1 / a gyro axis → cutoff sweep).
- **2-op FM** (DX7-style) — feasible within the SR/CPU budget; adds metallic /
  bell timbres beyond the four basic waveforms.
- **User-remappable MIDI CC** — let the player assign which gyro/accel axis
  drives which CC, instead of the fixed mapping.

## Permanently out of scope

ESP32-S3 has no SIMD and runs at 16 kHz here. Vital/Surge-class DSP — spectral
warping, large mod matrices, ZDF filters, polyphony > 2 — is not feasible. See
the host-side constraints in the project notes.

## Verifying on hardware

Each feature ships behind the on-device gate — see
[hardware-verification.md](hardware-verification.md). Nothing is "done" until
flashed and heard on a real Cardputer ADV.
