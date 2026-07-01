# Phase 1 — Polyphonic Engine Implementation Plan

*Design record for phase 1 (completed). Kept for history; the live code is the
source of truth.*

**Goal:** Convert the monophonic synth to a 6-voice polyphonic engine (float32 audio path) that plays chords, while keeping the v0.1.0 sound, controls, and on-device tunings unchanged.

**Architecture:** Replace the single-voice globals with a fixed `Voice[6]` pool. Each note-on allocates a voice (free, else steal oldest); note-off gates that voice's envelope. Per-voice: phase, envelope, latched velocity. Global (applied to all voices / the master bus): pitch-bend ratio, vibrato LFO, the low-pass filter (moved from per-voice to the summed master bus per the spec), and a master soft-limiter so summed voices don't clip.

**Tech Stack:** C++17, PlatformIO (`espressif32`/arduino), M5Cardputer. Pure-logic headers host-tested with `g++ -std=c++17`; hardware glue in `main.cpp`/`cardputer_hw`.

## Global Constraints

- **float32 in the audio path.** LX7 FPU is single-precision; `double` is emulated. Note math (`midiToHz`) may stay double (computed once per note, not per sample).
- **Preserve v0.1.0 invariants:** per-waveform `shapeGain` table (tri 1.0 / sine 0.94 / saw 0.38 / square 0.24); single-voice loudness ≈ v0.1.0 (master soft-limit targets the same safe analog peak `AMP = 14000`).
- **6 voices** shared. Re-measure CPU on device; drop to 4 if cycles run short.
- **No behavior regression:** waveforms 1-4, `,`/`.` filter, octave `[`/`]`/`;`/`'`, volume `-`/`=`, IMU velocity/vibrato/pitch-bend all still work; now polyphonic.
- **Pure headers stay Arduino/M5-free** (host-testable). No new libraries.
- **Conventional commits**, body ends with the prose line `Assisted by AI.` (no `Assisted-by:` trailer, no `Co-Authored-By`).

## File Structure

- `src/synth/oscillator.h` — **modify**: `osc()` and `shapeGain()` to `float`; add `PI_F` / `TWO_PI_F`.
- `src/synth/oscillator_test.cpp` — **modify**: float expectations.
- `src/synth/voice.h` — **create**: `Voice` struct + `voiceSample()` (per-sample render).
- `src/synth/voice_test.cpp` — **create**.
- `src/synth/voicealloc.h` — **create**: `allocVoice()` + `findVoiceByKey()`.
- `src/synth/voicealloc_test.cpp` — **create**.
- `src/synth/mixer.h` — **create**: `softClip()` + `mixToInt16()` (master limiter).
- `src/synth/mixer_test.cpp` — **create**.
- `src/synth/main.cpp` — **modify**: replace single-voice globals + `noteOn`/`renderChunk`/note-on/release/`setup` with the `Voice[6]` engine.

---

### Task 1: Float oscillator

**Files:**
- Modify: `src/synth/oscillator.h`
- Test: `src/synth/oscillator_test.cpp`

**Interfaces:**
- Produces: `float osc(WaveShape, float phase)`, `float shapeGain(WaveShape)`, `constexpr float PI_F`, `constexpr float TWO_PI_F`.

- [ ] **Step 1: Update the test to float**

Replace the `--- shapeGain ... ---` block and add constant checks. In `src/synth/oscillator_test.cpp`, ensure these assertions exist (the gain block already does; add the constants check right after it):

```cpp
  // --- float constants present ---
  check(near(TWO_PI_F, 6.2831853f, 1e-4), "TWO_PI_F");
  check(near(PI_F, 3.1415927f, 1e-4), "PI_F");
```

Add `using synth::PI_F; using synth::TWO_PI_F;` near the other `using` lines.

- [ ] **Step 2: Run test to verify it fails**

Run: `g++ -std=c++17 src/synth/oscillator_test.cpp -o /tmp/osc && /tmp/osc`
Expected: FAIL to compile — `PI_F`/`TWO_PI_F` not declared.

- [ ] **Step 3: Convert oscillator.h to float**

