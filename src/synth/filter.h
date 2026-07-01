// Resonant State Variable Filter — TPT (topology-preserving transform) form.
// Pure math, no Arduino/M5 includes — host-testable.
//
// Andy Simper / Cytomic "trapezoidal SVF" (see cytomic.com technical papers;
// Vadim Zavalishin, The Art of VA Filter Design). Chosen over the RBJ cookbook
// biquad because it is FLOAT and needs only ONE transcendental (tanf) per
// coefficient update — the ESP32-S3 has a single-precision FPU and no hardware
// double, so double-precision cos/sin per sample starved the audio buffer.
//
// LP / HP / BP share the same integrator state (ic1, ic2). DC gain = 1.0 for LP,
// 0.0 for HP and BP. Q is the usual resonance: Q = 0.707 = Butterworth (flat),
// higher Q → resonant peak at cutoff, Q → ∞ → self-oscillation.
#pragma once
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif
#include <cmath>

namespace synth {

enum class FilterMode { LP, HP, BP };

struct SVFilter {
  // Integrator state (two delay lines)
  float ic1 = 0.0f;
  float ic2 = 0.0f;

  // Coefficients (set by calcCoeffs)
  float g  = 0.0f;   // prewarped integrator gain, tan(pi*fc/fs)
  float k  = 0.0f;   // damping = 1/Q
  float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
  FilterMode mode = FilterMode::LP;

  // Compute coefficients for a given mode, cutoff, Q, and sample rate.
  // Q = 0.707 = Butterworth; Q > 0.707 → resonant; Q ~10+ → near self-osc.
  void calcCoeffs(FilterMode m, float freq_hz, float q, float sample_rate) {
    if (freq_hz < 20.0f) freq_hz = 20.0f;
    float ny = sample_rate * 0.49f;
    if (freq_hz > ny) freq_hz = ny;
    if (q < 0.05f) q = 0.05f;               // guard k = 1/q

    g  = std::tan((float)M_PI * freq_hz / sample_rate);
    k  = 1.0f / q;                          // higher Q → smaller k → resonant
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
    mode = m;
  }

  // Process one sample (trapezoidal / TPT state update).
  float process(float input) {
    float v3 = input - ic2;
    float v1 = a1 * ic1 + a2 * v3;
    float v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    switch (mode) {
      case FilterMode::HP: return input - k * v1 - v2;
      case FilterMode::BP: return v1;
      default:             return v2;       // LP
    }
  }

  void reset() { ic1 = 0.0f; ic2 = 0.0f; }
};

}  // namespace synth
