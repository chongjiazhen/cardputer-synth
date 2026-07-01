// Resonant State Variable Filter — biquad implementation.
// Pure math, no Arduino/M5 includes — host-testable.
//
// Uses the Audio EQ Cookbook (RBJ) direct form II transposed biquad.
// Provides LP / HP / BP via coefficient rotation — all outputs share the
// same state so only one filter object is needed per voice.
//
// DC gain = 1.0 for LP, 0.0 for HP and BP (verified analytically).
// Resonance peaks at cutoff; Q > 0.707 gives emphasis, Q → ∞ → self-osc.
#pragma once
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif
#include <cmath>

namespace synth {

enum class FilterMode { LP, HP, BP };

struct SVFilter {
  // State (direct form II transposed — two delay lines)
  double z1 = 0.0;
  double z2 = 0.0;

  // Coefficients (set by calcCoeffs)
  double b0 = 0.0, b1 = 0.0, b2 = 0.0;
  double a1 = 0.0, a2 = 0.0;  // normalized so a0 = 1

  // Compute coefficients for a given mode, cutoff, Q, and sample rate.
  // Q = 0.707 = Butterworth (flat passband, no resonance peak).
  // Q > 0.707 → resonant peak at cutoff; Q ~10+ → near self-oscillation.
  void calcCoeffs(FilterMode mode, double freq_hz, double q, double sample_rate) {
    double w0 = 2.0 * M_PI * freq_hz / sample_rate;
    double cos_w0 = std::cos(w0);
    double sin_w0 = std::sin(w0);
    double alpha = sin_w0 / (2.0 * q);

    switch (mode) {
      case FilterMode::LP: {
        double norm = 1.0 + alpha;
        b0 =  (1.0 - cos_w0) / 2.0 / norm;
        b1 =  (1.0 - cos_w0)        / norm;
        b2 =  (1.0 - cos_w0) / 2.0 / norm;
        a1 = -2.0 * cos_w0          / norm;
        a2 =  (1.0 - alpha)          / norm;
        break;
      }
      case FilterMode::HP: {
        double norm = 1.0 + alpha;
        b0 =  (1.0 + cos_w0) / 2.0 / norm;
        b1 = -(1.0 + cos_w0)        / norm;
        b2 =  (1.0 + cos_w0) / 2.0 / norm;
        a1 = -2.0 * cos_w0          / norm;
        a2 =  (1.0 - alpha)          / norm;
        break;
      }
      case FilterMode::BP: {
        double norm = 1.0 + alpha;
        b0 =  alpha             / norm;
        b1 =  0.0;
        b2 = -alpha             / norm;
        a1 = -2.0 * cos_w0      / norm;
        a2 =  (1.0 - alpha)     / norm;
        break;
      }
    }
  }

  // Process one sample through the biquad (direct form II transposed).
  double process(double input) {
    double out = b0 * input + z1;
    z1 = b1 * input - a1 * out + z2;
    z2 = b2 * input - a2 * out;
    return out;
  }

  void reset() { z1 = 0.0; z2 = 0.0; }
};

}  // namespace synth