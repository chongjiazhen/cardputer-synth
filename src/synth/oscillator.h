// Pure oscillator math. No Arduino/M5 includes — host-testable.
// osc(shape, phase) returns sample in -1..1. phase in radians, expected 0..2π
// but wrapping is caller's responsibility (main.cpp keeps phase mod 2π).
#pragma once
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

// PolyBLEP (polynomial band-limited step) residual for a unit +1 step
// discontinuity, evaluated near a waveform edge. `t` = normalized phase in
// [0,1); `dt` = normalized frequency (cycles per sample = phaseInc/2π). Returns
// a small correction that rounds off the naive discontinuity, cancelling most
// of the aliased harmonics a raw ramp/step would fold back below Nyquist.
// Two-sample kernel (Välimäki/Huovilainen). dt<=0 disables it (naive output).
inline float polyBlep(float t, float dt) {
  if (dt <= 0.0f) return 0.0f;
  if (t < dt) {              // sample just after a rising edge at t=0
    float x = t / dt;
    return x + x - x * x - 1.0f;
  }
  if (t > 1.0f - dt) {       // sample just before the edge at t=1 (wrap)
    float x = (t - 1.0f) / dt;
    return x * x + x + x + 1.0f;
  }
  return 0.0f;
}

// osc(shape, phase, dt): band-limited when dt > 0, naive when dt == 0.
// `dt` is the per-sample phase advance normalized to one cycle (phaseInc/2π);
// pass it so Saw/Square get PolyBLEP anti-aliasing. Sine is already band-limited;
// Tri's harmonics roll off 1/n² so it stays naive (not the harsh offender).
inline float osc(WaveShape shape, float phase, float dt = 0.0f) {
  float t = phase / TWO_PI_F;   // normalized phase 0..1
  switch (shape) {
    case WaveShape::Sine:   return std::sin(phase);
    case WaveShape::Saw: {
      // Naive ramp -1..+1, discontinuity (+1→-1) at the wrap; BLEP rounds it.
      float s = 2.0f * t - 1.0f;
      s -= polyBlep(t, dt);
      return s;
    }
    case WaveShape::Square: {
      // Naive ±1, 50% duty: rising edge at t=0, falling edge at t=0.5.
      float s = (t < 0.5f) ? 1.0f : -1.0f;
      s += polyBlep(t, dt);                        // correct the rising edge
      // Falling edge at t=0.5: measure phase PAST 0.5 (t-0.5), not t+0.5 — the
      // latter rounds to exactly 1.0 at t≈0.5 and wraps to 0.0, misapplying the
      // rising-edge residual and doubling the output. t-0.5 rounds consistently.
      float t2 = t - 0.5f; if (t2 < 0.0f) t2 += 1.0f;
      s -= polyBlep(t2, dt);                       // correct the falling edge
      return s;
    }
    case WaveShape::Tri:
      // 0..π  → -1..+1  (rising)
      // π..2π → +1..-1  (falling)
      if (phase < PI_F) return (2.0f * phase / PI_F) - 1.0f;
      else              return 3.0f - (2.0f * phase / PI_F);
    default:
      return 0.0f;
  }
}

}  // namespace synth
