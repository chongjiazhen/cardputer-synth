// Low-frequency oscillator. Pure math, no Arduino/M5 includes — host-testable.
// Phase-accumulator LFO with sine, triangle, saw, and square waveforms.
// Runs at audio rate but controls sub-audio parameters (0.1–20 Hz typical).
#pragma once
#include <cmath>

namespace synth {

enum class LfoShape { Sine, Tri, Saw, Square };

struct Lfo {
  double phase    = 0.0;   // 0..2π
  double phaseInc = 0.0;   // radians per sample
  double depth    = 1.0;   // output scale (0..1)
  LfoShape shape  = LfoShape::Sine;

  // Set frequency in Hz at the given sample rate.
  void setFreq(double hz, double sample_rate) {
    if (hz < 0.0) hz = 0.0;
    phaseInc = TWO_PI_CONST * hz / sample_rate;
  }

  // Advance one sample and return the LFO value in -depth..+depth.
  // When depth is zero, skip the waveform evaluation but still advance
  // phase so the LFO stays in sync if depth is later raised mid-stream.
  double step() {
    if (depth == 0.0) {
      phase += phaseInc;
      if (phase >= TWO_PI_CONST) phase -= TWO_PI_CONST;
      return 0.0;
    }
    double val = 0.0;
    switch (shape) {
      case LfoShape::Sine:
        val = std::sin(phase);
        break;
      case LfoShape::Tri:
        // 0..π → -1..+1, π..2π → +1..-1
        val = (phase < M_PI_VAL) ? (2.0 * phase / M_PI_VAL - 1.0)
                                  : (3.0 - 2.0 * phase / M_PI_VAL);
        break;
      case LfoShape::Saw:
        val = phase / M_PI_VAL - 1.0;  // -1..+1 over 0..2π
        break;
      case LfoShape::Square:
        val = (phase < M_PI_VAL) ? 1.0 : -1.0;
        break;
    }
    phase += phaseInc;
    if (phase >= TWO_PI_CONST) phase -= TWO_PI_CONST;
    return val * depth;
  }

  void reset() { phase = 0.0; }

private:
  static constexpr double M_PI_VAL    = 3.14159265358979323846;
  static constexpr double TWO_PI_CONST = 6.28318530717958647692;
};

}  // namespace synth