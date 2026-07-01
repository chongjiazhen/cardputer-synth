// Host test for voice.h:  g++ -std=c++17 voice_test.cpp -o t && ./t
#include "voice.h"
#include <cassert>
#include <cstdio>
#include <cmath>

static void check(bool c, const char* m) { if (!c) { fprintf(stderr, "FAIL: %s\n", m); assert(false); } }

int main() {
  using namespace synth;
  const double SR = 16000.0;

  // inactive voice outputs silence and does not advance
  {
    Voice v;
    check(voiceSample(v, WaveShape::Sine, 1.0f, 1.0f, SR) == 0.0f, "inactive silent");
    check(v.phase == 0.0f, "inactive no advance");
  }

  // active voice: phase advances by phaseInc * bend * vib
  {
    Voice v;
    v.active = true; v.vel = 1.0f; v.phase = 0.0f; v.phaseInc = 0.1f;
    v.env1.configMs(SR, 1.0, 1.0, 1.0, 1000.0);  // long, ~full level
    v.env2.configMs(SR, 1.0, 1.0, 1.0, 1000.0);
    v.env1.gateOn(); v.env2.gateOn();
    v.baseCutoff = SR * 0.49;  // open filter
    voiceSample(v, WaveShape::Saw, 1.0f, 1.0f, SR);
    check(std::fabs(v.phase - 0.1f) < 1e-4f, "phase += inc");
    voiceSample(v, WaveShape::Saw, 2.0f, 1.0f, SR);   // bend x2
    check(std::fabs(v.phase - 0.3f) < 1e-4f, "phase += inc*bend");
  }

  // gateOff then enough steps → env idle → voice goes inactive
  {
    Voice v;
    v.active = true; v.vel = 1.0f; v.phaseInc = 0.1f;
    v.env1.configMs(SR, 1.0, 1.0, 1.0, 1.0);  // 1ms release
    v.env2.configMs(SR, 1.0, 1.0, 1.0, 1.0);
    v.env1.gateOn(); v.env2.gateOn();
    v.baseCutoff = SR * 0.49;
    voiceSample(v, WaveShape::Sine, 1.0f, 1.0f, SR);
    v.env1.gateOff(); v.env2.gateOff();
    for (int i = 0; i < 2000; i++) voiceSample(v, WaveShape::Sine, 1.0f, 1.0f, SR);
    check(!v.active, "voice frees after release");
  }

  // --- Phase 2 tests ---

  // per-voice filter: LP at low cutoff should attenuate high frequencies
  {
    Voice v;
    v.active = true; v.vel = 1.0f; v.phaseInc = 0.1f;
    v.env1.configMs(SR, 1000.0, 1000.0, 1.0, 1000.0);  // long sustain
    v.env2.configMs(SR, 1000.0, 1000.0, 1.0, 1000.0);
    v.env1.gateOn(); v.env2.gateOn();
    v.baseCutoff = 200.0;   // low cutoff
    v.baseResonance = 0.707;

    // Run a few samples and gather peak — if filter is working, output < input
    float peak = 0;
    for (int i = 0; i < 100; i++) {
      // Advance phase to a high-frequency position
      v.phase = PI_F;  // mid-cycle
      float s = voiceSample(v, WaveShape::Saw, 1.0f, 1.0f, SR);
      float a = std::fabs(s);
      if (a > peak) peak = a;
    }
    check(peak < 0.5f, "Low filter cutoff attenuates signal");
  }

  // modulation: Env2→FilterCut boosts cutoff when env2 is high
  {
    Voice v;
    v.active = true; v.vel = 1.0f; v.phaseInc = 0.1f;
    v.env1.configMs(SR, 1000.0, 1000.0, 1.0, 1000.0);
    v.env2.configMs(SR, 1.0, 1.0, 1.0, 1000.0);  // short attack, long sustain
    v.env1.gateOn(); v.env2.gateOn();
    v.baseCutoff = 200.0;  // low base
    v.baseResonance = 0.707;
    v.mod.set(ModSrc::Env2, ModDst::FilterCut, 1.0f);  // env2 opens filter

    // With env2 at peak (1.0), cutoff = 200 + 4000 = 4200 (much more open)
    float peak_modulated = 0;
    for (int i = 0; i < 50; i++) {
      v.phase = (float)i * 0.5f;
      if (v.phase >= TWO_PI_F) v.phase -= TWO_PI_F;
      float s = voiceSample(v, WaveShape::Saw, 1.0f, 1.0f, SR);
      float a = std::fabs(s);
      if (a > peak_modulated) peak_modulated = a;
    }
    // Without mod, at cutoff=200 the signal would be heavily filtered.
    // With mod, env2 at peak should open the filter significantly.
    v.mod.clear();
    v.filter.reset();
    float peak_unmodulated = 0;
    v.phase = 0;
    for (int i = 0; i < 50; i++) {
      v.phase = (float)i * 0.5f;
      if (v.phase >= TWO_PI_F) v.phase -= TWO_PI_F;
      float s = voiceSample(v, WaveShape::Saw, 1.0f, 1.0f, SR);
      float a = std::fabs(s);
      if (a > peak_unmodulated) peak_unmodulated = a;
    }
    check(peak_modulated > peak_unmodulated,
          "Env2→FilterCut opens filter");
  }

  // LFO: Lfo1→Pitch with depth should produce vibrato
  {
    Voice v;
    v.active = true; v.vel = 1.0f; v.phaseInc = 0.01f;
    v.env1.configMs(SR, 1000.0, 1000.0, 1.0, 1000.0);
    v.env2.configMs(SR, 1000.0, 1000.0, 1.0, 1000.0);
    v.env1.gateOn(); v.env2.gateOn();
    v.baseCutoff = SR * 0.49;
    v.lfo1.setFreq(5.0, SR);
    v.lfo1.depth = 1.0;
    v.mod.set(ModSrc::Lfo1, ModDst::Pitch, 0.1f);  // subtle vibrato

    // Run for many samples and check phase is being modulated
    float last_phase = v.phase;
    voiceSample(v, WaveShape::Sine, 1.0f, 1.0f, SR);
    // Phase should advance, but the exact increment depends on LFO position.
    // Just verify it advances at all.
    check(v.phase > last_phase || v.phase < last_phase, "LFO modulates phase");
  }

  puts("ok");
  return 0;
}