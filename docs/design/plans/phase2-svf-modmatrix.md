# Phase 2: Resonant SVF Filter + Env2 + 2 LFOs + Mod-Matrix

Fossilized Design & Implementation Plan for Phase 2 of the Cardputer ADV Synthesizer.

## 1. Objectives & Overview
The goal of Phase 2 is to move the synthesizer from a basic polyphonic tone generator into a fully sculptable subtractive synthesizer engine with rich dynamic modulation capabilities. 

This phase delivers:
1. **Per-Voice Resonant SVF Filter**: Replaces the legacy global master-bus one-pole low-pass filter with an Audio EQ Cookbook (RBJ) Direct Form II Transposed Biquad State Variable Filter (SVF) supporting Low-Pass (LP), High-Pass (HP), and Band-Pass (BP) modes.
2. **Dual Envelopes**: `env1` (Ampltiude/Gate envelope) and `env2` (Filter/Modulation envelope) using the pre-existing pure host-testable ADSR implementation.
3. **Dual LFOs**: Fast, low-overhead phase-accumulator LFOs supporting Sine, Triangle, Sawtooth, and Square wave shapes, with adjustable depth and frequency.
4. **Modulation Matrix**: A fast, fixed-size 2D matrix (`ModSrc::COUNT` x `ModDst::COUNT`) routing modulation sources (Env1, Env2, LFO1, LFO2, Velocity, KeyTrack) to modulation destinations (Pitch, FilterCut, FilterRes, Amp, OscShape, FmDepth, Lfo1Depth, Lfo2Depth) with additive accumulation per sample.
5. **Full Host-Testable DSP**: Maintaining the project invariant that all DSP code is 100% host-testable with regular g++ under a standard desktop environment without requiring ESP32 hardware/SDK dependencies.

---

## 2. Technical Decisions & Refinements

### Chamberlin SVF vs. RBJ Biquad SVF
During development, we initially implemented a classic **Chamberlin SVF** topology with an $f_c = 1 - e^{-2\pi f / f_s}$ mapping. While highly efficient, this topology suffered from a fatal **DC gain mismatch**:
* Under DC input ($f_{in} \approx 0$), the low-pass output failed to settle at unity gain, hovering around $\approx 0.21$ instead of $1.0$.
* This was caused by the mismatch between the single-pole coefficient mapping $f_c$ and the two-pole Chamberlin structure.
* **Resolution**: We rewrote the filter to use the **RBJ Biquad (Audio EQ Cookbook) Direct Form II Transposed** topology. This formulation provides mathematically guaranteed $1.0$ (0 dB) DC gain for the low-pass mode, and $0.0$ (-inf dB) DC gain for high-pass and band-pass modes. Testing verified that DC inputs settle exactly at $1.0$ for LP, and at $0.0$ for HP/BP.

### Modulation Matrix Implementation
Instead of a complex, dynamically allocated or pointer-based modulation routing system (which risks fragmentation and high CPU overhead on the ESP32-S3), we used a **dense static matrix** approach:
* We defined fixed `ModSrc` and `ModDst` enums.
* The modulation matrix `ModMatrix` stores a flat float array `depth[MOD_SRC_COUNT][MOD_DST_COUNT]`.
* In each sample process step, a float array of calculated source values is passed into `ModMatrix::process(src)`. This loops over all slots, multiplies source values by their depth, and accumulates the result into a per-destination `dst` array.
* The voice then fetches these offsets using `ModMatrix::get(dest)` and applies them.
* This approach is extremely cheap, deterministic, cache-friendly, and has zero allocation overhead.

---

## 3. Implementation Details

### File Structure & Symbols Added
* **`src/synth/filter.h`**
  * `enum class FilterMode { LP, HP, BP }`
  * `struct SVFilter`: Direct Form II Transposed biquad filter. Holds internal delay states `z1`, `z2` and coefficients `b0`, `b1`, `b2`, `a1`, `a2`.
  * `calcCoeffs(mode, cutoff, resonance, sampleRate)`: Computes RBJ coefficient formulas.
  * `process(double in)`: Filters a single sample and returns the result.
* **`src/synth/lfo.h`**
  * `enum class LfoShape { Sine, Tri, Saw, Square }`
  * `struct Lfo`: Uses a float-based phase accumulator `phase` and step size `phaseInc`.
  * `setFreq(hz, sr)`, `step()`: Advances phase and evaluates waveform shapes, returning scaled values in range `[-depth, +depth]`.
* **`src/synth/modmatrix.h`**
  * `enum class ModSrc` and `enum class ModDst` defining all inputs/outputs.
  * `struct ModMatrix`: Depth matrix and calculated offset array, with `set()`, `clear()`, `process()`, and `get()`.
