// Host test for sampler.h. Compile and run:
//   g++ -std=c++17 sampler_test.cpp -o t && ./t
#include "sampler.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <vector>

static void check(bool cond, const char* msg) {
  if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); assert(false); }
}
static bool near(double a, double b, double tol = 1e-9) { return std::fabs(a - b) <= tol; }

// Dominant DFT bin (1..N/2) of a real signal — its frequency = bin/N cyc/sample.
static int peakBin(const std::vector<float>& x) {
  int N = (int)x.size(), best = 1; double bestMag = -1;
  for (int k = 1; k < N / 2; k++) {
    double re = 0, im = 0;
    for (int n = 0; n < N; n++) { double a = 2 * M_PI * k * n / N; re += x[n]*std::cos(a); im -= x[n]*std::sin(a); }
    double m = re*re + im*im;
    if (m > bestMag) { bestMag = m; best = k; }
  }
  return best;
}
static double rms(const std::vector<float>& x) {
  double s = 0; for (float v : x) s += (double)v * v; return std::sqrt(s / x.size());
}

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

  // --- Granular pitch-shift: pitch tracks pitchRatio, independent of rate ---
  {
    const int LEN = 4096, CYC = 32;            // buffer holds CYC whole cycles
    int16_t sbuf[LEN];
    for (int i = 0; i < LEN; i++)
      sbuf[i] = (int16_t)(16000.0 * std::sin(2.0 * M_PI * CYC * i / LEN));

    auto render = [&](float rate, float pitch) {
      Grain g; g.reset(0.0f);
      for (int i = 0; i < GRAIN_LEN; i++) g.process(sbuf, LEN, rate, pitch);  // warmup
      std::vector<float> out(2048);
      for (auto& s : out) s = g.process(sbuf, LEN, rate, pitch);
      return out;
    };

    // At pitch=1 the dominant freq matches the source's own frequency.
    int k1 = peakBin(render(1.0f, 1.0f));
    // At pitch=2 (octave up) the dominant freq doubles...
    int k2 = peakBin(render(1.0f, 2.0f));
    check(std::abs(k2 - 2 * k1) <= 2, "grain: pitch=2 doubles frequency");
    // ...and pitch=0.5 halves it.
    int kh = peakBin(render(1.0f, 0.5f));
    check(std::abs(2 * kh - k1) <= 2, "grain: pitch=0.5 halves frequency");

    // Length/time-base is set by rate, not pitch: pitch alone must NOT change
    // the dominant bin when we also stretch time to compensate. Simpler check:
    // rate has no effect on pitch — pitch=2 at half rate still reads as doubled.
    int k2slow = peakBin(render(0.5f, 2.0f));
    check(std::abs(k2slow - k2) <= 2, "grain: pitch independent of rate");

    // Overlap-add is ~unity gain (no ripple doubling/halving the amplitude).
    double src = rms({sbuf, sbuf + 2048}) / 32768.0;  // approx source RMS (norm)
    double got = rms(render(1.0f, 1.0f));
    check(got > src * 0.6 && got < src * 1.4, "grain: ~unity gain");
  }

  puts("ok");
  return 0;
}
