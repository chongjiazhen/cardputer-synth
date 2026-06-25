// Voice allocation policy. Pure, no Arduino/M5 includes — host-testable.
#pragma once
#include "voice.h"

namespace synth {

// Pick a voice for a new note: prefer an inactive voice; if none, steal the
// oldest active one (lowest age). Stamps the chosen voice's age with nowAge
// (a monotonically increasing counter) so later steals pick the true oldest.
// Returns the voice index.
inline int allocVoice(Voice* v, int n, int nowAge) {
  int oldest = 0;
  for (int i = 0; i < n; i++) {
    if (!v[i].active) { v[i].age = nowAge; return i; }
    if (v[i].age < v[oldest].age) oldest = i;
  }
  v[oldest].age = nowAge;
  return oldest;
}

// Find the active voice gated by `key` (for note-off). Returns index or -1.
inline int findVoiceByKey(Voice* v, int n, char key) {
  for (int i = 0; i < n; i++)
    if (v[i].active && v[i].key == key) return i;
  return -1;
}

}  // namespace synth
