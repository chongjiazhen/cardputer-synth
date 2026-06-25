// Host test for filter.h. Compile and run:
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
  using synth::OnePole;
  const double SR = 16000.0;

  // --- open (alpha=1) is passthrough ---
  {
    OnePole f;  // default alpha = 1
    check(near(f.process(0.5),  0.5,  1e-12), "open: pass 0.5");
    check(near(f.process(-0.3), -0.3, 1e-12), "open: pass -0.3");
  }

  // --- setCutoff at/above Nyquist → open ---
  {
    OnePole f; f.setCutoff(SR * 0.5, SR);
    check(f.alpha == 1.0, "cutoff >= Nyquist → open");
    f.setCutoff(SR, SR);
    check(f.alpha == 1.0, "cutoff above Nyquist → open");
  }

  // --- setCutoff <= 0 → closed ---
  {
    OnePole f; f.setCutoff(0.0, SR);
    check(f.alpha == 0.0, "cutoff 0 → closed");
  }

  // --- lower cutoff → smaller alpha (more filtering) ---
  {
    OnePole a, b; a.setCutoff(500.0, SR); b.setCutoff(2000.0, SR);
    check(a.alpha < b.alpha, "lower cutoff → smaller alpha");
    check(a.alpha > 0.0 && b.alpha < 1.0, "alpha in (0,1)");
  }

  // --- DC (constant input) passes through to the input value ---
  {
    OnePole f; f.setCutoff(1000.0, SR);
    double y = 0;
    for (int i = 0; i < 5000; i++) y = f.process(1.0);
    check(near(y, 1.0, 0.001), "DC settles to input");
  }

  // --- a fast ±1 alternating signal is heavily attenuated at low cutoff ---
  {
    OnePole f; f.setCutoff(200.0, SR);
    double last = 0;
    for (int i = 0; i < 20000; i++) last = f.process((i & 1) ? 1.0 : -1.0);
    check(std::fabs(last) < 0.1, "Nyquist-ish tone attenuated");
  }

  puts("ok");
  return 0;
}
