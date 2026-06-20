// Linear ADSR envelope + monophonic gate. Pure math, NO Arduino/M5 includes —
// host-testable with plain g++ (see adsr_test.cpp). Hardware glue (speaker
// streaming, keyboard gate) lives in main.cpp.
//
// Model: gate on -> Attack (ramp 0->1) -> Decay (ramp 1->sustain) -> Sustain
// (hold while key held) -> gate off -> Release (ramp current->0) -> Idle.
// step() advances one output sample and returns amplitude 0..1.
#pragma once

namespace synth {

enum class EnvStage { Idle, Attack, Decay, Sustain, Release };

struct Adsr {
  // Config in samples (attack/decay/release) and level (sustain, 0..1).
  int    attack  = 1;     // samples to ramp 0 -> 1      (>= 1)
  int    decay   = 1;     // samples to ramp 1 -> sustain (>= 1)
  double sustain = 1.0;   // hold level 0..1
  int    release = 1;     // samples to ramp current -> 0 (>= 1)

  EnvStage stage   = EnvStage::Idle;
  double   level   = 0.0; // current amplitude 0..1
  double   relStep = 0.0; // per-sample release decrement, fixed at gate-off

  static int clampPos(int n) { return n < 1 ? 1 : n; }

  // Set ADSR from milliseconds at sample rate sr.
  void configMs(int sr, double aMs, double dMs, double sLevel, double rMs) {
    attack  = clampPos((int)(aMs * sr / 1000.0));
    decay   = clampPos((int)(dMs * sr / 1000.0));
    release = clampPos((int)(rMs * sr / 1000.0));
    sustain = sLevel < 0.0 ? 0.0 : (sLevel > 1.0 ? 1.0 : sLevel);
  }

  // Gate on: (re)trigger from the current level — Attack always rises to 1,
  // so a retrigger mid-release clicks up cleanly rather than jumping.
  void gateOn() { stage = EnvStage::Attack; }

  // Gate off: ramp from wherever we are now down to 0 over `release` samples.
  void gateOff() {
    if (stage == EnvStage::Idle) return;
    relStep = level / release;
    stage = EnvStage::Release;
  }

  bool active() const { return stage != EnvStage::Idle; }

  // Advance one sample, return amplitude 0..1.
  double step() {
    switch (stage) {
      case EnvStage::Idle:
        level = 0.0;
        break;
      case EnvStage::Attack:
        level += 1.0 / attack;
        if (level >= 1.0) { level = 1.0; stage = EnvStage::Decay; }
        break;
      case EnvStage::Decay:
        level -= (1.0 - sustain) / decay;
        if (level <= sustain) { level = sustain; stage = EnvStage::Sustain; }
        break;
      case EnvStage::Sustain:
        level = sustain;
        break;
      case EnvStage::Release:
        level -= relStep;
        // 1e-9 not 0.0: float accumulation can leave a dangling ~1e-17 sample.
        if (level <= 1e-9) { level = 0.0; stage = EnvStage::Idle; }
        break;
    }
    return level;
  }
};

}  // namespace synth