Replace the body of `src/synth/oscillator.h` from the `#include <cmath>` line down to the end of `osc()` with:

```cpp
#include <cmath>

namespace synth {

enum class WaveShape { Sine, Saw, Square, Tri };

constexpr float PI_F     = 3.14159265358979f;
constexpr float TWO_PI_F = 6.28318530717959f;

inline const char* shapeName(WaveShape s) {
  switch (s) {
    case WaveShape::Sine:   return "SINE";
    case WaveShape::Saw:    return "SAW";
    case WaveShape::Square: return "SQR";
    case WaveShape::Tri:    return "TRI";
    default:                return "?";
  }
}

// Per-waveform loudness gains (perceptual, ear-tuned on device — v0.1.0).
inline float shapeGain(WaveShape shape) {
  switch (shape) {
    case WaveShape::Tri:    return 1.00f;
    case WaveShape::Sine:   return 0.94f;
    case WaveShape::Saw:    return 0.38f;
    case WaveShape::Square: return 0.24f;
    default:                return 1.0f;
  }
}

inline float osc(WaveShape shape, float phase) {
  switch (shape) {
    case WaveShape::Sine:   return std::sin(phase);
    case WaveShape::Saw:    return 2.0f * phase / TWO_PI_F - 1.0f;
    case WaveShape::Square: return phase < PI_F ? 1.0f : -1.0f;
    case WaveShape::Tri:
      if (phase < PI_F) return (2.0f * phase / PI_F) - 1.0f;
      else              return 3.0f - (2.0f * phase / PI_F);
    default:
      return 0.0f;
  }
}

}  // namespace synth
```

(Delete the old `#ifndef M_PI ... #endif` block and the old double versions.)

- [ ] **Step 4: Run test to verify it passes**

Run: `g++ -std=c++17 src/synth/oscillator_test.cpp -o /tmp/osc && /tmp/osc`
Expected: PASS — prints `ok`.

- [ ] **Step 5: Commit**

```bash
git add src/synth/oscillator.h src/synth/oscillator_test.cpp
git commit -m "refactor(synth): float oscillator + PI_F/TWO_PI_F

Assisted by AI."
```

---

### Task 2: Voice struct + per-sample render

**Files:**
- Create: `src/synth/voice.h`
- Test: `src/synth/voice_test.cpp`

**Interfaces:**
- Consumes: `osc`, `shapeGain`, `TWO_PI_F` (Task 1); `Adsr` (`adsr.h`: `configMs(sr,a,d,s,r)`, `gateOn()`, `gateOff()`, `double step()`, `bool active()`).
- Produces: `struct Voice { bool active; char key; int midi; int age; float phase, phaseInc, vel; Adsr env; }`; `float voiceSample(Voice&, WaveShape, float bendRatio, float vibRatio)`.

- [ ] **Step 1: Write the failing test**

Create `src/synth/voice_test.cpp`:

```cpp
// Host test for voice.h:  g++ -std=c++17 voice_test.cpp -o t && ./t
#include "voice.h"
#include <cassert>
#include <cstdio>
#include <cmath>

static void check(bool c, const char* m) { if (!c) { fprintf(stderr, "FAIL: %s\n", m); assert(false); } }

int main() {
  using namespace synth;

  // inactive voice outputs silence and does not advance
  {
    Voice v;
    check(voiceSample(v, WaveShape::Sine, 1.0f, 1.0f) == 0.0f, "inactive silent");
    check(v.phase == 0.0f, "inactive no advance");
  }

  // active voice: phase advances by phaseInc * bend * vib
  {
    Voice v;
    v.active = true; v.vel = 1.0f; v.phase = 0.0f; v.phaseInc = 0.1f;
    v.env.configMs(16000, 1.0, 1.0, 1.0, 1000.0);  // long, ~full level
    v.env.gateOn();
    voiceSample(v, WaveShape::Saw, 1.0f, 1.0f);
    check(std::fabs(v.phase - 0.1f) < 1e-4f, "phase += inc");
    voiceSample(v, WaveShape::Saw, 2.0f, 1.0f);   // bend x2
    check(std::fabs(v.phase - 0.3f) < 1e-4f, "phase += inc*bend");
  }

  // gateOff then enough steps → env idle → voice goes inactive
  {
    Voice v;
    v.active = true; v.vel = 1.0f; v.phaseInc = 0.1f;
    v.env.configMs(16000, 1.0, 1.0, 1.0, 1.0);  // 1ms release
    v.env.gateOn();
    voiceSample(v, WaveShape::Sine, 1.0f, 1.0f);
    v.env.gateOff();
    for (int i = 0; i < 2000; i++) voiceSample(v, WaveShape::Sine, 1.0f, 1.0f);
    check(!v.active, "voice frees after release");
  }

  puts("ok");
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `g++ -std=c++17 src/synth/voice_test.cpp -o /tmp/voice && /tmp/voice`
Expected: FAIL to compile — `voice.h` not found.

- [ ] **Step 3: Create voice.h**

```cpp
// One synthesizer voice. Pure math, no Arduino/M5 includes — host-testable.
#pragma once
#include "oscillator.h"
#include "adsr.h"

