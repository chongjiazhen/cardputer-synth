# Full-build design — poly + sampler + arp + looper + delay + RTC

Design for the next wave of features. Built in dependency order, **each layer
flash-tested before the next** (audio/mic/timing are opaque to host tests). The
pure-math pieces stay in host-testable headers; hardware glue in `main.cpp`.

## Two clocks — do not confuse

- **Musical clock** = the audio sample counter (samples ÷ SR). Sample-accurate,
  jitter-free. The *only* tempo source for arp / looper / delay-sync.
- **RTC** (RX8130, battery-backed wall time) = date/time only. For a clock
  display + timestamping SD files. **Never** drives musical timing.

## Dependency graph

```
FOUNDATIONS            LEAVES
───────────            ──────
Voice[] / polyphony ─→ sampler (per-voice sample source)
                    ├─→ delay  (global, on summed mix)
sample-clock        ─→ arp     (clock + held keys → voices)
                    └─→ looper (clock + recorded events → voices)
2-octave + Fn-layer ─→ control surface for everything above
RTC                 ─→ clock display / SD timestamps (independent leaf)
```

## Foundation 1 — polyphony / Voice model

Replace the global voice singletons with a small `Voice` array.

```
struct Voice {
  bool     active;
  char     key;        // which key gates it (0 = free)
  double   phase, phaseInc;
  double   samplePos, sampleStep;   // sampler source
  double   velScale;   // per-note, latched from tilt at its note-on
  Adsr     env;
};
Voice voices[N];        // N = 4 (simple osc+env voices are cheap at 16kHz/240MHz)
```

- **Allocation:** note-on → pick a free voice, else steal the oldest/quietest.
  note-off → that voice's `env.gateOff()` (release tail), freed when env idle.
- **Per-voice:** phase, env, velocity, sample position.
- **Global (one instance, applied to all/the mix):** pitch bend ratio, vibrato
  LFO, **filter** (move to the summed mix — one filter, not N), master volume.
- `renderChunk`: sum active voices → apply global filter → delay → output.
  Watch headroom: sum of N voices can exceed ±1; scale by `1/N` or soft-limit.
- Replaces mono "last-note priority"; velocity still latched per note from the
  fwd/back tilt at that key's press.

Revises the earlier "poly ≤ 2 permanent" note — that was the Vital-cribbing
context (heavy per-voice DSP). Simple voices → 4–6 fit easily.

## Foundation 2 — sample-clock

A free-running sample counter (uint32) incremented per output sample. Tempo in
BPM → samples-per-step = `SR * 60 / (BPM * stepsPerBeat)`. Arp/looper/delay read
this. Lives next to the audio loop.

## Foundation 3 — 2-octave keyboard + Fn-layer

- Notes on both rows: lower octave `z x c v b n m` + `s d g h j` (existing);
  upper octave `q w e r t y u` (white) + `2 3 5 6 7` (sharps over the gaps).
- Number row now carries sharps → **all controls move to Fn+key** (lib exposes
  `keysState().fn`). Expose `fn` through `cardputer_hw`.
- **Risk to verify early on HW:** Fn+letter must report the letter char (some
  Fn combos remap to arrows/specials). Check before committing the layout.
- Control map (Fn + …): waveform, filter cutoff, octave shift, volume, record,
  sampler-mode, arp on/off + mode, looper rec/play/clear, tempo.

## Leaves

- **Sampler** — `r`-equivalent records ~1 s from mic into a RAM buffer (mic +
  speaker share the ES8311 codec → `M5.Speaker.end()` before `M5.Mic.begin()`,
  restore after). A 5th source plays the buffer per-voice via `sampler.h`
  (`sampleRead` + `sampleStep`, already written + host-tested). Loops while
  gated. Mic capture + codec switch unverifiable by host — flash-test.
- **Delay** — circular buffer on the summed mix; feedback + mix params.
  16 kHz×16-bit ≈ 32 KB/s → ~seconds fit in SRAM. Cheap, pure-ish (host-test the
  buffer read/write/feedback math).
- **Arp** — held keys + sample-clock → retrigger voices in a pattern
  (up/down/up-down/random) at the tempo. Wider 2-octave range = better patterns.
- **Looper** — record note *events* (note, vel, time) to RAM, loop on the
  sample-clock, play over live input. Events are tiny. Shares arp's clock; a
  metronome falls out of the same clock.
- **RTC** — read RX8130 wall time → optional clock display + timestamp SD sample
  /WAV files. Independent, low priority.

## Build order (flash-gate each)

1. Poly / Voice refactor → play a chord.
2. 2-octave + Fn-layer → verify keys + Fn combos.
3. Sampler (per-voice) → test mic.
4. Delay → hear echo.
5. Clock + Arp → hear arpeggio in time.
6. Looper → layer a loop. (RTC slots in whenever; independent.)

1–3 = a solid session; 4–6 if energy holds. Each independently shippable.

## Verification

Every layer ships behind the on-device gate — see
[../hardware-verification.md](../hardware-verification.md).
