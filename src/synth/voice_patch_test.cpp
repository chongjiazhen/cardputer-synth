// Host repro of the DEFAULT patch to hunt the "scratchy sine" bug. Renders a
// held C4 sine through the real Voice chain (osc -> SVF -> env), no IMU
// (bend=vib=1), then measures how far the steady-state output is from a pure
// sine. A clean sine satisfies x[n] = 2cos(w)x[n-1] - x[n-2]; the residual of
// that recurrence is ~0 for a pure tone and spikes on any added harshness.
//   g++ -std=c++17 src/synth/voice_patch_test.cpp -o vt && ./vt
#include "voice.h"
#include "notes.h"
#include <cstdio>
#include <cmath>

using namespace synth;

static constexpr double SR = 32000.0;

static double analyzeNote(int midi);

int main() {
  // Sweep notes: is the pure chain dirty specifically at G (vs A)? If every note
  // is clean, the G-vs-A distortion is acoustic (enclosure/speaker), not DSP.
  const int notes[] = {55, 57, 60, 67, 69, 79};   // G3 A3 C4 G4 A4 G5
  const char* names[] = {"G3", "A3", "C4", "G4", "A4", "G5"};
  printf("note  freqHz   residual%%\n");
  for (int i = 0; i < 6; i++)
    printf("%-4s  %6.1f   %.4f%%\n", names[i], midiToHz(notes[i]), analyzeNote(notes[i]));
  return 0;
}

static double analyzeNote(int midi) {
  // --- replicate noteOnMidi()'s default patch ---
  Voice v;
  v.active   = true;
  v.key      = 'z';
  v.midi     = midi;
  v.phase    = 0.0f;
  v.phaseInc = TWO_PI_F * (float)midiToHz(midi) / (float)SR;
  v.vel      = 100.0f / 127.0f;

  v.env1.configMs(SR, 8.0, 40.0, 0.7, 120.0);
  v.env2.configMs(SR, 5.0, 80.0, 0.5, 200.0);
  v.env1.gateOn();
  v.env2.gateOn();

  v.filterMode    = FilterMode::LP;
  v.baseCutoff    = 2000.0f;    // g_cutoff default
  v.baseResonance = 0.707f;     // g_resonance default
  v.filter.reset();
  v.filterCtr     = 0;

  v.mod.clear();
  v.mod.set(ModSrc::Env2, ModDst::FilterCut, 1.0f);

  v.lfo1.setFreq(5.5, SR); v.lfo1.depth = 0.0f;
  v.lfo2.setFreq(0.5, SR); v.lfo2.depth = 0.0f;
  v.grain.reset(0.0f);

  const int N = (int)(SR * 0.3);   // 300 ms
  const int SETTLE = (int)(SR * 0.15);  // skip attack+decay+filter settle
  double w = 2.0 * M_PI * midiToHz(midi) / SR;
  double c2 = 2.0 * std::cos(w);

  float buf[9600];
  for (int n = 0; n < N; n++)
    buf[n] = voiceSample(v, WaveShape::Sine, 1.0f, 1.0f, SR);

  double peak = 0.0, maxResid = 0.0;
  for (int n = SETTLE + 2; n < N; n++) {
    double a = std::fabs(buf[n]);
    if (a > peak) peak = a;
    double resid = std::fabs(buf[n] - c2 * buf[n-1] + buf[n-2]);
    if (resid > maxResid) maxResid = resid;
  }
  return peak > 0 ? 100.0 * maxResid / peak : 0.0;
}
