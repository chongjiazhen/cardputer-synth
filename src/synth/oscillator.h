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

// Per-waveform gain so the four shapes sound about equally loud. At equal peak
// amplitude, perceived loudness tracks RMS: square 1.0, sine 1/√2≈0.707,
// saw/tri 1/√3≈0.577. We normalize to the quietest (saw/tri) so no shape needs
// boosting past ±1 (which would clip): gain = 0.5774 / shapeRMS. Net effect —
// square and sine come down to match saw/tri; saw/tri stay at full headroom.
inline double shapeGain(WaveShape shape) {
  switch (shape) {
    case WaveShape::Sine:   return 0.8165;  // 0.5774 / 0.7071
    case WaveShape::Square: return 0.5774;  // 0.5774 / 1.0
    case WaveShape::Saw:    return 1.0;
    case WaveShape::Tri:    return 1.0;
    default:                return 1.0;
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
