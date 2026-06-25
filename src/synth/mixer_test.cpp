// Host test for mixer.h:  g++ -std=c++17 mixer_test.cpp -o t && ./t
#include "mixer.h"
#include <cassert>
#include <cstdio>
#include <cmath>

static void check(bool c, const char* m) { if (!c) { fprintf(stderr, "FAIL: %s\n", m); assert(false); } }
static bool near(float a, float b, float t) { return std::fabs(a - b) <= t; }

int main() {
  using namespace synth;

  // softClip: 0→0, ±1→±2/3, saturates beyond
  check(near(softClip(0.0f), 0.0f, 1e-6f), "softClip 0");
  check(near(softClip(1.0f), 2.0f/3.0f, 1e-6f), "softClip 1");
  check(near(softClip(5.0f), 2.0f/3.0f, 1e-6f), "softClip saturates +");
  check(near(softClip(-5.0f), -2.0f/3.0f, 1e-6f), "softClip saturates -");

  // single voice at peak 1, gain 1 → ~amp (v0.1.0 level preserved)
  check(mixToInt16(1.0f, 1.0f, 14000) == 14000, "1 voice peak = amp");
  // six aligned voices don't exceed amp (soft saturation)
  check(mixToInt16(6.0f, 1.0f, 14000) == 14000, "6 voices clamp to amp");
  // silence → 0
  check(mixToInt16(0.0f, 1.0f, 14000) == 0, "silence = 0");
  // never exceeds int16
  check(mixToInt16(100.0f, 1.0f, 32767) <= 32767, "no int16 overflow");

  puts("ok");
  return 0;
}
