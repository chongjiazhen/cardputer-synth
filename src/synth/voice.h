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
