// Host test for arp.h. Compile and run:
//   g++ -std=c++17 arp_test.cpp -o t && ./t
#include "arp.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>

static void check(bool cond, const char* msg) {
  if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); assert(false); }
}

int main() {
  using namespace synth;

  // --- Up: cycles 0,1,2,0,1,2,... ---
  {
    ArpState s; s.mode = ArpMode::Up;
    size_t seq[6];
    for (auto& i : seq) i = arpStep(s, 3);
    size_t want[6] = {0, 1, 2, 0, 1, 2};
    for (int i = 0; i < 6; i++) check(seq[i] == want[i], "Up sequence");
  }

  // --- Down: cycles 2,1,0,2,1,0,... ---
  {
    ArpState s; s.mode = ArpMode::Down;
    size_t seq[6];
    for (auto& i : seq) i = arpStep(s, 3);
    size_t want[6] = {2, 1, 0, 2, 1, 0};
    for (int i = 0; i < 6; i++) check(seq[i] == want[i], "Down sequence");
  }

  // --- UpDown: ping-pongs 0,1,2,1,0,1,2,1,0,... (endpoints not repeated) ---
  {
    ArpState s; s.mode = ArpMode::UpDown;
    size_t seq[9];
    for (auto& i : seq) i = arpStep(s, 3);
    size_t want[9] = {0, 1, 2, 1, 0, 1, 2, 1, 0};
    for (int i = 0; i < 9; i++) check(seq[i] == want[i], "UpDown sequence");
  }

  // --- UpDown with a single note: always index 0, no crash ---
  {
    ArpState s; s.mode = ArpMode::UpDown;
    for (int i = 0; i < 5; i++) check(arpStep(s, 1) == 0, "UpDown n=1 stays at 0");
  }

  // --- Random: always in range [0, n) ---
  {
    ArpState s; s.mode = ArpMode::Random;
    for (int i = 0; i < 200; i++) {
      size_t idx = arpStep(s, 5);
      check(idx < 5, "Random in range");
    }
  }

  // --- n == 0: doesn't crash, returns 0 ---
  {
    ArpState s; s.mode = ArpMode::Up;
    check(arpStep(s, 0) == 0, "n=0 safe");
  }

  // --- arpReset zeroes index and direction ---
  {
    ArpState s; s.mode = ArpMode::UpDown;
    arpStep(s, 3); arpStep(s, 3); arpStep(s, 3);   // index=2, about to flip
    check(s.index != 0, "precondition: index moved");
    arpReset(s);
    check(s.index == 0 && s.dirUp == true, "arpReset zeroes state");
  }

  // --- nextArpMode cycles Up->Down->UpDown->Random->Up ---
  {
    check(nextArpMode(ArpMode::Up) == ArpMode::Down, "next Up->Down");
    check(nextArpMode(ArpMode::Down) == ArpMode::UpDown, "next Down->UpDown");
    check(nextArpMode(ArpMode::UpDown) == ArpMode::Random, "next UpDown->Random");
    check(nextArpMode(ArpMode::Random) == ArpMode::Up, "next Random->Up (wraps)");
  }

  // --- arpModeName covers all modes ---
  {
    check(arpModeName(ArpMode::Up)[0]     != '?', "name Up");
    check(arpModeName(ArpMode::Down)[0]   != '?', "name Down");
    check(arpModeName(ArpMode::UpDown)[0] != '?', "name UpDown");
    check(arpModeName(ArpMode::Random)[0] != '?', "name Random");
  }

  puts("ok");
  return 0;
}
