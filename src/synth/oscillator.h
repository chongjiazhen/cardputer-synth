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
