// Host test for sequencer.h. Build + run:
//   g++ -std=c++17 src/synth/sequencer_test.cpp -o t && ./t
#include "sequencer.h"
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace synth;

static void test_samples_per_step() {
  Sequencer s;                       // 120bpm, 16ths, 44100Hz
  // 120bpm = 2 beats/s; *4 steps/beat = 8 steps/s; 44100/8 = 5512.5
  assert(std::fabs(seqSamplesPerStep(s) - 5512.5) < 0.01);
}

static void test_advance_crosses_on_time() {
  Sequencer s;
  s.length = 4;
  seqStart(s);
  double sps = seqSamplesPerStep(s);

  // A block just short of one step => no boundary yet.
  assert(seqAdvance(s, int(sps) - 10) == -1);
  assert(s.cur == 0);

  // Next block pushes past the boundary => step 1.
  assert(seqAdvance(s, 20) == 1);
  assert(s.cur == 1);
}

static void test_wraps_around_length() {
  Sequencer s;
  s.length = 2;
  seqStart(s);
  int sps = int(seqSamplesPerStep(s)) + 1;
  assert(seqAdvance(s, sps) == 1);
  assert(seqAdvance(s, sps) == 0);   // wrapped
  assert(seqAdvance(s, sps) == 1);
}

static void test_stopped_does_not_advance() {
  Sequencer s;
  assert(seqAdvance(s, 100000) == -1);  // running=false by default
  assert(s.cur == 0);
}

static void test_toggle_gate() {
  Sequencer s;
  assert(!s.tracks[0].steps[3].gate);
  seqToggleGate(s, 0, 3);
  assert(s.tracks[0].steps[3].gate);
  seqToggleGate(s, 0, 3);
  assert(!s.tracks[0].steps[3].gate);
  seqToggleGate(s, 99, 0);  // out of range: no-op, no crash
}

int main() {
  test_samples_per_step();
  test_advance_crosses_on_time();
  test_wraps_around_length();
  test_stopped_does_not_advance();
  test_toggle_gate();
  printf("all sequencer tests passed\n");
  return 0;
}
