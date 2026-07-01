// One synthesizer voice with per-voice SVF filter, dual envelopes, dual LFOs,
// and modulation matrix. Pure math, no Arduino/M5 includes — host-testable.
#pragma once
#include "oscillator.h"
#include "adsr.h"
#include "sampler.h"
#include "filter.h"
#include "lfo.h"
#include "modmatrix.h"

namespace synth {

inline float keyTrackNorm(int midi) {
  if (midi < 21) return 0.0f;
  float k = (float)(midi - 21) / 87.0f;
  return k > 1.0f ? 1.0f : k;
}

struct Voice {
  bool       active     = false;  // sounding (gate held OR release tail)
  char       key        = 0;      // gating key char, 0 = not gated
  int        midi       = 0;      // MIDI note number
  int        age        = 0;      // allocation order; lower = older (steal target)
  float      phase      = 0.0f;   // oscillator phase, 0..TWO_PI_F
  float      phaseInc   = 0.0f;   // base radians/sample for the note
  float      vel        = 1.0f;   // 0..1 latched velocity
  float      samplePos  = 0.0f;   // sampler: read position
  float      sampleStep = 1.0f;   // sampler: samples advanced per output sample

  // --- Phase 2 additions ---
  Adsr       env1;             // amp envelope
  Adsr       env2;             // filter / modulation envelope
  Lfo        lfo1;             // LFO 1
  Lfo        lfo2;             // LFO 2
  SVFilter   filter;           // per-voice resonant SVF
  ModMatrix  mod;              // per-voice modulation routing

  // Base parameter values (before modulation)
  float      baseCutoff  = 8000.0f;  // Hz, default open
  float      baseResonance = 0.707f; // Q, Butterworth default
  FilterMode filterMode = FilterMode::LP;
};

// Render one sample for a voice and advance its state.
// Uses the modulation matrix to route env1, env2, lfo1, lfo2, velocity,
// and key-tracking to pitch, filter cutoff/resonance, and amp.
// `bendRatio` and `vibRatio` are external pitch modifiers (IMU).
inline float voiceSample(Voice& v, WaveShape wave,
                          float bendRatio, float vibRatio,
                          double sampleRate) {
  if (!v.active) return 0.0f;

  // Step envelopes and LFOs
  float e1 = (float)v.env1.step();
  float e2 = (float)v.env2.step();
  float l1 = (float)v.lfo1.step();
  float l2 = (float)v.lfo2.step();

  // Build modulation source values
  float src[MOD_SRC_COUNT] = {};
  src[(int)ModSrc::Env1]     = e1;
  src[(int)ModSrc::Env2]     = e2;
  src[(int)ModSrc::Lfo1]     = l1;
  src[(int)ModSrc::Lfo2]     = l2;
  src[(int)ModSrc::Velocity] = v.vel;
  // Key-track: normalize MIDI 21..108 to 0..1
  src[(int)ModSrc::KeyTrack] = keyTrackNorm(v.midi);

  // Process modulation matrix
  v.mod.process(src);

  // Apply modulations to parameters
  // Pitch: base + mod offset (in semitones, scaled)
  float pitchMod = v.mod.get(ModDst::Pitch);
  float pitchRatio = std::pow(2.0f, pitchMod / 12.0f);

  // Filter cutoff: base + mod offset (in Hz, additive)
  float cutoffMod = v.mod.get(ModDst::FilterCut) * 4000.0f;  // ±4000 Hz range
  float cutoff = v.baseCutoff + cutoffMod;
  if (cutoff < 20.0f) cutoff = 20.0f;
  if (cutoff > sampleRate * 0.49) cutoff = sampleRate * 0.49f;

  // Filter resonance: base + mod offset
  float resMod = v.mod.get(ModDst::FilterRes) * 5.0f;  // ±5 Q range
  float resonance = v.baseResonance + resMod;
  if (resonance < 0.1f) resonance = 0.1f;
  if (resonance > 20.0f) resonance = 20.0f;

  // Update filter coefficients (only when cutoff/res changes significantly;
  // for simplicity we recompute every sample — cheap for biquad)
  v.filter.calcCoeffs(v.filterMode, cutoff, resonance, sampleRate);

  // Amp: env1 × velocity × mod offset
  float ampMod = 1.0f + v.mod.get(ModDst::Amp);
  if (ampMod < 0.0f) ampMod = 0.0f;

  // Render oscillator → filter → amp. PolyBLEP anti-aliasing needs the
  // per-sample phase advance (normalized to one cycle) as dt.
  float inc = v.phaseInc * bendRatio * vibRatio * pitchRatio;
  float dt  = inc / TWO_PI_F;
  float oscOut = osc(wave, v.phase, dt) * shapeGain(wave);
  float filtered = (float)v.filter.process((double)oscOut);
  float s = filtered * e1 * v.vel * ampMod;

  // Advance oscillator phase
  v.phase += inc;
  if (v.phase >= TWO_PI_F) v.phase -= TWO_PI_F;

  // Voice goes inactive when amp envelope finishes
  if (!v.env1.active()) v.active = false;
  return s;
}

// Sampler variant: plays from a recorded buffer instead of the oscillator,
// through the same filter/modulation path.
inline float voiceSampleBuf(Voice& v, const int16_t* buf, int len,
                             float bendRatio, float vibRatio,
                             double sampleRate) {
  if (!v.active) return 0.0f;

  float e1 = (float)v.env1.step();
  float e2 = (float)v.env2.step();
  float l1 = (float)v.lfo1.step();
  float l2 = (float)v.lfo2.step();

  float src[MOD_SRC_COUNT] = {};
  src[(int)ModSrc::Env1]     = e1;
  src[(int)ModSrc::Env2]     = e2;
  src[(int)ModSrc::Lfo1]     = l1;
  src[(int)ModSrc::Lfo2]     = l2;
  src[(int)ModSrc::Velocity] = v.vel;
  src[(int)ModSrc::KeyTrack] = keyTrackNorm(v.midi);

  v.mod.process(src);

  float pitchMod = v.mod.get(ModDst::Pitch);
  float pitchRatio = std::pow(2.0f, pitchMod / 12.0f);

  float cutoffMod = v.mod.get(ModDst::FilterCut) * 4000.0f;
  float cutoff = v.baseCutoff + cutoffMod;
  if (cutoff < 20.0f) cutoff = 20.0f;
  if (cutoff > sampleRate * 0.49) cutoff = sampleRate * 0.49f;

  float resMod = v.mod.get(ModDst::FilterRes) * 5.0f;
  float resonance = v.baseResonance + resMod;
  if (resonance < 0.1f) resonance = 0.1f;
  if (resonance > 20.0f) resonance = 20.0f;

  v.filter.calcCoeffs(v.filterMode, cutoff, resonance, sampleRate);

  float ampMod = 1.0f + v.mod.get(ModDst::Amp);
  if (ampMod < 0.0f) ampMod = 0.0f;

  float s = 0.0f;
  if (len > 0) {
    float rawSample = (float)sampleRead(buf, len, v.samplePos);
    float filtered = (float)v.filter.process((double)rawSample);
    s = filtered * e1 * v.vel * ampMod;
    v.samplePos += v.sampleStep * bendRatio * vibRatio * pitchRatio;
    if (v.samplePos >= (float)len) v.samplePos -= (float)len;
  }

  if (!v.env1.active()) v.active = false;
  return s;
}

}  // namespace synth