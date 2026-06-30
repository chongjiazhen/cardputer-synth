# Phase 7 — Arpeggiator Implementation Plan

> **Scope:** Part of architecture phase 7 ("Sample-clock + arp + looper → jam box").
> The arpeggiator sits in the PERFORMANCE layer, above the polyphonic engine
> (phase 1, already shipped) and alongside the sample-clock (separate task).

**Goal:** Add an arpeggiator that, when enabled, takes the set of currently held
notes and plays them in a repeating pattern (up / down / up-down / random) at a
configurable tempo. Works with the existing 6-voice pool, sampler mode, and MIDI
output. One voice at a time (mono output), cycling through any number of held keys.

## Global Constraints

- **Monophonic output** — one note at a time in the arp cycle; avoids excessive
  voice stealing while still cycling through N held notes.
- When arp is on, key-press note-ons are **suppressed** — the arp generates
  note-ons from the held set on the sample clock.
- Must respect all global state: waveform, octave, volume, filter, sampler mode,
  IMU expression, and MIDI settings.
- **Fn+key** for arp controls (toggle, mode, tempo). This requires exposing the
  Fn modifier through `cardputer_hw` first — the M5Stack keyboard driver already
  tracks it in `KeysState.fn`; we just need the wrapper.
- Tempo via **Fn + volume keys** (`Fn+-` / `Fn+=`) — no key repurposing; volume
  keys keep their normal function when Fn is not held.
- All existing behaviour (arp off) unchanged.
- Conventional commits, body ends with `Assisted by AI.`

## File Structure

| File | Action |
|------|--------|
| `lib/cardputer_hw/cardputer_hw.h` | **modify** — add `fnHeld()` |
| `lib/cardputer_hw/cardputer_hw.cpp` | **modify** — implement `fnHeld()` |
| `src/synth/main.cpp` | **modify** — held-key set, arp state, logic, UI |
| `src/synth/arp.h` | **create** — pure-header arp engine (host-testable) |
| `src/synth/arp_test.cpp` | **create** — host tests for arp.h |

**Why a pure header?** Per architecture, all DSP/logic lives in host-testable
headers. The arp pattern logic (mode stepping, note selection) is testable
without Arduino. Only the HW glue (keys, display, MIDI calls) stays in
`main.cpp`.

---

## Tasks

### Task 0: Expose Fn modifier through cardputer_hw

**Files:**
- Modify: `lib/cardputer_hw/cardputer_hw.h`
- Modify: `lib/cardputer_hw/cardputer_hw.cpp`

**Goal:** Add `bool fnHeld()` so the rest of the synth can detect Fn+key combos.
This unblocks proper Fn+a / Fn+s / Fn+volume-key bindings (the architecture
already calls for Fn-layer in phase 3; we need it now for arp controls).

**Steps:**
1. In `cardputer_hw.h`, add to the `cardputer` namespace:
   ```cpp
   bool fnHeld();
   ```
2. In `cardputer_hw.cpp`, implement:
   ```cpp
   bool fnHeld() {
       return M5Cardputer.Keyboard.keysState().fn;
   }
   ```
3. **Hardware verify**: Flash a test build that `/e` prints `Fn: 1` / `Fn: 0`
   on the display when Fn is held/released. Confirm that Fn+letter still
   reports the letter character in `keysHeld()` (i.e., Fn does not remap
   letter keys — it is a pure modifier flag). *This is the open risk from
   architecture.md "Fn-combo reporting"; resolve it here.*

**Checkpoint:**
- Build: `pio run -e synth`
- Flash + verify Fn state on display
- Commit: `feat(synth): expose Fn key state via cardputer::fnHeld()`

---

### Task 1: Held-key set + normal-mode preservation

**Files:**
- Modify: `src/synth/main.cpp`

**Goal:** Track held keys as a `std::set<char>` so the arp can read the full
held set. Normal note-on/note-off is unchanged when arp is off.

