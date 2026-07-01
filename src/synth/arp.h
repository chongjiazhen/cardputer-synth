// Arpeggiator pattern engine. Pure math/logic, no Arduino/M5 includes —
// host-testable. Given a sorted list of N held notes, arpStep() returns which
// index to play next and advances the internal state. Note lifecycle (voice
// allocation, MIDI, timing/clock) is hardware glue in main.cpp; this header
// only decides "which note index is next."
#pragma once
#include <cstdlib>

namespace synth {

enum class ArpMode { Up, Down, UpDown, Random };

struct ArpState {
  ArpMode mode  = ArpMode::Up;
  size_t  index = 0;
  bool    dirUp = true;   // UpDown: current sweep direction
};

// Advance the arp state and return the index (0..n-1) of the next note to
// play. Caller must ensure n >= 1 (empty held-set means "don't call").
inline size_t arpStep(ArpState& s, size_t n) {
  if (n == 0) return 0;
  switch (s.mode) {
    case ArpMode::Up: {
      size_t i = s.index % n;
      s.index = i + 1;
      return i;
    }
    case ArpMode::Down: {
      if (s.index == 0) s.index = n;
      s.index--;
      return s.index;
    }
    case ArpMode::UpDown: {
      if (n == 1) return 0;   // single note: no direction to ping-pong
      size_t i = s.index;
      if (s.dirUp) {
        if (s.index < n - 1) s.index++;
        else { s.dirUp = false; s.index--; }
      } else {
        if (s.index > 0) s.index--;
        else { s.dirUp = true; s.index++; }
      }
      return i;
    }
    case ArpMode::Random:
      return (size_t)rand() % n;
  }
  return 0;
}

// Reset position — call when the held-note set changes composition, or when
// the arp is (re-)enabled, so a new chord always starts from the pattern's
// natural beginning rather than a stale mid-cycle index.
inline void arpReset(ArpState& s) {
  s.index = 0;
  s.dirUp = true;
}

inline const char* arpModeName(ArpMode m) {
  switch (m) {
    case ArpMode::Up:     return "UP";
    case ArpMode::Down:   return "DN";
    case ArpMode::UpDown: return "U/D";
    case ArpMode::Random: return "RND";
    default:              return "?";
  }
}

inline ArpMode nextArpMode(ArpMode m) {
  switch (m) {
    case ArpMode::Up:     return ArpMode::Down;
    case ArpMode::Down:   return ArpMode::UpDown;
    case ArpMode::UpDown: return ArpMode::Random;
    default:              return ArpMode::Up;
  }
}

}  // namespace synth
