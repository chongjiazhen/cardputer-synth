# Phase 2 Review — Fix Plan

Action plan for a coding agent to resolve findings from the Phase 2 (SVF +
mod-matrix + LFO) review. Each task is independent unless noted. Verify with the
host tests after each task: `g++ -std=c++17 src/synth/<name>_test.cpp -o t && ./t`.

Context: all host suites (`oscillator`, `filter`, `lfo`, `modmatrix`, `voice`)
build and pass. Do not regress them. Pure-logic headers must stay free of
Arduino/M5 includes (host-testable invariant).

---

## Task 0 — PolyBLEP anti-aliased Saw/Square  ✅ DONE

**Files:** `src/synth/oscillator.h`, `src/synth/voice.h`, `oscillator_test.cpp`

**Was:** naive Saw (`2t-1`) and Square (hard step) had unbounded harmonics →
hard aliasing at 32 kHz → the harsh, buzzy tone (confirmed by ear A/B vs GLIDE,
which uses band-limited wavetables). This was the dominant harshness source —
NOT the filter. Sine/Tri were fine (Tri rolls off 1/n²).

**Done:** added `polyBlep(t, dt)` (two-sample Välimäki/Huovilainen kernel) and a
3rd `dt` arg to `osc()` (defaulted 0 = naive, backward-compatible). Saw subtracts
one BLEP at the wrap; Square corrects both edges. `voice.h` passes
`dt = inc/2π` (inc = the modulated per-sample phase advance). Sine/Tri unchanged.

**Edge bug fixed during impl:** Square's falling-edge phase was `t+0.5`, which
rounds to exactly `1.0f` at `t≈0.5` (phase=π), wraps to `0.0`, and misapplies the
rising-edge residual → output spiked to 2.0. Changed to `t-0.5` (past-the-edge
distance) which rounds consistently. Caught by the bounds assertion.

**Verified:** `oscillator_test.cpp` adds (a) dt=0 byte-identical to naive
(back-compat), (b) output bounded <1.25, (c) a mini-DFT that measures
inharmonic/alias energy and asserts PolyBLEP cuts it >50% vs naive for both saw
and square. All suites green.

**Follow-up (optional):** Tri stays naive; if it ever sounds harsh at high notes,
band-limit it via a leaky-integrated BLAMP of the square (adds one state var).
Not needed now.

---

## Task 1 — Remove the dead `env` reference alias  (priority: high)

**File:** `src/synth/voice.h`

**Problem:** `Adsr& env = env1;` (line ~33) is a reference data member that is
never read anywhere (`grep -rn "\.env\b" src/` returns no hits — `main.cpp`
uses `env1`/`env2` directly). It is also a latent trap:
- Deletes Voice's implicit copy-assignment operator (`Voice a; a = b;` fails to
  compile).
- Default copy-constructor makes the copy's `env` alias the *source* object's
  `env1` (dangling/wrong-object aliasing).

Not triggered today (voices live in a default-constructed array, passed by
pointer/reference, never value-copied), but zero benefit for real risk.

**Change:** delete the two lines:
```cpp
  // Legacy compatibility: env getter maps to env1 for existing code.
  Adsr&      env        = env1;
```

**Verify:** `voice_test` still passes. Optionally confirm `Voice a,b; a=b;` now
compiles (it should, once the reference member is gone).

---

## Task 2 — Make the default filter envelope audible  (priority: high)

**Files:** `src/synth/main.cpp`

**Problem:** default routing is `Env2 → FilterCut` depth 1.0 (`main.cpp` ~line
161), which only *adds* cutoff (`cutoffMod` ranges 0..+4000 Hz, `voice.h` ~line
83). But `g_cutoff` starts at `FC_MAX` (open, `main.cpp` ~line 75) and cutoff is
clamped to `sampleRate * 0.49`. So the filter envelope produces no audible
motion until the user manually lowers cutoff with `,`. The headline "filter
opens on attack" behavior is silent out of the box.