**Steps:**
1. Add `#include <set>` and `#include <algorithm>`.
2. Add global: `std::set<char> g_heldKeys;`
3. In `loop()`, after `cardputer::update()`:
   - `auto nowVec = cardputer::keysHeld();`
   - `std::set<char> newHeld(nowVec.begin(), nowVec.end());`
   - Compute `justPressed` / `justReleased` via `std::set_difference`.
   - `g_heldKeys = newHeld;`
4. No behavioural change yet — the existing justPressed / gate-release logic
   still runs on the same data.

**Checkpoint:**
- Build + flash; synth behaves identically to before.
- Commit: `feat(synth): held-key set for arpeggiator preparation`

---

### Task 2: Arp engine (pure header + host tests)

**Files:**
- Create: `src/synth/arp.h`
- Create: `src/synth/arp_test.cpp`

**Goal:** Encapsulate the arp pattern logic in a testable header: mode enum,
step function (given a sorted note list + current index + direction, return the
next note index), and a rebuild helper.

**Design:**
```cpp
namespace synth {

enum class ArpMode { Up, Down, UpDown, Random };

struct ArpState {
    ArpMode mode       = ArpMode::Up;
    size_t  index      = 0;
    bool    dirUp      = true;   // for UpDown
};

// Advance the arp state and return the index of the next note to play.
// `n` = number of notes in the sorted list (must be >= 1).
inline size_t arpStep(ArpState& s, size_t n) {
    if (n == 0) return 0;  // caller must check
    switch (s.mode) {
        case ArpMode::Up: {
            size_t i = s.index % n;
            s.index = i + 1;
            return i;
        }
        case ArpMode::Down: {
            if (s.index == 0) s.index = n;
            s.index--;
            return s.index;
        }
        case ArpMode::UpDown: {
            size_t i = s.index;
            if (s.dirUp) {
                if (s.index < n - 1) s.index++;
                else { s.dirUp = false; if (s.index > 0) s.index--; }
            } else {
                if (s.index > 0) s.index--;
                else { s.dirUp = true; if (s.index < n - 1) s.index++; }
            }
            return i;
        }
        case ArpMode::Random:
            return rand() % n;   // seeded by caller
    }
    return 0;
}

// Reset position after a held-set change (or on enable).
inline void arpReset(ArpState& s) {
    s.index = 0;
    s.dirUp = true;
}

inline const char* arpModeName(ArpMode m) {
    switch (m) {
        case ArpMode::Up:      return "UP";
        case ArpMode::Down:    return "DOWN";
        case ArpMode::UpDown:  return "U/D";
        case ArpMode::Random:  return "RND";
        default:               return "?";
    }
}

}  // namespace synth
```

**Tests** (`arp_test.cpp`):
- `Up` with 3 notes cycles 0 → 1 → 2 → 0 …
- `Down` with 3 notes cycles 2 → 1 → 0 → 2 …
- `UpDown` with 3 notes cycles 0 → 1 → 2 → 1 → 0 → 1 …
- `Random` returns indices within range
- `arpReset` zeroes index + resets dirUp

**Checkpoint:**
- `g++ -std=c++17 src/synth/arp_test.cpp -o /tmp/arp && /tmp/arp` → `ok`
- Commit: `feat(synth): arp pattern engine (pure header, host-tested)`

---

### Task 3: Arp toggle + mode cycle (Fn+key controls)

**Files:**
- Modify: `src/synth/main.cpp`

**Goal:** Wire up the Fn+key combos and display the arp state.

**Key bindings:**
| Combo | Action |
|-------|--------|
| `Fn + a` | Toggle arpeggiator on/off |
| `Fn + s` | Cycle arp mode (Up → Down → UpDown → Random → Up) |
| `Fn + -` | Decrease tempo (increase interval by 10 ms) |
| `Fn + =` | Increase tempo (decrease interval by 10 ms) |

These require the `cardputer::fnHeld()` from Task 0. All combos are
Fn-gated — they never conflict with note keys or normal controls.

**Steps:**
1. Add globals:
   ```cpp
   static bool             g_arpEnabled    = false;
   static synth::ArpState  g_arp;
   static unsigned int      g_arpIntervalMs = 200;   // default ≈ 5 nps
   static unsigned long     g_lastArpTime  = 0;
   static constexpr unsigned ARP_MIN_MS    = 50;
   static constexpr unsigned ARP_MAX_MS    = 1000;
   ```
