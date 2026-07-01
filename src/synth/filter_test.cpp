// Host test for filter.h (biquad SVF). Compile and run:
//   g++ -std=c++17 filter_test.cpp -o t && ./t
#include "filter.h"
#include <cassert>
#include <cstdio>
#include <cmath>

static void check(bool cond, const char* msg) {
  if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); assert(false); }
}
static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

int main() {
  using synth::SVFilter;
  using synth::FilterMode;
  const double SR = 16000.0;

  // --- Test 1: LPF DC gain = 1.0 ---
  {
    SVFilter f;
    f.calcCoeffs(FilterMode::LP, 1000.0, 0.707, SR);

    double out = 0;
    for (int i = 0; i < 5000; i++) out = f.process(1.0);
    check(near(out, 1.0, 0.01), "LP DC gain = 1.0");
  }

  // --- Test 2: HPF DC gain = 0.0 ---
  {
    SVFilter f;
    f.calcCoeffs(FilterMode::HP, 1000.0, 0.707, SR);

    double out = 0;
    for (int i = 0; i < 5000; i++) out = f.process(1.0);
    check(near(out, 0.0, 0.01), "HP DC gain = 0.0");
  }

  // --- Test 3: BPF DC gain = 0.0 ---
  {
    SVFilter f;
    f.calcCoeffs(FilterMode::BP, 1000.0, 0.707, SR);

    double out = 0;
    for (int i = 0; i < 5000; i++) out = f.process(1.0);
    check(near(out, 0.0, 0.01), "BP DC gain = 0.0");
  }

  // --- Test 4: LPF attenuates high frequency ---
  {
    SVFilter f;
    f.calcCoeffs(FilterMode::LP, 500.0, 0.707, SR);
    f.reset();

    double out = 0;
    for (int i = 0; i < 500; i++) {
      double sig = (i & 1) ? 1.0 : -1.0;  // Nyquist signal
      out = f.process(sig);
    }
    check(std::fabs(out) < 0.1, "LPF attenuates Nyquist");
  }

  // --- Test 5: HPF passes high frequency ---
  {
    SVFilter f;
    f.calcCoeffs(FilterMode::HP, 500.0, 0.707, SR);
    f.reset();

    double peak = 0;
    for (int i = 0; i < 500; i++) {
      double sig = (i & 1) ? 1.0 : -1.0;  // Nyquist signal
      double out = f.process(sig);
      if (std::fabs(out) > peak) peak = std::fabs(out);
    }
    check(peak > 0.5, "HPF passes high frequency");
  }

  // --- Test 6: Resonance — higher Q gives bigger peak at cutoff ---
  {
    // Impulse-test: send a single 1.0, measure max output.
    // Higher Q → longer ring → larger cumulative energy.
    double lo_q_max = 0, hi_q_max = 0;

    SVFilter f_lo;
    f_lo.calcCoeffs(FilterMode::LP, 1000.0, 0.707, SR);
    f_lo.reset();
    f_lo.process(1.0);  // impulse
    for (int i = 0; i < 200; i++) {
      double v = std::fabs(f_lo.process(0.0));
      if (v > lo_q_max) lo_q_max = v;
    }

    SVFilter f_hi;
    f_hi.calcCoeffs(FilterMode::LP, 1000.0, 5.0, SR);  // Q=5 resonant
    f_hi.reset();
    f_hi.process(1.0);  // impulse
    for (int i = 0; i < 200; i++) {
      double v = std::fabs(f_hi.process(0.0));
      if (v > hi_q_max) hi_q_max = v;
    }

    check(hi_q_max > lo_q_max * 1.5,
          "High Q resonance > low Q peak");
  }

  // --- Test 7: Reset clears state ---
  {
    SVFilter f;
    f.calcCoeffs(FilterMode::LP, 1000.0, 0.707, SR);
    f.process(1.0);
    f.process(1.0);
    f.reset();
    // After reset, first sample on impulse should equal b0
    double out = f.process(1.0);
    check(near(out, f.b0, 1e-10), "Reset clears state");
  }

  // --- Test 8: cutoff frequency scaling ---
  {
    SVFilter f1, f2;
    f1.calcCoeffs(FilterMode::LP, 200.0, 0.707, SR);
    f2.calcCoeffs(FilterMode::LP, 2000.0, 0.707, SR);
    // Higher cutoff → more high-frequency energy passes.
    // Compare peak amplitude with Nyquist input (both oscillate near zero
    // at steady state, so measure peak).
    double lo_peak = 0, hi_peak = 0;
    for (int i = 0; i < 200; i++) {
      double sig = (i & 1) ? 1.0 : -1.0;
      double lo = std::fabs(f1.process(sig));
      double hi = std::fabs(f2.process(sig));
      if (lo > lo_peak) lo_peak = lo;
      if (hi > hi_peak) hi_peak = hi;
    }
    check(hi_peak > lo_peak, "Higher cutoff passes more HF");
  }

  puts("ok");
  return 0;
}