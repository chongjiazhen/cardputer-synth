// Sample playback math. Pure, no Arduino/M5 includes — host-testable.
// Reads a recorded int16 buffer at a fractional position with linear
// interpolation, returning a sample in -1..1. Hardware mic capture lives in
// main.cpp; only the read/resample math is here.
#pragma once
#include <cstdint>
#include <cmath>

namespace synth {

// Linear-interpolated read of buf[pos], pos in [0, len). Wraps for positions at
// or past the end so a held note loops the buffer seamlessly. len must be > 0.
inline double sampleRead(const int16_t* buf, int len, double pos) {
  if (len <= 0) return 0.0;
  // Wrap pos into [0, len).
  double p = std::fmod(pos, (double)len);
  if (p < 0) p += len;
  int   i0 = (int)p;
  int   i1 = i0 + 1; if (i1 >= len) i1 = 0;     // wrap the upper tap too
  double frac = p - i0;
  double a = buf[i0] / 32768.0;
  double b = buf[i1] / 32768.0;
  return a + (b - a) * frac;
}

// Playback step (buffer samples advanced per output sample) for a MIDI note,
// with C4 (60) = 1.0 (original speed/pitch). Each octave doubles the rate.
inline double sampleStep(int midi) {
  return std::pow(2.0, (midi - 60) / 12.0);
}

}  // namespace synth
