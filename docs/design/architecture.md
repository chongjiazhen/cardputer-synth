# Cardputer ADV synth — full architecture

Design for the complete instrument. Built in phases, each flash-tested and
independently shippable; the **v0.1.0 stable build never regresses**. Supersedes
the earlier `full-build.md` sketch.

## Vision & non-goals

**Goal:** the maximal synthesizer this hardware can host — a polyphonic,
multi-timbral, **groovebox-capable performance synth**, built performance-first.
A device you can play on a spin today and grow into a beat machine.

**Non-goal — NOT Vital/Surge-tier** (established day 1, still true). No
FFT/spectral oscillators, no 4–16× oversampling, no 100-voice poly, no large
wavetables, no double-precision DSP. That class needs GHz + SIMD + GBs; this chip
has none. We get richness from **stacking cheap synthesis tricks + modulation**,
the Mutable-Plaits approach, not one heavy engine.

## Hardware envelope

| Resource | Value | Consequence |
|---|---|---|
| CPU | 2× Xtensa LX7 @240 MHz | ~15 k cycles/sample @16 kHz across both cores |
| FPU | single-precision only | **float32 in the audio path**; `double` is emulated (slow) |
| SRAM | 512 KB, **no PSRAM** | sample/wavetable/delay buffers are tight; budget RAM |
| Flash | 8 MB | firmware + small built-in tables; user data → SD |
| Codec | ES8311 (shared mic + speaker) | **cannot record + play at once** — switch begin/end |
| I/O | IMU (accel+gyro), microSD, RTC (RX8130), USB, BLE, IR, Grove | |

Audio: 256-sample blocks, 16 kHz now; budget keeps headroom to try 32 kHz later.

## Architecture layers

```
SONG        chain Patterns into a song                  ┐ groovebox
SEQUENCE    per-Part step sequencer; drum/sample Parts   ┘ (later phases)
PERFORMANCE keys · IMU expression · arp · looper         ← synth (early phases)
──────────────────────────────────────────────────────
ENGINE      Part[] → shared Voice pool → master FX → out
            sample-clock · mod-matrix · preset/NVS · SD · MIDI I/O · RTC
```

Each upper layer is **additive** over the engine — the groovebox grows on the
performance synth without rewriting it.

## Data model

- **Voice** — one sounding note: oscillator(s) + per-voice envelope(s) +
  per-voice resonant filter + per-voice modulation. Bound to a Part while active.
- **Part** — one timbre: owns a **Patch**, its allocated voices, and (later) a
  sequencer track. v1 = a single Part (the synth). Groovebox = N Parts (lead,
  bass, drums…).
- **Patch** — the sound design: osc config, filter, envelopes, LFOs, mod-matrix,
  FX sends. Serializable → SD + NVS.
- **Pattern / Song** — (groovebox phases) per-Part step data; chained patterns.
- **Voice pool** — **6 voices, global + shared** across Parts. note-on allocates
  a free voice or steals the oldest/quietest. Tax: a drum Part borrows from synth
  polyphony (drums often use short/sample voices, so manageable).

## Engine signal path (per voice, per sample)

```
osc(s)  [classic / wavetable / FM / sampler / noise]
  → per-voice resonant filter (SVF: LP / HP / BP, with resonance)
  → amp  (env1 × velocity × part level)
  → sum into master bus
master bus → master FX (delay / chorus / drive / bitcrush)
           → limiter → int16 → ES8311
```

**Modulation — a small fixed mod-matrix** of `{source → dest × depth}` entries.
- Sources: env1, env2, LFO1, LFO2, velocity, IMU (tilt / bend / mod-wheel),
  key-track.
- Dests: pitch, filter cutoff, amp, osc params (FM depth, PWM, wavetable pos),
  FX sends.

This *replaces* today's hardwired IMU→param mappings with routable ones (the
current velocity/vibrato/bend become default matrix entries).

### Output level & headroom (v0.1.0 tunings are preserved invariants)

The hard-won on-device tunings carry forward — they are not re-litigated:
- **Per-waveform loudness table** (`shapeGain`: tri 1.0 / sine 0.94 / saw 0.38 /
  square 0.24) — stays, applied per voice.
- **Safe analog ceiling** found on device (single-voice `AMP = 14000`, ≈ −6 dB)
  keeps the ES8311 + NS4150B + 1 W speaker out of clipping.

But `AMP = 14000` was a **single-voice** value. With up to 6 summed voices the
bus can reach ~6× and clip. So in the poly engine the level structure changes:
each voice outputs at its loudness-matched level, then the **master applies a
headroom gain + soft limiter** so the *bus peak* lands at the same safe analog
ceiling we found at v0.1.0. Net target: one voice ≈ as loud as v0.1.0; six
voices don't clip. Re-measure on device in phase 1.

Residual pure-tone (sine/tri) distortion is the 1 W speaker's THD — a hardware
limit, accepted.

## Timbre toolbox (how we get range from cheap tricks)

