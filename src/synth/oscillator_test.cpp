// Host test for oscillator.h — compile and run with:
//   g++ -std=c++17 oscillator_test.cpp -o t && ./t
// Prints "ok" on success, otherwise prints the failing assertion and exits 1.
#include "oscillator.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>

using synth::WaveShape;
using synth::osc;
using synth::shapeName;
using synth::shapeGain;
using synth::PI_F;
using synth::TWO_PI_F;

static void check(bool cond, const char* msg) {
  if (!cond) {
    std::printf("FAIL: %s\n", msg);
    std::exit(1);
  }
}

static bool near(double a, double b, double eps = 1e-9) {
  return std::fabs(a - b) < eps;
}

int main() {
  // --- Sine ---
  check(near(osc(WaveShape::Sine, 0.0),       0.0), "sine(0) == 0");
  check(near(osc(WaveShape::Sine, PI_F / 2),  1.0), "sine(pi/2) == 1");

  // --- Saw ---
  check(near(osc(WaveShape::Saw, 0.0),   -1.0), "saw(0) == -1");
  check(near(osc(WaveShape::Saw, PI_F),   0.0), "saw(pi) == 0 (midpoint)");

  // --- Square ---
  check(osc(WaveShape::Square, PI_F / 2)    == 1.0,  "square(pi/2) == 1");
  check(osc(WaveShape::Square, 3 * PI_F / 2) == -1.0, "square(3pi/2) == -1");

  // --- Tri ---
  check(near(osc(WaveShape::Tri, 0.0),      -1.0), "tri(0) == -1");
  check(near(osc(WaveShape::Tri, PI_F / 2),  0.0), "tri(pi/2) == 0");
  check(near(osc(WaveShape::Tri, PI_F),       1.0), "tri(pi) == 1");

  // --- shapeGain: perceptual loudness table sanity ---
  {
    // All in (0,1] → no clipping; bright shapes trimmed below the pure shapes.
    for (auto s : {WaveShape::Sine, WaveShape::Saw, WaveShape::Square, WaveShape::Tri})
      check(shapeGain(s) > 0.0 && shapeGain(s) <= 1.0, "gain: in (0,1]");
    check(shapeGain(WaveShape::Saw)    < shapeGain(WaveShape::Sine), "gain: saw < sine");
    check(shapeGain(WaveShape::Square) < shapeGain(WaveShape::Sine), "gain: square < sine");
    check(shapeGain(WaveShape::Square) < shapeGain(WaveShape::Saw),  "gain: square < saw");
    check(shapeGain(WaveShape::Tri) == 1.0, "gain: tri anchors top");
  }

  // --- float constants present ---
  check(near(TWO_PI_F, 6.2831853f, 1e-4), "TWO_PI_F");
  check(near(PI_F, 3.1415927f, 1e-4), "PI_F");

  // --- shapeName ---
  check(std::string(shapeName(WaveShape::Sine))   == "SINE", "name Sine");
  check(std::string(shapeName(WaveShape::Saw))    == "SAW",  "name Saw");
  check(std::string(shapeName(WaveShape::Square)) == "SQR",  "name Square");
  check(std::string(shapeName(WaveShape::Tri))    == "TRI",  "name Tri");

  std::printf("ok\n");
  return 0;
}
