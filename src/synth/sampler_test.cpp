// Host test for sampler.h. Compile and run:
//   g++ -std=c++17 sampler_test.cpp -o t && ./t
#include "sampler.h"
#include <cassert>
#include <cstdio>
#include <cmath>

static void check(bool cond, const char* msg) {
  if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); assert(false); }
}
static bool near(double a, double b, double tol = 1e-9) { return std::fabs(a - b) <= tol; }

int main() {
  using namespace synth;

  // --- empty buffer → 0 ---
  check(sampleRead(nullptr, 0, 0.0) == 0.0, "empty → 0");

  int16_t buf[4] = {0, 16384, -16384, 32767};  // ~0, +0.5, -0.5, ~+1

  // --- integer positions read the stored value (/32768) ---
  check(near(sampleRead(buf, 4, 0.0), 0.0),            "read[0]");
  check(near(sampleRead(buf, 4, 1.0), 16384/32768.0),  "read[1] = 0.5");
  check(near(sampleRead(buf, 4, 2.0), -16384/32768.0), "read[2] = -0.5");

  // --- linear interpolation at the midpoint of [0]..[1] ---
  check(near(sampleRead(buf, 4, 0.5), 0.25), "interp 0..1 midpoint");

  // --- wrap: pos == len loops to [0]; pos past end wraps ---
  check(near(sampleRead(buf, 4, 4.0), 0.0),           "wrap len → [0]");
  check(near(sampleRead(buf, 4, 5.0), 16384/32768.0), "wrap len+1 → [1]");
  // --- last sample interpolates back toward [0] (seamless loop) ---
  check(near(sampleRead(buf, 4, 3.5), (32767/32768.0) * 0.5), "interp last→wrap");

  // --- step: C4 = 1.0, octave up = 2x, octave down = 0.5x ---
  check(near(sampleStep(60), 1.0),  "step C4 = 1.0");
  check(near(sampleStep(72), 2.0),  "step +octave = 2.0");
  check(near(sampleStep(48), 0.5),  "step -octave = 0.5");

  puts("ok");
  return 0;
}
