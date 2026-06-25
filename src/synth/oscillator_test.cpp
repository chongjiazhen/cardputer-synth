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
  check(near(osc(WaveShape::Sine, M_PI / 2),  1.0), "sine(pi/2) == 1");

  // --- Saw ---
  check(near(osc(WaveShape::Saw, 0.0),   -1.0), "saw(0) == -1");
  check(near(osc(WaveShape::Saw, M_PI),   0.0), "saw(pi) == 0 (midpoint)");

  // --- Square ---
  check(osc(WaveShape::Square, M_PI / 2)    == 1.0,  "square(pi/2) == 1");
  check(osc(WaveShape::Square, 3 * M_PI / 2) == -1.0, "square(3pi/2) == -1");

  // --- Tri ---
  check(near(osc(WaveShape::Tri, 0.0),      -1.0), "tri(0) == -1");
  check(near(osc(WaveShape::Tri, M_PI / 2),  0.0), "tri(pi/2) == 0");
  check(near(osc(WaveShape::Tri, M_PI),       1.0), "tri(pi) == 1");

  // --- shapeGain equalizes RMS across shapes (within ~2%) ---
  {
    auto rms = [](WaveShape s) {
      double sum = 0; const int N = 4096;
      for (int i = 0; i < N; i++) {
        double v = osc(s, 2.0 * M_PI * i / N) * shapeGain(s);
        sum += v * v;
      }
      return std::sqrt(sum / N);
    };
    double base = rms(WaveShape::Saw);
    check(near(rms(WaveShape::Sine),   base, 0.02 * base), "gain: sine RMS ~= saw");
    check(near(rms(WaveShape::Square), base, 0.02 * base), "gain: square RMS ~= saw");
    check(near(rms(WaveShape::Tri),    base, 0.02 * base), "gain: tri RMS ~= saw");
    check(shapeGain(WaveShape::Saw) == 1.0 && shapeGain(WaveShape::Tri) == 1.0,
          "gain: saw/tri unboosted");
  }

  // --- shapeName ---
  check(std::string(shapeName(WaveShape::Sine))   == "SINE", "name Sine");
  check(std::string(shapeName(WaveShape::Saw))    == "SAW",  "name Saw");
  check(std::string(shapeName(WaveShape::Square)) == "SQR",  "name Square");
  check(std::string(shapeName(WaveShape::Tri))    == "TRI",  "name Tri");

  std::printf("ok\n");
  return 0;
}
