// Host test for oscillator.h — compile and run with:
//   g++ -std=c++17 oscillator_test.cpp -o t && ./t
// Prints "ok" on success, otherwise prints the failing assertion and exits 1.
#include "oscillator.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

using synth::WaveShape;
using synth::osc;
using synth::polyBlep;
using synth::shapeName;
using synth::shapeGain;
using synth::PI_F;
using synth::TWO_PI_F;

// Render N samples of `shape` at frequency f (Hz) and sample rate sr, then sum
// the DFT power in bins that are NOT harmonics of the fundamental — i.e. the
// aliased/inharmonic energy a non-band-limited oscillator folds back below
// Nyquist. `blep` selects PolyBLEP (dt>0) vs naive (dt=0). f is chosen to land
// exactly on a DFT bin (f = fund_bin * sr / N) so harmonics fall on integer bins.
static double aliasEnergy(WaveShape shape, double f, double sr, int N, bool blep) {
  std::vector<float> x(N);
  double phase = 0.0;
  double inc   = TWO_PI_F * f / sr;
  float  dt    = blep ? (float)(f / sr) : 0.0f;
  for (int n = 0; n < N; n++) {
    x[n] = osc(shape, (float)phase, dt);
    phase += inc;
    if (phase >= TWO_PI_F) phase -= TWO_PI_F;
  }
  int fund = (int)std::lround(f * N / sr);   // fundamental bin index
  double alias = 0.0;
  for (int k = 1; k < N / 2; k++) {
    if (k % fund == 0) continue;             // skip true harmonics
    double re = 0.0, im = 0.0;
    for (int n = 0; n < N; n++) {
      double a = 2.0 * M_PI * k * n / N;
      re += x[n] * std::cos(a);
      im -= x[n] * std::sin(a);
    }
    alias += re * re + im * im;
  }
  return alias;
}

static void check(bool cond, const char* msg) {
  if (!cond) {
    std::printf("FAIL: %s\n", msg);
    std::exit(1);
  }
}

static bool near(float a, float b, float eps = 1e-5f) {
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
  check(near(TWO_PI_F, 6.2831853f, 1e-4f), "TWO_PI_F");
  check(near(PI_F, 3.1415927f, 1e-4f), "PI_F");

  // --- shapeName ---
  check(std::string(shapeName(WaveShape::Sine))   == "SINE", "name Sine");
  check(std::string(shapeName(WaveShape::Saw))    == "SAW",  "name Saw");
  check(std::string(shapeName(WaveShape::Square)) == "SQR",  "name Square");
  check(std::string(shapeName(WaveShape::Tri))    == "TRI",  "name Tri");

  // --- PolyBLEP: dt==0 is byte-identical to the old naive path (back-compat) ---
  {
    for (float ph = 0.0f; ph < TWO_PI_F; ph += 0.013f) {
      check(osc(WaveShape::Saw, ph, 0.0f)    == osc(WaveShape::Saw, ph),    "saw dt=0 == naive");
      check(osc(WaveShape::Square, ph, 0.0f) == osc(WaveShape::Square, ph), "sqr dt=0 == naive");
    }
    // polyBlep is a no-op away from an edge and disabled at dt<=0.
    check(polyBlep(0.5f, 0.01f) == 0.0f, "blep zero mid-cycle");
    check(polyBlep(0.5f, 0.0f)  == 0.0f, "blep disabled at dt=0");
  }

  // --- PolyBLEP: band-limited saw/square stay bounded (no BLEP overshoot blowup) ---
  {
    const double sr = 32000.0, f = 3000.0;
    for (int n = 0; n < 512; n++) {
      float ph = std::fmod((float)(TWO_PI_F * f / sr * n), TWO_PI_F);
      float dt = (float)(f / sr);
      check(std::fabs(osc(WaveShape::Saw, ph, dt))    < 1.25f, "blep saw bounded");
      check(std::fabs(osc(WaveShape::Square, ph, dt)) < 1.25f, "blep sqr bounded");
    }
  }

  // --- PolyBLEP: cuts aliased/inharmonic energy vs the naive oscillator ---
  {
    const double sr = 32000.0, f = 3000.0;   // 3 kHz saw/sqr aliases hard @32k
    const int N = 320;                       // bin width 100 Hz → f on bin 30
    double sawNaive = aliasEnergy(WaveShape::Saw,    f, sr, N, false);
    double sawBlep  = aliasEnergy(WaveShape::Saw,    f, sr, N, true);
    double sqrNaive = aliasEnergy(WaveShape::Square, f, sr, N, false);
    double sqrBlep  = aliasEnergy(WaveShape::Square, f, sr, N, true);
    // PolyBLEP should remove a large majority of the fold-back energy.
    check(sawBlep < sawNaive * 0.5, "blep cuts saw alias >50%");
    check(sqrBlep < sqrNaive * 0.5, "blep cuts sqr alias >50%");
  }

  std::printf("ok\n");
  return 0;
}