2. In the key-handling loop (inside `if (!g_arpEnabled)` branch for normal
   note-on, but Fn combos are checked **before** the normal branch — they
   are global regardless of arp state):
   ```cpp
   // --- Fn combos (checked first, before note handling) ---
   if (cardputer::fnHeld()) {
       if (justPressed contains 'a') g_arpEnabled = !g_arpEnabled;
       if (g_arpEnabled && justPressed contains 's')
           g_arp.mode = (synth::ArpMode)(((int)g_arp.mode + 1) % 4);
       if (g_arpEnabled && justPressed contains '-') {
           if (g_arpIntervalMs < ARP_MAX_MS) g_arpIntervalMs += 10;
       }
       if (g_arpEnabled && justPressed contains '=') {
           if (g_arpIntervalMs > ARP_MIN_MS) g_arpIntervalMs -= 10;
       }
       // suppress the combo key from normal handling
       // (remove 'a','s','-','=' from justPressed if present)
   }
   ```
3. Update `redraw()` to show arp status when enabled:
   ```cpp
   if (g_arpEnabled)
       M5.Display.printf("\nARP %s %dms", synth::arpModeName(g_arp.mode), g_arpIntervalMs);
   ```

**Checkpoint:**
- Build + flash; `Fn+a` toggles ARP line on display; `Fn+s` cycles mode;
  `Fn+-`/`Fn+=` adjust interval shown.
- Normal play (arp off) unchanged.
- Commit: `feat(synth): arpeggiator toggle + mode + tempo controls`

---

### Task 4: Arp note generation (clocked)

**Files:**
- Modify: `src/synth/main.cpp`

**Goal:** Generate arp notes from the held set, clocked by `g_arpIntervalMs`
using the sample-clock (`millis()`), and drive the Voice pool (one voice at a
time).

**Steps:**
1. Add held-key → sorted-note list builder:
   ```cpp
   static std::vector<std::pair<int,char>> g_arpNotes;  // (midiNote, key)

   static void rebuildArpNotes() {
       g_arpNotes.clear();
       for (char key : g_heldKeys) {
           int midi = synth::noteToMidi(synth::keyToSemitone(key), g_octave);
           g_arpNotes.emplace_back(midi, key);
       }
       std::sort(g_arpNotes.begin(), g_arpNotes.end());
       synth::arpReset(g_arp);
   }
   ```
2. Mark the arp dirty when the held set changes (justPressed / justReleased
   non-empty while arp is on).
3. In the arp-enabled branch of `loop()`:
   ```cpp
   if (g_arpEnabled) {
       if (g_arpDirty) { rebuildArpNotes(); g_arpDirty = false; }

       unsigned long now = millis();
       if (!g_arpNotes.empty() && now - g_lastArpTime >= g_arpIntervalMs) {
           size_t i = synth::arpStep(g_arp, g_arpNotes.size());
           char key = g_arpNotes[i].second;
           int  midi = g_arpNotes[i].first;

           // kill previous arp voice
           if (g_arpCurrentKey) {
               int vi = synth::findVoiceByKey(g_voices, N_VOICES, g_arpCurrentKey);
               if (vi >= 0) { g_voices[vi].env.gateOff(); g_voices[vi].key = 0; }
               #ifdef SYNTH_USB_MIDI
               usbMidi.sendNoteOff(g_arpCurrentMidi, 0, 1);
               #endif
               #ifdef SYNTH_BLE_MIDI
               MIDI.sendNoteOff(g_arpCurrentMidi, 0, 1);
               #endif
           }

           // trigger new arp note
           uint8_t vel = synth::tiltVelocity(g_tiltFwd);
           noteOn(key, synth::keyToSemitone(key), vel);
           g_arpCurrentKey  = key;
           g_arpCurrentMidi = midi;
           #ifdef SYNTH_USB_MIDI
           usbMidi.sendNoteOn(midi, vel, 1);
           #endif
           #ifdef SYNTH_BLE_MIDI
           MIDI.sendNoteOn(midi, vel, 1);
           #endif

           g_lastArpTime = now;
       }

       // all keys released → kill last arp note
       if (g_heldKeys.empty() && g_arpCurrentKey) {
           int vi = synth::findVoiceByKey(g_voices, N_VOICES, g_arpCurrentKey);
           if (vi >= 0) { g_voices[vi].env.gateOff(); g_voices[vi].key = 0; }
           g_arpCurrentKey = 0;
       }
   }
   ```