**Change:** lower the default so the envelope has room to sweep. Set the initial
cutoff to a partially-closed value, e.g.:
```cpp
static double g_cutoff = 2000.0;   // start partly closed so Env2->FilterCut is audible
```
Keep `FC_MIN`/`FC_MAX` unchanged. Confirm the `redraw()` "open" special-case
(`g_cutoff >= FC_MAX`) still reads correctly — with the new default it will show
`2000Hz` instead of `open`, which is correct.

**Note / alternative (pick one, do not do both):** if a starting-open feel is
preferred for playing, instead change the routing semantics so `FilterCut` mod
is applied relative to a lower envelope floor rather than added to an
already-open base. The simplest, lowest-risk fix is the `g_cutoff = 2000.0`
default above — prefer that unless there is a reason not to.

**Verify:** manual/on-device (no host test covers the main.cpp default). Do not
change `voice_test` — it sets `baseCutoff` explicitly and is unaffected.

---

## Task 3 — Skip LFO stepping when depth is zero, or wire a default  (priority: low)

**File:** `src/synth/voice.h` (and optionally `main.cpp`)

**Problem:** `lfo1.step()` / `lfo2.step()` run every sample in both
`voiceSample` and `voiceSampleBuf`, but default LFO depth is 0 and there is no
default LFO routing, so they always contribute 0. Two `sin` calls + branch per
voice per sample wasted.

**Change (choose one):**
- **A (cheap correctness, keeps flexibility):** early-out in `Lfo::step()` or at
  the call site when `depth == 0.0` — return 0 without evaluating the waveform
  and without advancing phase. Caution: not advancing phase changes phase
  continuity if depth is later raised; acceptable for an LFO. If phase
  continuity matters, advance phase but skip the `switch`/`sin`.
- **B (make the feature real):** add a light default routing in `noteOn`
  (`main.cpp`), e.g. `lfo1` sine at ~5.5 Hz, `depth ≈ 0.02`, routed
  `Lfo1 → Pitch` for subtle vibrato. Only if a default LFO effect is wanted.

Prefer **A** unless product intent is to ship an audible default LFO.

**Verify:** `lfo_test` and `voice_test` still pass. If choosing A with the
skip-phase-advance variant, check `lfo_test` does not assert phase advances at
depth 0 (it currently drives non-zero depth, so it should be fine).

---

## Task 4 — Clamp KeyTrack normalization  (priority: low)

**File:** `src/synth/voice.h`

**Problem:** `src[(int)ModSrc::KeyTrack] = v.midi >= 21 ? (v.midi-21)/87.0f : 0`
(both `voiceSample` and `voiceSampleBuf`). For `midi > 108` this exceeds 1.0,
unclamped. No default route uses KeyTrack today, so no live bug, but clamp for
safety before a patch enables it.

**Change:** clamp the result to `[0, 1]` in both call sites. Consider factoring
the KeyTrack computation into a small helper to avoid the duplicated expression
drifting between the two render functions:
```cpp
inline float keyTrackNorm(int midi) {
  if (midi < 21) return 0.0f;
  float k = (float)(midi - 21) / 87.0f;
  return k > 1.0f ? 1.0f : k;
}
```
Call it from both `voiceSample` and `voiceSampleBuf`.

**Verify:** `voice_test` passes.

---

## Task 5 — Drop the redundant `Voice()` constructor body  (priority: nit)

**File:** `src/synth/voice.h`

**Problem:** the `Voice()` constructor sets `lfo1.shape = LfoShape::Sine;` /
`lfo2.shape = LfoShape::Sine;`, but `Lfo::shape` already default-initializes to
`LfoShape::Sine` in `lfo.h`. The constructor body is redundant.

**Change:** remove the constructor body's assignments. If nothing else remains
in the constructor, remove the `Voice()` constructor entirely (default is fine).
Note: if Task 3B added LFO defaults in `main.cpp::noteOn` that is the right place
for them — do not reintroduce them here.

**Verify:** `voice_test` passes.

---

## Task 6 — Hoist filter coeff calc to block rate + smooth the cutoff target  (priority: high)

**Files:** `src/synth/voice.h`, `src/synth/main.cpp`

