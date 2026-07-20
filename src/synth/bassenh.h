// Missing-fundamental bass enhancer. A 1 W micro-speaker can't reproduce low
// fundamentals (below ~250 Hz it barely moves air), but the ear reconstructs a
// note's pitch from its harmonic series even when the fundamental is absent
// (the "missing fundamental" / residue-pitch effect). So for low notes we mix
// in a scaled harmonic series (2f, 3f, 4f) that lands where the speaker IS
// strong, and the brain fills in the bass the driver can't produce.
//
// Pure math, no Arduino/M5 includes, host-testable. Phase-locked to the voice's
// own phase so the added harmonics don't beat against the fundamental. Apply it
// to the raw oscillator output INSIDE the voice (before filter + amp envelope),
// so the harmonics get shaped and released with the note.
#pragma once
#include <cmath>
#include "oscillator.h"   // TWO_PI_F

namespace synth {

struct BassEnh {
  bool  on       = true;
  float cutoffHz = 250.0f;   // enhance notes whose fundamental is below this
  float amount   = 0.5f;     // max harmonic mix, reached at the lowest notes
};

// Enhancement gain for a fundamental f0: 0 at/above cutoff, ramps toward
// `amount` as the note drops (linear in how far below cutoff it sits).
inline float bassEnhGain(const BassEnh& b, float f0) {
  if (!b.on || f0 <= 0.0f || f0 >= b.cutoffHz) return 0.0f;
  float t = (b.cutoffHz - f0) / b.cutoffHz;    // 0 at cutoff -> ~1 near DC
  return b.amount * (t > 1.0f ? 1.0f : t);
}

// Add a phase-locked harmonic series (2f,3f,4f, rolling off) to `base`, scaled
// by `gain` (from bassEnhGain). `phase` is the voice's fundamental phase (rad).
inline float bassEnhApply(float base, float phase, float gain) {
  if (gain <= 0.0f) return base;
  float h = 0.50f * std::sin(2.0f * phase)
          + 0.33f * std::sin(3.0f * phase)
          + 0.25f * std::sin(4.0f * phase);
  return base + gain * h;
}

// Fundamental Hz from a voice's base phase increment + sample rate.
inline float bassEnhF0(float phaseInc, double sampleRate) {
  return phaseInc * (float)sampleRate / TWO_PI_F;
}

}  // namespace synth
