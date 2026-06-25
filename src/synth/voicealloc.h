// Voice allocation policy. Pure, no Arduino/M5 includes — host-testable.
#pragma once
#include "voice.h"

namespace synth {

// Pick a voice for a new note: prefer an inactive voice; if none, steal the
// oldest active one (lowest age). Caller passes a monotonically increasing
// nowAge and stores it on the chosen voice. Returns the voice index.
inline int allocVoice(Voice* v, int n, int nowAge) {
  (void)nowAge;
  int oldest = 0;
  for (int i = 0; i < n; i++) {
    if (!v[i].active) return i;
    if (v[i].age < v[oldest].age) oldest = i;
  }
  return oldest;
}

// Find the active voice gated by `key` (for note-off). Returns index or -1.
inline int findVoiceByKey(Voice* v, int n, char key) {
  for (int i = 0; i < n; i++)
    if (v[i].active && v[i].key == key) return i;
  return -1;
}

}  // namespace synth