4. In the arp-enabled branch, **skip** the normal `justPressed` note-on /
   gate-release handling so the arp owns note lifecycle. Control keys (octave,
   waveform, filter, volume, record, panic) still function.

**Checkpoint:**
- Build + flash; enable arp via Fn+a; hold 2–3 note keys → steady arpeggio at
  the set interval.
- Fn+s changes pattern; Fn+-/Fn+= changes speed.
- Release all keys → last arp note stops (no stuck notes).
- Normal mode (arp off) still works.
- Commit: `feat(synth): clocked arpeggiator note generation`

---

### Task 5: Polish + hardware verification

**Files:**
- Modify: `src/synth/main.cpp`

**Goal:** Edge cases, sampler/MIDI integration, display polish.

**Steps:**
1. **Sampler mode:** while arp is on, press `5` — arp notes should play the
   recorded sample as oscillator source (this works automatically because
   `noteOn` uses the current `g_wave`; if sampler is active the voice picks up
   the sample).
2. **MIDI out:** verify Fn+a then hold keys sends MIDI note-on/off pairs at the
   arp tempo (USB and BLE envs).
3. **Stuck-note safety:** when toggling arp off, kill any lingering arp voice
   + send MIDI note-off for `g_arpCurrentKey`. When toggling arp back on,
   reset `arpReset` + clear `g_arpCurrentKey`.
4. **Panic key (`):** already kills all voices; verify it also resets arp state
   (`g_arpCurrentKey = 0`, `arpReset`).
5. **Display:** show BPM instead of raw ms? `BPM = 60000 / g_arpIntervalMs`.
   Example: 200 ms → 300 BPM (this is "notes per minute", not quarter notes;
   label it `ARP 300 NPM` or just `ARP 200ms` for clarity).
6. **Rapid held-set changes:** press/release keys quickly while arp is on —
   `rebuildArpNotes` on each change resets the index, which may cause a
   "restart-from-bottom" feel. Consider only resetting if the note count
   changes, not when the same keys are held.

**Checkpoint:**
- All of the above pass on device.
- Commit: `feat(synth): arpeggiator polish + hw verification`

---

## Key bindings summary (arp scope)

| Combo | Action | Notes |
|-------|--------|-------|
| `Fn + a` | Toggle arp on/off | Kills last arp voice on off |
| `Fn + s` | Cycle arp mode | Only when arp is on |
| `Fn + -` | Slower (interval +10 ms) | Clamped 50–1000 ms |
| `Fn + =` | Faster (interval −10 ms) | Clamped 50–1000 ms |

Volume keys `-`/`=` **without** Fn retain their normal volume function.

## Architecture alignment

This plan is **phase 7** per `docs/design/architecture.md`. It depends on:
- Phase 1 (polyphonic engine) — ✅ shipped (commits `0cc9f21`..`c84a2d0`)
- Phase 3 (Fn-layer) — partially: we implement the `fnHeld()` wrapper here
  because arp needs it, but the full Fn-layer keymap redesign is phase 3's
  scope; our addition is minimal and non-breaking.

The arp logic is in a pure header (`arp.h`), host-testable, matching the
architecture's "all DSP/logic lives in pure headers" rule. Only the key/
display/MIDI glue lives in `main.cpp`.

## References

- `docs/design/architecture.md` — phase plan, module list (see `arp.h`),
  Fn-layer spec
- `src/synth/main.cpp` — current key handling, voice allocation, noteOn/noteOff
- `lib/cardputer_hw/cardputer_hw.{h,cpp}` — keyboard wrapper (needs `fnHeld()`)
- `docs/hardware-verification.md` — flash-gate checklist
