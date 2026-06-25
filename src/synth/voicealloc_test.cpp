// Host test for voicealloc.h:  g++ -std=c++17 voicealloc_test.cpp -o t && ./t
#include "voicealloc.h"
#include <cassert>
#include <cstdio>

static void check(bool c, const char* m) { if (!c) { fprintf(stderr, "FAIL: %s\n", m); assert(false); } }

int main() {
  using namespace synth;
  Voice v[3];

  // all free → returns index 0
  check(allocVoice(v, 3, 1) == 0, "first free = 0");

  // mark 0 and 1 active → returns the remaining free (2)
  v[0].active = true; v[0].age = 1;
  v[1].active = true; v[1].age = 2;
  check(allocVoice(v, 3, 3) == 2, "next free = 2");

  // all active → steal the oldest (lowest age)
  v[2].active = true; v[2].age = 3;
  check(allocVoice(v, 3, 4) == 0, "steal oldest = 0");
  v[0].age = 9;  // 0 now newest
  check(allocVoice(v, 3, 5) == 1, "steal new oldest = 1");

  // allocVoice stamps the chosen voice's age with nowAge
  {
    Voice w[2];
    int idx = allocVoice(w, 2, 42);
    check(w[idx].age == 42, "alloc stamps age");
  }

  // findVoiceByKey
  v[1].key = 'a';
  check(findVoiceByKey(v, 3, 'a') == 1, "find key a");
  check(findVoiceByKey(v, 3, 'z') == -1, "missing key = -1");
  v[1].active = false;
  check(findVoiceByKey(v, 3, 'a') == -1, "inactive not found");

  puts("ok");
  return 0;
}