namespace synth {

struct Voice {
  bool  active   = false;  // env sounding (gate held OR release tail)
  char  key      = 0;      // gating key char, 0 = not gated
  int   midi     = 0;      // MIDI note number (for note-off messages)
  int   age      = 0;      // allocation order; lower = older (steal target)
  float phase    = 0.0f;   // oscillator phase, 0..TWO_PI_F
  float phaseInc = 0.0f;   // base radians/sample for the note
  float vel      = 1.0f;   // 0..1 latched velocity
  Adsr  env;
};

// Render one sample for a voice and advance its state. Returns the voice's
// float contribution (osc x shapeGain x env x velocity). Frequency is modulated
// by the global bendRatio (pitch bend) and vibRatio (vibrato LFO). When the
// envelope goes idle the voice marks itself inactive (freeing it for reuse).
inline float voiceSample(Voice& v, WaveShape wave, float bendRatio, float vibRatio) {
  if (!v.active) return 0.0f;
  float e = (float)v.env.step();
  float s = osc(wave, v.phase) * shapeGain(wave) * e * v.vel;
  v.phase += v.phaseInc * bendRatio * vibRatio;
  if (v.phase >= TWO_PI_F) v.phase -= TWO_PI_F;
  if (!v.env.active()) v.active = false;
  return s;
}

}  // namespace synth
```

- [ ] **Step 4: Run test to verify it passes**

Run: `g++ -std=c++17 src/synth/voice_test.cpp -o /tmp/voice && /tmp/voice`
Expected: PASS — prints `ok`.

- [ ] **Step 5: Commit**

```bash
git add src/synth/voice.h src/synth/voice_test.cpp
git commit -m "feat(synth): Voice struct + per-sample render

Assisted by AI."
```

---

### Task 3: Voice allocation

**Files:**
- Create: `src/synth/voicealloc.h`
- Test: `src/synth/voicealloc_test.cpp`

**Interfaces:**
- Consumes: `Voice` (Task 2).
- Produces: `int allocVoice(Voice* v, int n, int nowAge)`, `int findVoiceByKey(Voice* v, int n, char key)`.

- [ ] **Step 1: Write the failing test**

Create `src/synth/voicealloc_test.cpp`:

```cpp
// Host test for voicealloc.h:  g++ -std=c++17 voicealloc_test.cpp -o t && ./t
#include "voicealloc.h"
#include <cassert>
#include <cstdio>

static void check(bool c, const char* m) { if (!c) { fprintf(stderr, "FAIL: %s\n", m); assert(false); } }

