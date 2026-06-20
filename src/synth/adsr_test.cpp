// Host test for adsr.h — g++ -std=c++17 adsr_test.cpp -o t && ./t  (prints "ok").
#include "adsr.h"
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace synth;

static bool near(double a, double b, double eps = 1e-6) {
  return std::fabs(a - b) < eps;
}

// Run n steps, return final level.
static double run(Adsr& e, int n) {
  double v = 0.0;
  for (int i = 0; i < n; i++) v = e.step();
  return v;
}

int main() {
  // --- Attack ramps 0 -> 1 over `attack` samples ---
  {
    Adsr e; e.attack = 4; e.decay = 4; e.sustain = 0.5; e.release = 4;
    e.gateOn();
    assert(e.active());
    assert(near(e.step(), 0.25));   // 1/4
    assert(near(e.step(), 0.50));
    assert(near(e.step(), 0.75));
    assert(near(e.step(), 1.0));    // hits 1.0 -> moves to Decay
    assert(e.stage == EnvStage::Decay);
  }

  // --- Decay ramps 1 -> sustain over `decay` samples, then holds ---
  {
    Adsr e; e.attack = 1; e.decay = 4; e.sustain = 0.5; e.release = 4;
    e.gateOn();
    e.step();                       // attack done (1 sample), level=1, ->Decay
    assert(e.stage == EnvStage::Decay);
    // decay decrement = (1-0.5)/4 = 0.125
    assert(near(e.step(), 0.875));
    assert(near(e.step(), 0.75));
    assert(near(e.step(), 0.625));
    assert(near(e.step(), 0.5));    // reaches sustain -> Sustain
    assert(e.stage == EnvStage::Sustain);
    // holds at sustain indefinitely while gated
    assert(near(run(e, 100), 0.5));
    assert(e.stage == EnvStage::Sustain);
  }

  // --- Release ramps from current level -> 0, then Idle ---
  {
    Adsr e; e.attack = 1; e.decay = 1; e.sustain = 0.5; e.release = 5;
    e.gateOn();
    run(e, 10);                     // settle to sustain 0.5
    assert(near(e.level, 0.5));
    e.gateOff();
    assert(e.stage == EnvStage::Release);
    // relStep = 0.5/5 = 0.1
    assert(near(e.step(), 0.4));
    assert(near(e.step(), 0.3));
    assert(near(e.step(), 0.2));
    assert(near(e.step(), 0.1));
    assert(near(e.step(), 0.0));
    assert(e.stage == EnvStage::Idle);
    assert(!e.active());
    // stays idle/silent
    assert(near(run(e, 50), 0.0));
  }

  // --- gateOff mid-attack releases from the partial level ---
  {
    Adsr e; e.attack = 10; e.decay = 4; e.sustain = 0.5; e.release = 2;
    e.gateOn();
    e.step(); e.step();             // level = 0.2, still Attack
    assert(near(e.level, 0.2));
    assert(e.stage == EnvStage::Attack);
    e.gateOff();                    // relStep = 0.2/2 = 0.1
    assert(near(e.step(), 0.1));
    assert(near(e.step(), 0.0));
    assert(e.stage == EnvStage::Idle);
  }

  // --- gateOff while idle is a no-op ---
  {
    Adsr e;
    e.gateOff();
    assert(e.stage == EnvStage::Idle);
    assert(!e.active());
  }

  // --- configMs converts ms to samples and clamps sustain ---
  {
    Adsr e;
    e.configMs(16000, 10.0, 20.0, 0.7, 100.0);
    assert(e.attack  == 160);       // 10ms @16k
    assert(e.decay   == 320);       // 20ms
    assert(e.release == 1600);      // 100ms
    assert(near(e.sustain, 0.7));
    // zero-length times clamp to >=1 (no div-by-zero)
    e.configMs(16000, 0.0, 0.0, 2.0, 0.0);
    assert(e.attack == 1 && e.decay == 1 && e.release == 1);
    assert(near(e.sustain, 1.0));   // clamped from 2.0
  }

  printf("ok\n");
  return 0;
}