**Problem:** `SVFilter::calcCoeffs()` (two `std::cos` + one `std::sin` + a divide,
in `double`) runs **every sample, per voice** (`voice.h` ~line 96 and ~line 152).
At 6 voices × 32 kHz that is ~192k trig-pair evaluations/sec purely for
coefficients. This is the perf concern flagged in `phase2-svf-modmatrix.md` §5,
now promoted to an actual task. Standard fix in any block-processed synth:
compute filter coefficients once per audio block, and de-zipper the cutoff with
a one-pole smoother on the *target* so block-rate updates don't step audibly.

**Pattern (textbook one-pole leaky integrator + block-rate coeff update):**
```cpp
// once per block, per voice:
cutoffSm += (cutoffTarget - cutoffSm) * SMOOTH;  // SMOOTH ~0.1–0.3; de-zippers
filter.calcCoeffs(mode, cutoffSm, res, sr);      // one trig eval per block
// inner sample loop calls only filter.process(x)
```
`SMOOTH` is a per-block coefficient we pick (start ~0.2, tune by ear). This is a
one-pole low-pass on the control signal — not specific to any implementation.

**Change (two coupled wins):**
1. **Move coeff calc out of the per-sample path.** The cleanest fit for our
   per-voice architecture: compute cutoff/resonance and call `calcCoeffs()` once
   per CHUNK (block) per voice, in `renderChunk()` (`main.cpp`), *not* inside
   `voiceSample`/`voiceSampleBuf`. The inner sample loop then only calls
   `filter.process()`. Keeps per-voice filters (our advantage) while dropping
   coeff cost by the block size (CHUNK=256 → ~256× fewer `calcCoeffs` calls).
2. **Smooth the cutoff target** with a one-pole per voice
   (`v.cutoffSm += (cutoffTarget - v.cutoffSm) * 0.2f`) so the block-rate coeff
   update does not zipper on fast cutoff sweeps (`,`/`.` keys, Env2→FilterCut).
   Add a `float cutoffSm` state field to `Voice`; init it to `baseCutoff` in
   `noteOn`.

**Caution — mod rate tradeoff:** our mod matrix currently runs per-sample, so
Env2/LFO→FilterCut modulates at audio rate. If coeffs move to block rate, the
filter only *sees* cutoff at block rate (still smoothed, so inaudible for
envelopes/LFOs at these speeds — GLIDE does exactly this). Keep the mod matrix
itself per-sample (it still drives pitch/amp at audio rate); only the
cutoff→coeff step decimates to block rate. Document this in the code comment.

**Verify:** `voice_test` filter-modulation test (test 5, Env2→FilterCut) must
still show cutoff opening on attack — it will, at block granularity. If the test
asserts a per-sample cutoff value it may need loosening to a block-boundary
tolerance; adjust the test, not the intent. Re-run all four suites.

---

## Task 7 — Swap RBJ biquad → TPT/Zavalishin SVF  (priority: medium; do WITH Task 6)

**File:** `src/synth/filter.h` (+ `filter_test.cpp`)

