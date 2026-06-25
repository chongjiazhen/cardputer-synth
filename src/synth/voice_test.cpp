// Host test for voice.h:  g++ -std=c++17 voice_test.cpp -o t && ./t
#include "voice.h"
#include <cassert>
#include <cstdio>
#include <cmath>

static void check(bool c, const char* m) { if (!c) { fprintf(stderr, "FAIL: %s\n", m); assert(false); } }

int main() {
  using namespace synth;

  // inactive voice outputs silence and does not advance
  {
    Voice v;
    check(voiceSample(v, WaveShape::Sine, 1.0f, 1.0f) == 0.0f, "inactive silent");
    check(v.phase == 0.0f, "inactive no advance");
  }

  // active voice: phase advances by phaseInc * bend * vib
  {
    Voice v;
    v.active = true; v.vel = 1.0f; v.phase = 0.0f; v.phaseInc = 0.1f;
    v.env.configMs(16000, 1.0, 1.0, 1.0, 1000.0);  // long, ~full level
    v.env.gateOn();
    voiceSample(v, WaveShape::Saw, 1.0f, 1.0f);
    check(std::fabs(v.phase - 0.1f) < 1e-4f, "phase += inc");
    voiceSample(v, WaveShape::Saw, 2.0f, 1.0f);   // bend x2
    check(std::fabs(v.phase - 0.3f) < 1e-4f, "phase += inc*bend");
  }

  // gateOff then enough steps → env idle → voice goes inactive
  {
    Voice v;
    v.active = true; v.vel = 1.0f; v.phaseInc = 0.1f;
    v.env.configMs(16000, 1.0, 1.0, 1.0, 1.0);  // 1ms release
    v.env.gateOn();
    voiceSample(v, WaveShape::Sine, 1.0f, 1.0f);
    v.env.gateOff();
    for (int i = 0; i < 2000; i++) voiceSample(v, WaveShape::Sine, 1.0f, 1.0f);
    check(!v.active, "voice frees after release");
  }

  puts("ok");
  return 0;
}
