// Modulation matrix — routes sources to destinations with depth scaling.
// Pure math, no Arduino/M5 includes — host-testable.
//
// Fixed-size matrix: N_SRC sources × N_DST destinations. Each slot holds a
// depth value (-1..+1). Sources are computed per-sample by the voice; the
// matrix multiplies them through and accumulates per-destination modulation
// amounts that the voice adds to its base parameter values.
#pragma once
#include <cmath>

namespace synth {

// Modulation sources — indices into the source value array.
enum class ModSrc : int {
  Env1 = 0,   // amp envelope (0..1)
  Env2,       // filter / modulation envelope (0..1)
  Lfo1,       // LFO 1 (-1..+1)
  Lfo2,       // LFO 2 (-1..+1)
  Velocity,   // note-on velocity (0..1)
  KeyTrack,   // normalized key position (0..1 low-to-high)
  COUNT       // sentinel
};

constexpr int MOD_SRC_COUNT = (int)ModSrc::COUNT;

// Modulation destinations — indices into the destination offset array.
enum class ModDst : int {
  Pitch       = 0,  // semitones
  FilterCut,         // Hz offset
  FilterRes,         // Q offset
  Amp,               // level multiplier
  OscShape,          // waveform morph (for future use)
  FmDepth,           // FM modulation index (for future use)
  Lfo1Depth,         // LFO1 depth self-mod
  Lfo2Depth,         // LFO2 depth self-mod
  COUNT              // sentinel
};

constexpr int MOD_DST_COUNT = (int)ModDst::COUNT;

struct ModMatrix {
  // Depth table: depth[src][dst]. -1.0 to +1.0.
  float depth[MOD_SRC_COUNT][MOD_DST_COUNT] = {};

  // Computed destination offsets (accumulated per sample).
  float dst[MOD_DST_COUNT] = {};

  // Set a routing: source → destination with depth.
  void set(ModSrc src, ModDst dest, float d) {
    depth[(int)src][(int)dest] = d;
  }

  // Clear all routings.
  void clear() {
    for (int s = 0; s < MOD_SRC_COUNT; s++)
      for (int d = 0; d < MOD_DST_COUNT; d++)
        depth[s][d] = 0.0f;
  }

  // Process one sample: multiply source values through the matrix and
  // accumulate destination offsets. `src` is an array of source values
  // indexed by ModSrc (must have MOD_SRC_COUNT elements).
  void process(const float src[]) {
    for (int d = 0; d < MOD_DST_COUNT; d++) {
      float sum = 0.0f;
      for (int s = 0; s < MOD_SRC_COUNT; s++) {
        sum += src[s] * depth[s][d];
      }
      dst[d] = sum;
    }
  }

  // Get the current modulation offset for a destination.
  float get(ModDst d) const { return dst[(int)d]; }
};

}  // namespace synth