**Problem / opportunity:** our SVF is an RBJ cookbook biquad (DF2T, 5 mults +
per-mode coeff formulas, `double`). The TPT (topology-preserving transform) /
Zavalishin state-variable form is cheaper in the inner loop (3 mults, **no
per-sample transcendentals**), stable-by-construction near Nyquist, and yields
LP/HP/BP/**Notch** from one pass (we have 3 modes; Notch is free in this form).

**Algorithm source — public reference, implement from these, not from any repo:**
- Andy Simper (Cytomic), *"Solving the continuous SVF equations using trapezoidal
  integration and equivalent currents"* (cytomic.com/technical-papers) — the
  canonical TPT SVF and where the `g / k / v1 v2 v3 / ic1 ic2` naming comes from.
- Vadim Zavalishin, *The Art of VA Filter Design* — the TPT derivation.

**Canonical form (from the Simper paper; standard, provider-agnostic):**
```
g  = tan(pi * fc / sr)          // prewarped integrator gain
k  = 1 / Q                      // damping; Q high → k small → resonant
a1 = 1 / (1 + g*(g + k))
a2 = g * a1
a3 = g * a2

// per sample:
v3  = x - ic2
v1  = a1*ic1 + a2*v3
v2  = ic2 + a2*ic1 + a3*v3
ic1 = 2*v1 - ic1                // trapezoidal state update
ic2 = 2*v2 - ic2
LP = v2;  BP = v1;  HP = x - k*v1 - v2;  Notch = x - k*v1
```

**Change:** replace `SVFilter`'s biquad internals with the TPT form above,
keeping our `namespace synth`, `FilterMode` enum (add `Notch`), and method names.
Use `k = 1/Q` (canonical) so our existing `Q` convention (0.1..20) carries over
unchanged — no need to rewrite `voice.h`'s res clamp, `main.cpp`'s `g_resonance`
(`6`/`7` keys), or the `Q%.1f` display. Pick **our own** cutoff clamps
(reuse the existing `FC_MIN` / `sampleRate*0.49` bounds already in the code) and
our own damping floor — do not import external magic constants. Keep the state in
`double` to match the rest of our DSP.

**Provenance note:** this is standard published DSP. Write it from the Simper/
Zavalishin references in our own style; cite them in the `filter.h` header
comment. Do not paste any third-party repo's source file (some are unlicensed).

**Why do it with Task 6, not instead:** block-rate coeffs (Task 6) already fixes
the CPU cost, so TPT's cheap-coeff win is partly moot — but TPT's stability near
Nyquist, the free Notch mode, and the cleaner one-param resonance map stand on
their own. If time-boxed, Task 6 alone captures the perf win; Task 7 is the
quality/mode upgrade.

**Verify:** rewrite `filter_test.cpp` DC-gain + attenuation + resonance tests
against the TPT outputs. TPT LP also has unity DC gain and HP/BP → 0 at DC, so
the existing assertions should hold with retuned tolerances. Add a Notch test
(deep attenuation at cutoff, pass-through away from it). All four suites green.

**Do NOT copy GLIDE's lane-bus filtering.** GLIDE filters a shared per-lane mix
(2 filters total), not per voice — that is its mono/paraphonic constraint, not an
upgrade. Our per-voice filter (per-note envelope + key-track) is our deliberate
polyphonic advantage. Keep it.

---

## Minor borrows (optional, low priority)

- **Multiplicative Amp mod:** GLIDE does `modAmpMul *= (1+v)` for VCA-style
  tremolo; ours is additive-only (`voice.h` ~line 99). Multiplicative is more
  musical for LFO→Amp. Consider when wiring an LFO→Amp default.
- **Sparse mod-slot skip:** GLIDE's matrix is a 6-slot list with
  `if (depth==0) continue`; ours is a dense 6×8 swept every sample (48 mul-adds).
  Only worth it if the matrix stays per-sample AND CPU is tight — moot if Task 6
  moves cutoff to block rate. Do not restructure the matrix speculatively.

---

## Out of scope (documented, do not implement here)

- IMU-vibrato consolidation into the mod matrix: tracked in
  `phase2-svf-modmatrix.md` §5. Separate phase.

## Suggested commit sequence

One commit per task, conventional prefix:
1. `refactor(synth): drop dead env reference alias from Voice` (Task 1)
2. `fix(synth): start filter partly closed so Env2 sweep is audible` (Task 2)
3. `perf(synth): skip LFO eval when depth is zero` (Task 3)
4. `fix(synth): clamp KeyTrack normalization to 0..1` (Task 4)
5. `chore(synth): remove redundant Voice ctor` (Task 5)
6. `perf(synth): compute filter coeffs at block rate with smoothed cutoff` (Task 6)
7. `refactor(synth): swap RBJ biquad SVF for TPT/Zavalishin form + Notch` (Task 7)

Tasks 1, 4, 5 are pure-safe (tests cover them). Tasks 2 and 3 change audible
behavior — verify on device before considering done. Tasks 6 and 7 are the
GLIDE-derived DSP borrows: 6 fixes the per-sample coeff-cost weakness (do first,
highest value), 7 is the filter-quality/mode upgrade (do with 6). Both keep our
per-voice filter architecture — do not adopt GLIDE's shared-lane filtering.

Reference: GLIDE = CHARL3X/GLIDE-Synth-Cardputer-ADV (`src/dsp/svf.h`,
`src/dsp/synth.cpp`, `src/dsp/params.h`). TPT SVF and block-rate coeff pattern
documented in Tasks 6–7 above.
