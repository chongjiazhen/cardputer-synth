// Pure oscillator math. No Arduino/M5 includes — host-testable.
// osc(shape, phase) returns sample in -1..1. phase in radians, expected 0..2π
// but wrapping is caller's responsibility (main.cpp keeps phase mod 2π).
#pragma once
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif
#include <cmath>

namespace synth {

enum class WaveShape { Sine, Saw, Square, Tri };

inline const char* shapeName(WaveShape s) {
  switch (s) {
    case WaveShape::Sine:   return "SINE";
    case WaveShape::Saw:    return "SAW";
    case WaveShape::Square: return "SQR";
    case WaveShape::Tri:    return "TRI";
    default:                return "?";
  }
}

inline double osc(WaveShape shape, double phase) {
  switch (shape) {
    case WaveShape::Sine:
      return std::sin(phase);
    case WaveShape::Saw:
      // Rises linearly from -1 at phase=0 to +1 at phase approaching 2π.
      return 2.0 * phase / (2.0 * M_PI) - 1.0;
    case WaveShape::Square:
      return phase < M_PI ? 1.0 : -1.0;
    case WaveShape::Tri:
      // 0..π  → -1..+1  (rising)
      // π..2π → +1..-1  (falling)
      if (phase < M_PI)
        return (2.0 * phase / M_PI) - 1.0;
      else
        return 3.0 - (2.0 * phase / M_PI);
    default:
      return 0.0;
  }
}

} // namespace synth
