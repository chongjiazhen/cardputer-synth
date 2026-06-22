// Pure note-math for the synth app. NO Arduino/M5 includes — host-testable with
// plain g++ (see notes_test.cpp). Keep it that way: parsers/timing/math live here,
// hardware glue lives in main.cpp.
#pragma once
#include <cmath>

namespace synth {

// Tracker/DAW keyboard layout mapped onto the Cardputer's QWERTY rows.
// Bottom letter row = white keys (one octave, C..B):
//   z  x  c  v  b  n  m
//   C  D  E  F  G  A  B   -> semitone 0 2 4 5 7 9 11
// Row above = black keys (sharps), aligned over the gaps between whites:
//   s  d  .  g  h  j      (no black between E/F and B/C, so f/k positions skipped)
//   C# D#    F# G# A#      -> semitone 1 3    6 8 10
// Returns the semitone offset from C (0..11), or -1 for any non-note key.
inline int keyToSemitone(char c) {
  switch (c) {
    // white keys
    case 'z': return 0;   // C
    case 'x': return 2;   // D
    case 'c': return 4;   // E
    case 'v': return 5;   // F
    case 'b': return 7;   // G
    case 'n': return 9;   // A
    case 'm': return 11;  // B
    // black keys
    case 's': return 1;   // C#
    case 'd': return 3;   // D#
    case 'g': return 6;   // F#
    case 'h': return 8;   // G#
    case 'j': return 10;  // A#
    default:  return -1;  // not a note
  }
}

// Equal temperament: A4 (MIDI 69) = 440 Hz. f = 440 * 2^((midi-69)/12).
inline double midiToHz(int midi) {
  return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
}

// Combine base octave + semitone-from-C into a MIDI number.
// MIDI octave convention: C in octave 4 = MIDI 60, so C0 = 12.
inline int noteToMidi(int semitoneFromC, int octave) {
  return (octave + 1) * 12 + semitoneFromC;
}

// Note name for a semitone (0..11). Sharps only. Returns "?" out of range.
inline const char* semitoneName(int s) {
  static const char* names[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  return (s >= 0 && s < 12) ? names[s] : "?";
}

}  // namespace synth
