# Roadmap

Feasible, not-yet-built features, roughly in priority order. Hardware limits that
put things permanently out of scope are at the bottom.

## Shipped

- **Polyphonic engine** — 6-voice pool, oldest-voice stealing, MIDI note-off
  correctly released on steal + panic (no stuck notes on external gear).
- **Per-voice resonant filter** — state-variable filter (LP/HP/BP), runtime
  cutoff (`,`/`.`) and resonance (`6`/`7`); coefficients decimated to block
  rate so it doesn't starve the audio buffer.
- **Dual envelopes, dual LFOs, modulation matrix** — env/LFO/velocity/key-track
  routable to pitch, cutoff, resonance, and amp.
- **Anti-aliased oscillators** — saw/square are PolyBLEP band-limited (the
  naive versions aliased hard at this sample rate).
- **Mic sampler, tape + granular pitch-shift** — record ~1 s from the mic;
  play it back either varispeed (pitch/speed coupled) or granular (pitch
  follows the note, length/speed independent).
- **IMU expression** — accel tilt → velocity (latched) + vibrato depth; gyro
  twist → self-centering pitch bend.
- **MIDI out** — USB-MIDI and BLE-MIDI: note on/off, CC1 (vibrato), pitch bend.
- **Arpeggiator** — `Fn+a` toggle, Up/Down/Up-Down/Random (`Fn+s`), tempo
  (`Fn+-`/`Fn+=`); owns note lifecycle from the held-key set while active.
- **Desktop audio harness** (`host/`) — play the synth on a PC for fast
  by-ear iteration without a flash cycle.

All of the above is host-tested (`src/synth/*_test.cpp`, plain g++) and
compiles across all three build envs (`synth`, `synth-usb-midi`,
`synth-ble-midi`). Flash-verification per feature tracked in
[hardware-verification.md](hardware-verification.md).

## Planned

- **SD save/load (patches + patterns)** — everything is RAM-only today;
  power off and it's gone. Serialize a `Patch` (osc/filter/env/LFO/mod-matrix
  state) to a file on SD, load it back. Highest-value gap — makes the
  instrument feel like an instrument instead of a toy that forgets itself.
- **Per-note slide / portamento** — a glide time between consecutive notes on
  a voice, instead of an instant pitch jump. Small: one glide-rate constant +
  a per-voice "target pitch" that eases toward the new note.
- **Master FX send (delay / chorus / drive)** — one or two send effects on the
  summed bus. Currently the only "effect" is the soft limiter; this is the
  biggest lever left on richness/warmth without touching the engine itself.
- **Step sequencer + song mode** — record a pattern of note events (not
  audio) per track, loop it, chain patterns into a song. Turns the synth into
  a self-contained groovebox rather than a played-live instrument only.
- **2-op FM** — feasible in the CPU/SR budget; adds metallic/bell/e-piano
  timbres beyond the four subtractive waveforms.
- **On-device parameter editing + NVS persistence** — an edit mode to tune
  values that are compile-time constants today (vibrato depth/rate,
  per-waveform loudness gains, ADSR times, filter range, IMU sensitivity,
  bend range, arp tempo range) and persist them across reboots. The natural
  home for everything currently dialed in by reflash.
- **User-remappable MIDI CC** — let the player assign which gyro/accel axis
  drives which CC, instead of the fixed mapping.
- **Looper** — record a live performance (note events, not audio) and play it
  back layered under new playing. Shares the arp's clock/timing plumbing.

## Permanently out of scope

ESP32-S3 has no SIMD and a single-precision FPU only (double-precision DSP is
software-emulated and too slow for the audio path — learned the hard way when
a per-sample double-precision filter starved the buffer). Running at 32 kHz.
Vital/Surge-class DSP — spectral warping, large wavetables, ZDF filters,
polyphony beyond what fits the per-sample budget here — is not feasible.

## Verifying on hardware

Each feature ships behind the on-device gate — see
[hardware-verification.md](hardware-verification.md). Nothing is "done" until
flashed and heard on a real Cardputer ADV.
