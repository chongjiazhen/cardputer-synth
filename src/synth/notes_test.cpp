// Host test for notes.h — g++ -std=c++17 notes_test.cpp -o t && ./t  (prints "ok").
#include "notes.h"
#include <cassert>
#include <cmath>
#include <cstdio>

using namespace synth;

static bool near(double a, double b, double eps = 0.01) {
  return std::fabs(a - b) < eps;
}

int main() {
  // A4 = 440 exactly.
  assert(near(midiToHz(69), 440.0));
  // Middle C (MIDI 60) ~= 261.63 Hz.
  assert(near(midiToHz(60), 261.63, 0.02));
  // An octave up doubles Hz.
  assert(near(midiToHz(81), 2.0 * midiToHz(69)));   // A5 = 2*A4
  assert(near(midiToHz(72), 2.0 * midiToHz(60)));   // C5 = 2*C4

  // keyToSemitone: white keys.
  assert(keyToSemitone('z') == 0);    // C
  assert(keyToSemitone('m') == 11);   // B
  assert(keyToSemitone('b') == 7);    // G
  // black keys.
  assert(keyToSemitone('s') == 1);    // C#
  assert(keyToSemitone('j') == 10);   // A#
  // non-note keys -> -1.
  assert(keyToSemitone('q') == -1);
  assert(keyToSemitone(' ') == -1);
  assert(keyToSemitone('1') == -1);

  // noteToMidi: C in octave 4 = MIDI 60.
  assert(noteToMidi(0, 4) == 60);
  assert(noteToMidi(9, 4) == 69);     // A4
  assert(noteToMidi(0, 5) == 72);     // C5

  // round-trip: playing 'z' in octave 4 yields middle C.
  assert(near(midiToHz(noteToMidi(keyToSemitone('z'), 4)), 261.63, 0.02));

  printf("ok\n");
  return 0;
}