int main() {
  using namespace synth;
  Voice v[3];

  // all free → returns index 0
  check(allocVoice(v, 3, 1) == 0, "first free = 0");

  // mark 0 and 1 active → returns the remaining free (2)
  v[0].active = true; v[0].age = 1;
  v[1].active = true; v[1].age = 2;
  check(allocVoice(v, 3, 3) == 2, "next free = 2");

  // all active → steal the oldest (lowest age)
  v[2].active = true; v[2].age = 3;
  check(allocVoice(v, 3, 4) == 0, "steal oldest = 0");
  v[0].age = 9;  // 0 now newest
  check(allocVoice(v, 3, 5) == 1, "steal new oldest = 1");

  // findVoiceByKey
  v[1].key = 'a';
  check(findVoiceByKey(v, 3, 'a') == 1, "find key a");
  check(findVoiceByKey(v, 3, 'z') == -1, "missing key = -1");
  v[1].active = false;
  check(findVoiceByKey(v, 3, 'a') == -1, "inactive not found");

  puts("ok");
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `g++ -std=c++17 src/synth/voicealloc_test.cpp -o /tmp/va && /tmp/va`
Expected: FAIL to compile — `voicealloc.h` not found.

- [ ] **Step 3: Create voicealloc.h**

```cpp
// Voice allocation policy. Pure, no Arduino/M5 includes — host-testable.
#pragma once
#include "voice.h"

namespace synth {

// Pick a voice for a new note: prefer an inactive voice; if none, steal the
// oldest active one (lowest age). Caller passes a monotonically increasing
// nowAge and stores it on the chosen voice. Returns the voice index.
inline int allocVoice(Voice* v, int n, int nowAge) {
  (void)nowAge;
  int oldest = 0;
  for (int i = 0; i < n; i++) {
    if (!v[i].active) return i;
    if (v[i].age < v[oldest].age) oldest = i;
  }
  return oldest;
}

// Find the active voice gated by `key` (for note-off). Returns index or -1.
inline int findVoiceByKey(Voice* v, int n, char key) {
  for (int i = 0; i < n; i++)
    if (v[i].active && v[i].key == key) return i;
  return -1;
}

}  // namespace synth
```

- [ ] **Step 4: Run test to verify it passes**

Run: `g++ -std=c++17 src/synth/voicealloc_test.cpp -o /tmp/va && /tmp/va`
Expected: PASS — prints `ok`.

- [ ] **Step 5: Commit**

```bash
git add src/synth/voicealloc.h src/synth/voicealloc_test.cpp
git commit -m "feat(synth): voice allocation (free-first, steal-oldest)

Assisted by AI."
```

---

### Task 4: Master mix + soft limiter

**Files:**
- Create: `src/synth/mixer.h`
- Test: `src/synth/mixer_test.cpp`

**Interfaces:**
- Produces: `float softClip(float x)`, `int16_t mixToInt16(float busSum, float masterGain, int amp)`.

**Design note:** cubic soft clip `f(x)=x − x³/3` is unity-ish for |x|≤1 (saturates to ±2/3), so we scale its output ×1.5 → a single voice at peak 1 maps to `amp` (matches v0.1.0 level), while summed voices saturate gracefully instead of hard-clipping.

- [ ] **Step 1: Write the failing test**

Create `src/synth/mixer_test.cpp`:

```cpp
// Host test for mixer.h:  g++ -std=c++17 mixer_test.cpp -o t && ./t
#include "mixer.h"
#include <cassert>
#include <cstdio>
#include <cmath>

static void check(bool c, const char* m) { if (!c) { fprintf(stderr, "FAIL: %s\n", m); assert(false); } }
static bool near(float a, float b, float t) { return std::fabs(a - b) <= t; }

int main() {
  using namespace synth;

  // softClip: 0→0, ±1→±2/3, saturates beyond
  check(near(softClip(0.0f), 0.0f, 1e-6f), "softClip 0");
  check(near(softClip(1.0f), 2.0f/3.0f, 1e-6f), "softClip 1");
  check(near(softClip(5.0f), 2.0f/3.0f, 1e-6f), "softClip saturates +");
  check(near(softClip(-5.0f), -2.0f/3.0f, 1e-6f), "softClip saturates -");

  // single voice at peak 1, gain 1 → ~amp (v0.1.0 level preserved)
  check(mixToInt16(1.0f, 1.0f, 14000) == 14000, "1 voice peak = amp");
  // six aligned voices don't exceed amp (soft saturation)
  check(mixToInt16(6.0f, 1.0f, 14000) == 14000, "6 voices clamp to amp");
  // silence → 0
  check(mixToInt16(0.0f, 1.0f, 14000) == 0, "silence = 0");
  // never exceeds int16
  check(mixToInt16(100.0f, 1.0f, 32767) <= 32767, "no int16 overflow");

  puts("ok");
  return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `g++ -std=c++17 src/synth/mixer_test.cpp -o /tmp/mix && /tmp/mix`
Expected: FAIL to compile — `mixer.h` not found.

- [ ] **Step 3: Create mixer.h**

```cpp
// Master mix + soft limiter. Pure, no Arduino/M5 includes — host-testable.
#pragma once
#include <cstdint>

namespace synth {

// Cubic soft clip: f(x) = x - x^3/3 for |x|<=1 (saturates to +/-2/3 beyond).
inline float softClip(float x) {
  if (x >=  1.0f) return  2.0f / 3.0f;
  if (x <= -1.0f) return -2.0f / 3.0f;
  return x - (x * x * x) / 3.0f;
}

// Sum the master bus to int16 at output amplitude `amp`, after applying
// masterGain and the soft limiter. The x1.5 rescale makes a single voice at
// peak 1.0 map to `amp` (preserving v0.1.0 loudness); summed voices saturate.
inline int16_t mixToInt16(float busSum, float masterGain, int amp) {
  float s = softClip(busSum * masterGain) * 1.5f;
  float y = s * (float)amp;
  if (y >  32767.0f) y =  32767.0f;
  if (y < -32768.0f) y = -32768.0f;
  return (int16_t)y;
}

}  // namespace synth
```

- [ ] **Step 4: Run test to verify it passes**

Run: `g++ -std=c++17 src/synth/mixer_test.cpp -o /tmp/mix && /tmp/mix`
Expected: PASS — prints `ok`.

- [ ] **Step 5: Commit**

```bash
git add src/synth/mixer.h src/synth/mixer_test.cpp
git commit -m "feat(synth): master soft-limiter + int16 mix

Assisted by AI."
```

---

### Task 5: Integrate the poly engine into main.cpp

**Files:**
- Modify: `src/synth/main.cpp`

**Interfaces:**
- Consumes: `Voice`/`voiceSample` (Task 2), `allocVoice`/`findVoiceByKey` (Task 3), `mixToInt16` (Task 4), `osc`/`shapeGain`/`TWO_PI_F` (Task 1).

This task has no host test (it's M5 hardware glue); it is verified by building all envs and flashing (Steps 6–7). Apply each edit exactly.

- [ ] **Step 1: Replace the voice-state globals**

Find this block (the IMU/voice/MIDI state, around the `g_velScale`/`g_phaseInc` declarations) and replace the **single-voice voice-state** portion. Replace:

```cpp
// Voice state — monophonic. Phase runs continuously across chunks (no per-chunk
// reset) so sustained tones don't click.
static synth::Adsr g_env;
static double      g_phaseInc = 0.0;      // radians per sample for current note
static double      g_phase    = 0.0;      // running oscillator phase
static char        g_gateKey  = 0;        // note key currently held, 0 = none
```

with:

```cpp
// Voice state — polyphonic. Fixed pool; note-on allocates, note-off gates.
static constexpr int N_VOICES = 6;
static synth::Voice  g_voices[N_VOICES];
static int           g_noteAge = 0;       // monotonic allocation counter
```

Also change `g_velScale` (per-note level) — it is now per voice. Find and DELETE:

```cpp
static double g_velScale    = 1.0;   // per-note amplitude, latched at note-on
```

- [ ] **Step 2: Add the new includes**

After `#include "filter.h"` add:

```cpp
#include "voice.h"
#include "voicealloc.h"
#include "mixer.h"
```

- [ ] **Step 3: Replace `noteOn` and `renderChunk`**

Replace the whole `noteOn(...)` function with one that allocates a voice:

```cpp
// Allocate a voice for a key press at a semitone + velocity (0..127).
static void noteOn(char key, int semitone, uint8_t velocity) {
  int midi = synth::noteToMidi(semitone, g_octave);
  int i    = synth::allocVoice(g_voices, N_VOICES, ++g_noteAge);
  synth::Voice& v = g_voices[i];
  v.active   = true;
  v.key      = key;
  v.midi     = midi;
  v.age      = g_noteAge;
  v.phase    = 0.0f;
  v.phaseInc = synth::TWO_PI_F * (float)synth::midiToHz(midi) / SR;
  v.vel      = (float)velocity / 127.0f;
  v.env.configMs(SR, 8.0, 40.0, 0.7, 120.0);
  v.env.gateOn();
}
```

Replace the whole `renderChunk()` body with the poly mix:

```cpp
// Render one CHUNK: sum all active voices, apply the global vibrato LFO + pitch
// bend (per voice) and the master low-pass + soft limiter (on the mix).
static void renderChunk() {
  for (size_t i = 0; i < CHUNK; i++) {
    float vibSemi  = g_vibratoDepth * synth::VIB_MAX_SEMITONES * sinf((float)g_lfoPhase);
    float vibRatio = 1.0f + vibSemi * 0.0577623f;
    float bus = 0.0f;
    for (int vch = 0; vch < N_VOICES; vch++)
      bus += synth::voiceSample(g_voices[vch], g_wave, (float)g_bendRatio, vibRatio);
    bus = (float)g_filter.process(bus);        // master low-pass (open = passthrough)
    g_buf[i] = synth::mixToInt16(bus, 1.0f, AMP);
    g_lfoPhase += g_lfoInc;
    if (g_lfoPhase >= TWO_PI) g_lfoPhase -= TWO_PI;
  }
  M5.Speaker.playRaw(g_buf, CHUNK, SR, false, 1, 0);   // channel 0, no repeat
}

// True while any voice is still sounding (gate or release tail).
static bool anyVoiceActive() {
  for (int i = 0; i < N_VOICES; i++) if (g_voices[i].active) return true;
  return false;
}
```

- [ ] **Step 4: Update the note-on / note-off handlers in `loop()`**

In the `keysJustPressed()` loop, replace the note-trigger branch:

```cpp
    int s = synth::keyToSemitone(c);
    if (s >= 0) {
      // Latch velocity from the current fwd/back tilt (last IMU sample).
      uint8_t vel = synth::tiltVelocity(g_tiltFwd);
      noteOn(c, s, vel);
      int midiNote = synth::noteToMidi(s, g_octave);
#ifdef SYNTH_USB_MIDI
      usbMidi.sendNoteOn(midiNote, vel, 1);
#endif
#ifdef SYNTH_BLE_MIDI
      MIDI.sendNoteOn(midiNote, vel, 1);
#endif
      char label[8];
      snprintf(label, sizeof(label), "%s%d", synth::semitoneName(s), g_octave);
      redraw(label);
      continue;
    }
```

Replace the whole mono gate-release block:

```cpp
  // --- gate release: the held note key let go -> enter release tail ---
  if (g_gateKey && !heldContains(cardputer::keysHeld(), g_gateKey)) {
    int midiNote = synth::noteToMidi(synth::keyToSemitone(g_gateKey), g_octave);
    g_env.gateOff();
#ifdef SYNTH_USB_MIDI
    usbMidi.sendNoteOff(midiNote, 0, 1);
#endif
#ifdef SYNTH_BLE_MIDI
    MIDI.sendNoteOff(midiNote, 0, 1);
#endif
    g_gateKey = 0;
  }
```

with a per-voice sweep:

```cpp
  // --- gate release: any voice whose key is no longer held enters its tail ---
  {
    auto held = cardputer::keysHeld();
    for (int i = 0; i < N_VOICES; i++) {
      synth::Voice& v = g_voices[i];
      if (v.active && v.key && !heldContains(held, v.key)) {
        v.env.gateOff();
#ifdef SYNTH_USB_MIDI
        usbMidi.sendNoteOff(v.midi, 0, 1);
#endif
#ifdef SYNTH_BLE_MIDI
        MIDI.sendNoteOff(v.midi, 0, 1);
#endif
        v.key = 0;   // no longer gated; env releases, then voice frees itself
      }
    }
  }
```

- [ ] **Step 5: Update the speaker-feed condition**

Find the speaker-feed loop near the end of `loop()`:

```cpp
  while (g_env.active() && M5.Speaker.isPlaying(0) < 2) {
```

Replace `g_env.active()` with `anyVoiceActive()`:

```cpp
  while (anyVoiceActive() && M5.Speaker.isPlaying(0) < 2) {
```

(If there is a second reference to `g_env.active()` anywhere in `loop()`, replace it the same way.)

- [ ] **Step 6: Build all envs + run host tests**

Run:
```bash
cd src/synth && for t in oscillator adsr notes imu_map filter sampler voice voicealloc mixer; do g++ -std=c++17 ${t}_test.cpp -o /tmp/$t && /tmp/$t; done
cd ../.. && pio run -e synth -e synth-usb-midi -e synth-ble-midi 2>&1 | grep -iE "error|Flash:|SUCCESS|FAILED"
```
Expected: every test prints `ok`; all three envs `SUCCESS`. If `g_velScale`, `g_phase`, `g_phaseInc`, `g_env`, or `g_gateKey` are referenced anywhere still, the build fails — remove/redirect those references (display `redraw` uses `g_velScale`: change the `vel:` field to read the most recent voice, or drop the live `vel` readout to a static `--`; simplest: show the last-latched velocity by keeping a `static float g_lastVel` set in `noteOn` and printing that).

- [ ] **Step 7: Flash + verify polyphony on device**

Run: `pio run -e synth -t upload --upload-port COM5`
On the Cardputer, press two or three note keys together → they should sound **simultaneously** (a chord), each sustaining while held, releasing on key-up. Confirm waveforms 1-4, `,`/`.` filter, octave, volume, and IMU velocity/vibrato/pitch-bend still behave. One note should be about as loud as v0.1.0; a full chord should be louder but not harshly clipped.

- [ ] **Step 8: Commit**

```bash
git add src/synth/main.cpp
git commit -m "feat(synth): polyphonic engine (6-voice pool)

Replace the mono single-voice globals with a Voice[6] pool: note-on
allocates (free, else steal oldest), note-off gates the matching
voice. Per-voice phase/env/velocity; global pitch-bend + vibrato LFO;
the low-pass filter and a soft limiter move to the summed master bus
so chords don't clip while one voice stays at the v0.1.0 level.

Assisted by AI."
```

---

## Self-Review

**Spec coverage (phase 1 scope):** float32 path (Task 1 + floats throughout), Voice/Part poly (Tasks 2,3,5 — single implicit Part; multi-Part deferred to phase 8 per spec), shared 6-voice pool with steal (Task 3), per-voice phase/env/velocity (Task 2,5), global bend/vibrato (Task 5), filter moved to master bus (Task 5), headroom via master soft-limiter targeting v0.1.0 peak (Task 4,5), shapeGain table preserved (Task 1). Covered.

**Deferred (NOT phase 1, correct per spec):** SVF resonant filter, env2/LFO2/mod-matrix (phase 2); 2-octave/Fn-layer/EDIT/presets (phase 3); osc expansion (phase 4); sampler (phase 5). The `Part` struct is intentionally not introduced yet — the Voice pool + clean allocation is the multi-Part enabler; a 1-element Part struct now is premature (YAGNI).

**Placeholder scan:** none — every step has full code.

**Type consistency:** `osc(WaveShape,float)`/`shapeGain(WaveShape)→float` (Task 1) used by `voiceSample` (Task 2). `Voice` fields (`active,key,midi,age,phase,phaseInc,vel,env`) defined Task 2, used by `allocVoice`/`findVoiceByKey` (Task 3) and `main.cpp` (Task 5). `mixToInt16(float,float,int)` (Task 4) used in `renderChunk` (Task 5). `noteOn(char,int,uint8_t)` defined + called in Task 5. Consistent.
