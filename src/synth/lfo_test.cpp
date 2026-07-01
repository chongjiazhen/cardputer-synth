// Host test for lfo.h. Compile and run:
//   g++ -std=c++17 lfo_test.cpp -o t && ./t
#include "lfo.h"
#include <cassert>
#include <cstdio>
#include <cmath>

static void check(bool cond, const char* msg) {
  if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); assert(false); }
}
static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

int main() {
  using synth::Lfo;
  using synth::LfoShape;
  const double SR = 16000.0;

  // --- Test 1: Sine LFO at 1Hz ---
  {
    Lfo lfo;
    lfo.setFreq(1.0, SR);
    lfo.shape = LfoShape::Sine;
    lfo.depth = 1.0;

    // After 1/4 period (SR/4 samples), sine should peak at +1
    double val = 0;
    for (int i = 0; i < (int)(SR / 4); i++) val = lfo.step();
    check(near(val, 1.0, 0.05), "Sine LFO peaks at quarter period");
  }

  // --- Test 2: Triangle LFO symmetry ---
  {
    Lfo lfo;
    lfo.setFreq(1.0, SR);
    lfo.shape = LfoShape::Tri;
    lfo.depth = 1.0;

    // At phase=π (half period), triangle should peak at +1
    lfo.reset();
    double val = 0;
    for (int i = 0; i < (int)(SR / 2); i++) val = lfo.step();
    check(near(val, 1.0, 0.05), "Tri LFO peaks at half period");
  }

  // --- Test 3: Square LFO alternates ---
  {
    Lfo lfo;
    lfo.setFreq(1.0, SR);
    lfo.shape = LfoShape::Square;
    lfo.depth = 1.0;

    // First half should be +1, second half -1
    double first = lfo.step();
    check(near(first, 1.0, 0.01), "Square LFO starts at +1");
    lfo.reset();
    for (int i = 0; i < (int)(SR / 2); i++) lfo.step();
    double second = lfo.step();
    check(near(second, -1.0, 0.01), "Square LFO goes to -1 at half period");
  }

  // --- Test 4: Saw LFO ramp ---
  {
    Lfo lfo;
    lfo.setFreq(1.0, SR);
    lfo.shape = LfoShape::Saw;
    lfo.depth = 1.0;

    // First sample: phase ≈ 0 → saw ≈ -1
    double first = lfo.step();
    check(first < -0.9, "Saw LFO starts near -1");
  }

  // --- Test 5: Depth scaling ---
  {
    Lfo lfo;
    lfo.setFreq(1.0, SR);
    lfo.shape = LfoShape::Sine;
    lfo.depth = 0.5;

    double peak = 0;
    for (int i = 0; i < (int)(SR / 4); i++) {
      double v = std::fabs(lfo.step());
      if (v > peak) peak = v;
    }
    check(peak <= 0.55, "Depth=0.5 limits output to 0.5");
  }

  // --- Test 6: Reset ---
  {
    Lfo lfo;
    lfo.setFreq(10.0, SR);
    lfo.shape = LfoShape::Sine;

    for (int i = 0; i < 1000; i++) lfo.step();
    lfo.reset();

    // After reset, first sample should be sin(0) = 0
    double val = lfo.step();
    check(near(val, 0.0, 0.01), "Reset returns to start of cycle");
  }

  // --- Test 7: Frequency setting ---
  {
    Lfo lfo;
    lfo.setFreq(0.0, SR);
    check(near(lfo.phaseInc, 0.0, 1e-10), "0 Hz → no phase advance");

    lfo.setFreq(10.0, SR);
    double inc10 = lfo.phaseInc;

    lfo.setFreq(20.0, SR);
    double inc20 = lfo.phaseInc;

    check(near(inc20, 2.0 * inc10, 1e-6), "20Hz has 2x phase increment of 10Hz");
  }

  puts("ok");
  return 0;
}