// Master mix + soft limiter. Pure, no Arduino/M5 includes — host-testable.
#pragma once
#include <cstdint>

namespace synth {

// Cubic soft clip: f(x) = x - x^3/3 for |x|<=1 (saturates to +/-2/3 beyond).
inline float softClip(float x) {
  if (x >=  1.0f) return  2.0f / 3.0f;
  if (x <= -1.0f) return -2.0f / 3.0f;
  return x - (x * x * x) / 3.0f;
}

// Sum the master bus to int16 at output amplitude `amp`, after applying
// masterGain and the soft limiter. The x1.5 rescale makes a single voice at
// peak 1.0 map to `amp` (preserving v0.1.0 loudness); summed voices saturate.
inline int16_t mixToInt16(float busSum, float masterGain, int amp) {
  float s = softClip(busSum * masterGain) * 1.5f;
  float y = s * (float)amp;
  if (y >  32767.0f) y =  32767.0f;
  if (y < -32768.0f) y = -32768.0f;
  return (int16_t)y;
}

}  // namespace synth