* **`src/synth/voice.h`**
  * Upgraded `Voice` struct with `env1`, `env2`, `lfo1`, `lfo2`, `filter`, and `mod` matrix.
  * Legacied `env` as a reference alias to `env1` for backward compatibility.
  * `voiceSample()` and `voiceSampleBuf()`: Step all envelopes/LFOs $\rightarrow$ evaluate sources $\rightarrow$ run mod matrix $\rightarrow$ modulate pitch, filter cutoff, resonance, and amp $\rightarrow$ generate oscillator/sampler $\rightarrow$ filter $\rightarrow$ scale level.
* **`src/synth/main.cpp`**
  * Removed the global `OnePole` filter.
  * Integrated voice-level filters and modulation routing (default routing: `Env2 -> FilterCut` with depth 1.0).
  * Tied keyboard controls (`,`/`.` for cutoff, `6`/`7` for resonance) to update active voice and global defaults.
  * Modified keyboard display to show the active filter mode (LP/HP/BP), cutoff (Hz), and resonance (Q).

---

## 4. Test Verification
All DSP components are covered by host-test files compiling with `g++ -std=c++17` on any desktop environment:

### `filter_test.cpp` (8 Tests, All Pass)
1. LP DC Gain: Verifies steady-state DC step input settles at exactly $1.0$.
2. HP DC Gain: Verifies steady-state DC step input settles at exactly $0.0$.
3. BP DC Gain: Verifies steady-state DC step input settles at exactly $0.0$.
4. LPF Attenuation: Verifies high-frequency Nyquist signals are heavily attenuated by low cutoff.
5. HPF Transmission: Verifies high frequencies are passed by a high-pass filter.
6. Resonance: Verifies high-Q (Q=5) filters have higher peak gain near cutoff than Butterworth (Q=0.707).
7. Reset state: Verifies calling `reset()` clears internal delay lines `z1` and `z2`.
8. Cutoff comparison: Verifies a higher cutoff passes more high-frequency peak energy than a low cutoff.

### `lfo_test.cpp` (7 Tests, All Pass)
1. Sine Shape: Verifies quarter-period peaks at maximum depth.
2. Triangle Shape: Verifies half-period peaks at maximum depth.
3. Square Shape: Verifies output alternates between +depth and -depth.
4. Sawtooth Shape: Verifies shape starts near -depth and rises linearly.
5. Depth Control: Verifies output scales down proportionally with `depth`.
6. Reset Action: Verifies phase reset returns LFO to starting position.
7. Frequency Scaling: Verifies LFO step increments match frequency and sample rate.

### `modmatrix_test.cpp` (7 Tests, All Pass)
1. No routing: Zero sources/depths results in zero modulation offsets.
2. Single routing: Verifies scaling and assignment of a single source to destination.
3. Multiple sources: Verifies additive modulation from multiple sources to a single destination.
4. Negative depth: Verifies inversion of source signals.
5. One-to-many: Verifies routing one source to multiple destinations at different depths.
6. Clear routing: Verifies clearing resets all depths and outputs.
7. Zero sources: Verifies that zeroed sources yield zeroed outputs even with routing enabled.

### `voice_test.cpp` (6 Tests, All Pass)
1. Inactive Voice: Silent output, phase does not advance.
2. Active Voice: Phase advances properly incorporating bend, vibrato, and pitch modulation.
3. Envelope Release: Gated-off voice becomes inactive once the amplitude envelope release tail ends.
4. Filter Attenuation: LP at low cutoff attenuates high frequencies inside voice rendering.
5. Envelope Modulation: Verifies `Env2 -> FilterCut` opens up the cutoff and boosts output levels on envelope attack.
6. LFO Modulation: Verifies LFO pitch modulation continuously offsets phase increments (vibrato).

---

## 5. Outstanding & Post-Phase Tasks
* **PlatformIO Cross-Compilation**: Verified. The complete firmware with USB-MIDI and BLE-MIDI compiles and links successfully on PlatformIO for ESP32-S3 hardware.
* **IMU Vibrato Consolidation**: Currently, IMU-based vibrato/bend is computed externally in `main.cpp` and passed directly as parameters to `voiceSample()` for legacy reasons. In a future phase, IMU sources should be routed through the per-voice modulation matrix directly to keep the signal path completely unified.
* **Performance Tuning**: Verify coefficient recalculation overhead on actual hardware. Currently, `SVFilter::calcCoeffs()` is run on every single sample inside the voice loop. If CPU cycles are tight, we can optimize this to only recalculate coefficients when cutoff or resonance changes by more than a tiny threshold, or decimate it to calculate once every $N$ samples.
* **Headroom Check**: Summing 6 active voices through per-voice filters could result in higher peak amplitudes. Verify on device if soft limiter threshold or headroom scaling needs adjustment.
