// One-pole low-pass filter. Pure math, no Arduino/M5 includes — host-testable.
//   y[n] = y[n-1] + alpha * (x[n] - y[n-1])
// alpha in (0,1]: 1.0 = open (passthrough), smaller = heavier low-pass.
#pragma once
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif
#include <cmath>

namespace synth {

struct OnePole {
  double y     = 0.0;   // filter state (last output)
  double alpha = 1.0;   // 1.0 = open / bypass

  // Set the coefficient directly (clamped to [0,1]).
  void setAlpha(double a) { alpha = a < 0.0 ? 0.0 : (a > 1.0 ? 1.0 : a); }

  // Map a cutoff frequency (Hz) to alpha for the given sample rate.
  // fc >= Nyquist → fully open (1.0); fc <= 0 → fully closed (0.0).
  void setCutoff(double fc, double sr) {
    if (fc >= sr * 0.5) { alpha = 1.0; return; }
    if (fc <= 0.0)      { alpha = 0.0; return; }
    double a = 1.0 - std::exp(-2.0 * M_PI * fc / sr);
    alpha = a > 1.0 ? 1.0 : a;
  }

  double process(double x) { y += alpha * (x - y); return y; }

  void reset() { y = 0.0; }
};

}  // namespace synth
