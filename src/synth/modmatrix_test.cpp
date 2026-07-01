// Host test for modmatrix.h. Compile and run:
//   g++ -std=c++17 modmatrix_test.cpp -o t && ./t
#include "modmatrix.h"
#include <cassert>
#include <cstdio>
#include <cmath>

static void check(bool cond, const char* msg) {
  if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); assert(false); }
}
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
  using synth::ModMatrix;
  using synth::ModSrc;
  using synth::ModDst;

  // --- Test 1: No routing → all destinations zero ---
  {
    ModMatrix m;
    float src[synth::MOD_SRC_COUNT] = {};
    src[(int)ModSrc::Env1] = 1.0f;
    m.process(src);
    for (int d = 0; d < synth::MOD_DST_COUNT; d++)
      check(near(m.dst[d], 0.0f, 1e-6f), "No routing → zero offset");
  }

  // --- Test 2: Single routing: Env1 → Amp at depth 0.5 ---
  {
    ModMatrix m;
    m.set(ModSrc::Env1, ModDst::Amp, 0.5f);
    float src[synth::MOD_SRC_COUNT] = {};
    src[(int)ModSrc::Env1] = 0.8f;
    m.process(src);
    check(near(m.get(ModDst::Amp), 0.4f, 1e-5f), "Env1→Amp depth=0.5 value=0.8 → 0.4");
  }

  // --- Test 3: Multiple sources → same destination (additive) ---
  {
    ModMatrix m;
    m.set(ModSrc::Lfo1, ModDst::Pitch, 0.3f);
    m.set(ModSrc::Lfo2, ModDst::Pitch, 0.2f);
    float src[synth::MOD_SRC_COUNT] = {};
    src[(int)ModSrc::Lfo1] = 1.0f;
    src[(int)ModSrc::Lfo2] = 1.0f;
    m.process(src);
    check(near(m.get(ModDst::Pitch), 0.5f, 1e-5f),
          "Two sources additive → 0.5");
  }

  // --- Test 4: Negative depth inverts source ---
  {
    ModMatrix m;
    m.set(ModSrc::Velocity, ModDst::FilterCut, -1.0f);
    float src[synth::MOD_SRC_COUNT] = {};
    src[(int)ModSrc::Velocity] = 0.5f;
    m.process(src);
    check(near(m.get(ModDst::FilterCut), -0.5f, 1e-5f),
          "Negative depth inverts");
  }

  // --- Test 5: One source → multiple destinations ---
  {
    ModMatrix m;
    m.set(ModSrc::Env2, ModDst::FilterCut, 1.0f);
    m.set(ModSrc::Env2, ModDst::FilterRes, 0.5f);
    float src[synth::MOD_SRC_COUNT] = {};
    src[(int)ModSrc::Env2] = 0.6f;
    m.process(src);
    check(near(m.get(ModDst::FilterCut), 0.6f, 1e-5f),
          "Env2→FilterCut = 0.6");
    check(near(m.get(ModDst::FilterRes), 0.3f, 1e-5f),
          "Env2→FilterRes = 0.3");
  }

  // --- Test 6: Clear removes all routings ---
  {
    ModMatrix m;
    m.set(ModSrc::Env1, ModDst::Amp, 1.0f);
    m.clear();
    float src[synth::MOD_SRC_COUNT] = {};
    src[(int)ModSrc::Env1] = 1.0f;
    m.process(src);
    check(near(m.get(ModDst::Amp), 0.0f, 1e-6f), "Clear removes routing");
  }

  // --- Test 7: Zero source values → zero output (even with routing) ---
  {
    ModMatrix m;
    m.set(ModSrc::Lfo1, ModDst::Pitch, 1.0f);
    float src[synth::MOD_SRC_COUNT] = {};  // All zeros
    m.process(src);
    check(near(m.get(ModDst::Pitch), 0.0f, 1e-6f),
          "Zero sources → zero output");
  }

  puts("ok");
  return 0;
}