- **Subtractive** (phase 2) — rich wave (saw/pulse) carved by the resonant SVF +
  env/LFO on cutoff. The backbone.
- **FM, 2–4 op** (phase 4) — best range-per-cycle: bells, metallic, e-piano,
  growl. Pure math, no tables.
- **Wavetable scan** (phase 4) — small single-cycle tables, morph between them.
- **PWM** (phase 4) — LFO sweeps pulse width; nearly free, fat tones.
- **Hard sync / ring-mod / cross-mod** (phase 4) — cheap multiplies → aggressive,
  clangorous timbres.
- **Detune / unison + sub** (phase 4) — thickness (costs voices).
- **Noise → filter** (phase 4) — percussion, wind, hats.
- **Sampler** (phase 5) — record real sound → arbitrary timbre.
- **Waveshaping / drive / bitcrush** (phase 6) — nonlinear → added harmonics /
  lo-fi grit.
- **Mod-matrix** (phase 2) — the multiplier: routes movement onto all of the
  above, turning static patches into hundreds of sounds.

## Modules

Pure, host-testable headers (Arduino-free, g++ + `*_test.cpp`):
`oscillator.h`✓, `filter.h`✓ (extend to SVF), `adsr.h`✓, `lfo.h`, `modmatrix.h`,
`voice.h`, `part.h`, `voicealloc.h`, `clock.h`, `sampler.h`✓, `fx_delay.h`,
`fx_chorus.h`, `fx_shaper.h`, `sequencer.h`, `arp.h`, `patch.h` (serialize).

Hardware glue (in `main.cpp` / `cardputer_hw`): audio I/O, mic capture + codec
switch, SD, RTC, keyboard + Fn, display, MIDI USB/BLE.

## Control / UI

- **2-octave keymap:** lower octave `z x c v b n m` + `s d g h j` (existing);
  upper `q w e r t y u` (white) + `2 3 5 6 7` (sharps over the gaps).
- **Fn-layer:** number row now carries sharps, so all functions move to Fn+key
  (lib exposes `keysState().fn`; expose it through `cardputer_hw`). **Verify
  early on HW** that Fn+letter reports the letter (some Fn combos remap).
- **Modes:** PLAY (keys=notes, IMU=expression) · EDIT (tweak patch params,
  NVS-persisted) · SEQ / SONG (later). A mode key cycles; the screen shows mode.

## I/O & storage

- **MIDI in + out**, USB + BLE, with **clock sync** (master/slave).
- **SD:** patches, samples, songs, WAV export.
- **NVS (`Preferences`):** global settings + last state (survives reboot).
- **RTC (RX8130):** clock display + SD-file timestamps. Never musical timing.

## Two clocks — do not confuse

- **Musical clock** = the audio sample counter (samples ÷ SR → ticks/PPQN).
  Sample-accurate; the *only* tempo source for arp / looper / delay-sync.
- **RTC** = wall-clock time only (display, timestamps). Never drives tempo.

## Testing strategy

All DSP/logic lives in pure headers, host-tested with g++ (`*_test.cpp`), as
today. Hardware behavior (audio, mic, MIDI enumeration, timing) is verified
on-device per [../hardware-verification.md](../hardware-verification.md) — each
phase has a flash-gate before the next begins.

## Build phases (each shippable, flash-gated, v0.1.0 baseline preserved)

1. **Engine → float32 + Voice[] / Part poly** (1 Part) → *polyphonic current synth*
2. **Resonant SVF filter + env2 + 2 LFOs + mod-matrix** → *real sound design*
3. **2-octave + Fn-layer + EDIT mode + preset save/load (SD/NVS)** → *editable, savable*
4. **Osc expansion: wavetable + 2–4 op FM + PWM + sync/ring-mod + sub/noise** → *timbral range*
5. **Sampler (mic + SD) as a voice source** → *sampling*
6. **Master FX: delay / chorus / drive / bitcrush** (light reverb if RAM allows) → *FX*
7. **Sample-clock + arp + looper** → *jam box* (performance complete)
8. **Multi-Part + drum/sample Parts + step sequencer** → *groovebox*
9. **Song mode (pattern chaining)** → *full groovebox*
10. **MIDI in + clock sync + RTC + WAV export** → *I/O complete*

Performance synth is genuinely usable by ~phase 3–4; jam box at 7; groovebox 8–9.

## Open decisions / risks

- **Fn-combo reporting** (phase 3) — must confirm on hardware before committing
  the keymap; fallback = a dedicated mode/shift key.
- **Voice count under load** — 6 with per-voice SVF + sampler is the target
  ceiling; re-measure on device, drop to 4 if cycles run short.
- **Reverb** — RAM/CPU may not allow a good one; treat as optional in phase 6.
- **Per-Part vs master FX** — master-only first (simpler); per-Part sends only if
  the groovebox phases need them.
- **CPU/RAM budgeting** — measure per phase; this design assumes float32 + block
  rendering keep the per-sample cost within budget at 16 kHz.
