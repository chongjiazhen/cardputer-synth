// Sample playback math. Pure, no Arduino/M5 includes — host-testable.
// Reads a recorded int16 buffer at a fractional position with linear
// interpolation, returning a sample in -1..1. Hardware mic capture lives in
// main.cpp; only the read/resample math is here.
#pragma once
#include <cstdint>
#include <cmath>

namespace synth {

// Linear-interpolated read of buf[pos], pos in [0, len). Wraps for positions at
// or past the end so a held note loops the buffer seamlessly. len must be > 0.
inline double sampleRead(const int16_t* buf, int len, double pos) {
  if (len <= 0) return 0.0;
  // Wrap pos into [0, len).
  double p = std::fmod(pos, (double)len);
  if (p < 0) p += len;
  int   i0 = (int)p;
  int   i1 = i0 + 1; if (i1 >= len) i1 = 0;     // wrap the upper tap too
  double frac = p - i0;
  double a = buf[i0] / 32768.0;
  double b = buf[i1] / 32768.0;
  return a + (b - a) * frac;
}

// Playback step (buffer samples advanced per output sample) for a MIDI note,
// with C4 (60) = 1.0 (original speed/pitch). Each octave doubles the rate.
inline double sampleStep(int midi) {
  return std::pow(2.0, (midi - 60) / 12.0);
}

// --- Granular pitch-shift ------------------------------------------------
// Decouples pitch from playback rate (the plain sampler above welds them:
// higher note = faster read = higher pitch AND shorter/faster = "tape" mode).
//
// Two independent cursors are the whole trick:
//   * playhead advances at `rate` buffer-samples/output-sample → sets the
//     TIME BASE (1.0 = recorded speed; the sample's length/loop period is
//     preserved regardless of pitch).
//   * each grain READS at `pitchRatio` → sets the PITCH, independent of rate.
// Two Hann-windowed grains, half a window out of phase; Hann at 50% overlap
// sums to 1.0, so the overlap-add has no amplitude ripple. Each grain
// re-anchors to the current playhead when its window restarts.
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

// Grain length in output samples (~32 ms at 32 kHz). Tune by ear: shorter =
// tighter/grainier, longer = smoother but more smeared transients.
static constexpr int GRAIN_LEN = 1024;

struct Grain {
  float playhead = 0.0f;   // time-base cursor (advances at `rate`), wraps [0,len)
  float phase    = 0.0f;   // grain A phase 0..1; grain B is this + 0.5
  float anchorA  = 0.0f;   // buffer pos where grain A's current window started
  float anchorB  = 0.0f;   // buffer pos where grain B's current window started

  // Restart both cursors at a buffer position (call on note-on).
  void reset(float startPos = 0.0f) {
    playhead = startPos; phase = 0.0f;
    anchorA  = startPos; anchorB = startPos;
  }

  static float hann(float p) {           // window over one grain, 0 at edges
    return 0.5f - 0.5f * std::cos(2.0f * (float)M_PI * p);
  }

  // One output sample. `rate` = time-base speed (1.0 = original), `pitchRatio`
  // = transposition (2.0 = octave up). Length is governed by `rate`, not pitch.
  float process(const int16_t* buf, int len, float rate, float pitchRatio) {
    if (len <= 0) return 0.0f;
    float pB = phase + 0.5f; if (pB >= 1.0f) pB -= 1.0f;

    // Grain read = anchor + (grain-local elapsed output samples) * pitch.
    float posA = anchorA + phase * (float)GRAIN_LEN * pitchRatio;
    float posB = anchorB + pB    * (float)GRAIN_LEN * pitchRatio;
    float out  = hann(phase) * (float)sampleRead(buf, len, posA)
               + hann(pB)    * (float)sampleRead(buf, len, posB);

    // Advance grain phase; re-anchor each grain the instant its window wraps.
    float prev = phase;
    phase += 1.0f / (float)GRAIN_LEN;
    if (phase >= 1.0f) { phase -= 1.0f; anchorA = playhead; }   // A restarted
    if (prev < 0.5f && phase >= 0.5f) anchorB = playhead;       // B restarted

    // Advance the time base (independent of pitch).
    playhead += rate;
    if (playhead >= (float)len) playhead -= (float)len;
    if (playhead < 0.0f)        playhead += (float)len;
    return out;
  }
};

}  // namespace synth
