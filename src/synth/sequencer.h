// Step sequencer: a steps x tracks grid with a sample-accurate clock. Pure
// math/logic, NO Arduino/M5 includes — host-testable with plain g++ (see
// sequencer_test.cpp). This header owns "which step are we on, and did we just
// cross a boundary." Turning a fired step into sound (voice alloc, MIDI out,
// gate on/off) is hardware glue in main.cpp — same split as arp.h.
#pragma once
#include <cstdint>

namespace synth {

constexpr int SEQ_MAX_STEPS  = 16;
constexpr int SEQ_MAX_TRACKS = 4;

// One cell of the grid. gate=false means the step is silent (a rest).
struct Step {
  bool    gate = false;
  uint8_t note = 60;    // MIDI note played when gated (60 = C4)
  uint8_t vel  = 100;   // velocity 0..127
};

struct Track {
  Step steps[SEQ_MAX_STEPS];
  bool mute = false;
};

struct Sequencer {
  Track   tracks[SEQ_MAX_TRACKS];
  int     length       = 16;        // active step count (<= SEQ_MAX_STEPS)
  int     stepsPerBeat = 4;         // 4 => 16th notes at the given BPM
  float   bpm          = 120.0f;
  bool    running      = false;

  // Clock. Fed samples; steps advance when the accumulator crosses a boundary.
  uint32_t sampleRate  = 44100;
  double   acc         = 0.0;       // samples elapsed since last step boundary
  int      cur         = 0;         // current step index (0..length-1)
};

// Samples between two step boundaries. stepsPerSec = beats/sec * steps/beat.
inline double seqSamplesPerStep(const Sequencer& s) {
  double stepsPerSec = (double(s.bpm) / 60.0) * s.stepsPerBeat;
  return double(s.sampleRate) / stepsPerSec;
}

// Advance the clock by `frames` samples (call once per audio block). Returns
// the new step index if a boundary was crossed this block, else -1. On a
// non-negative return, trigger every non-muted track's Step at that column.
//
// Assumption: block size < one step (true on Cardputer — at 120bpm/16ths a
// step is ~5512 samples, blocks are 64..256). If you ever run blocks longer
// than a step, this drops the extra crossings — turn the `if` into a `while`
// and emit one event per crossing. Left as your TODO.
inline int seqAdvance(Sequencer& s, int frames) {
  if (!s.running) return -1;
  s.acc += frames;
  double sps = seqSamplesPerStep(s);
  if (s.acc < sps) return -1;
  s.acc -= sps;
  s.cur = (s.cur + 1) % s.length;
  return s.cur;
}

inline void seqReset(Sequencer& s) { s.cur = 0; s.acc = 0.0; }
inline void seqStart(Sequencer& s) { seqReset(s); s.running = true; }
inline void seqStop (Sequencer& s) { s.running = false; }

// Toggle a step's gate — the primary edit op from the keyboard grid.
inline void seqToggleGate(Sequencer& s, int track, int step) {
  if (track < 0 || track >= SEQ_MAX_TRACKS) return;
  if (step  < 0 || step  >= SEQ_MAX_STEPS)  return;
  Step& st = s.tracks[track].steps[step];
  st.gate = !st.gate;
}

}  // namespace synth
