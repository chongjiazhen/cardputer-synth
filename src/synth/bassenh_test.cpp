// Host test for bassenh.h.
//   g++ -std=c++17 src/synth/bassenh_test.cpp -o t && ./t
#include "bassenh.h"
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace synth;

static void test_gain_ramp() {
  BassEnh b;                       // cutoff 250, amount 0.5
  assert(bassEnhGain(b, 300.0f) == 0.0f);   // above cutoff: no lift
  assert(bassEnhGain(b, 250.0f) == 0.0f);   // at cutoff: no lift
  float g100 = bassEnhGain(b, 100.0f);      // below: some lift
  float g60  = bassEnhGain(b, 60.0f);       // lower: more lift
  assert(g100 > 0.0f && g60 > g100);        // monotone: lower note -> more
  assert(g60 <= b.amount + 1e-6f);          // never exceeds amount
}

static void test_disabled_and_guard() {
  BassEnh b; b.on = false;
  assert(bassEnhGain(b, 60.0f) == 0.0f);    // off: nothing
  b.on = true;
  assert(bassEnhGain(b, 0.0f) == 0.0f);     // f0<=0 guard
}

static void test_apply_adds_only_when_gain() {
  // gain 0 => identity
  assert(bassEnhApply(0.7f, 1.234f, 0.0f) == 0.7f);
  // gain>0 => output differs (harmonics added) for most phases
  float out = bassEnhApply(0.0f, 0.9f, 0.5f);
  assert(std::fabs(out) > 1e-4f);
}

static void test_f0_from_inc() {
  double sr = 32000.0;
  float inc = TWO_PI_F * 65.41f / (float)sr;   // C2
  assert(std::fabs(bassEnhF0(inc, sr) - 65.41f) < 0.1f);
}

int main() {
  test_gain_ramp();
  test_disabled_and_guard();
  test_apply_adds_only_when_gain();
  test_f0_from_inc();
  printf("all bassenh tests passed\n");
  return 0;
}
