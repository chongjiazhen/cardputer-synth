// IMU gyro → synth expression mapping. Pure math, no Arduino/M5 includes.
// Caller samples imuRead() and passes gx/gy/gz (deg/s) each loop.
#pragma once
#include <cmath>
#include <cstdint>

namespace synth {

struct ImuCalib {
  float gx0 = 0, gy0 = 0, gz0 = 0;  // zero offsets captured at startup
};

// Dead-zone: values within ±IMU_DEAD of zero treated as zero.
constexpr float IMU_DEAD  = 3.0f;   // deg/s
// Gyro range for full-scale mapping (deg/s)
constexpr float IMU_RANGE = 60.0f;

// Clamp helper
inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Apply dead-zone to a calibrated gyro reading.
inline float deadzone(float v) {
  return (std::fabs(v) < IMU_DEAD) ? 0.0f : v;
}

// Pitch-bend range at full-scale gz rotation (semitones). ±2 = MIDI default.
constexpr float IMU_BEND_SEMITONES = 2.0f;

struct ImuResult {
  float   ampScale;   // 0.2..1.0 multiplied onto AMP in renderChunk
  uint8_t vol;        // 0..255 speaker volume
  float   pitchBend;  // -IMU_BEND_SEMITONES..+IMU_BEND_SEMITONES semitones
};

// Map calibrated gyro readings to synth parameters.
// gx: ±IMU_RANGE → ampScale 0.2..1.0
// gy: ±IMU_RANGE → vol 0..255
// gz: ±IMU_RANGE → pitchBend ∓..±IMU_BEND_SEMITONES semitones (self-centering:
//     gyro reads angular *velocity*, so the bend springs back to 0 when the
//     wrist stops rotating — a spring-loaded pitch wheel.)
inline ImuResult mapImu(float gx, float gy, float gz) {
  float nx = clampf(deadzone(gx) / IMU_RANGE, -1.0f, 1.0f);
  float ny = clampf(deadzone(gy) / IMU_RANGE, -1.0f, 1.0f);
  float nz = clampf(deadzone(gz) / IMU_RANGE, -1.0f, 1.0f);

  ImuResult r;
  r.ampScale  = clampf(0.6f + 0.4f * nx, 0.2f, 1.0f);
  // 128 + 128*ny: ny=-1→0, ny=0→128, ny=+1→256 (clamped to 255).
  float rawVol = 128.0f + 128.0f * ny;
  r.vol       = (uint8_t)clampf(rawVol, 0.0f, 255.0f);
  r.pitchBend = IMU_BEND_SEMITONES * nz;
  return r;
}

// Calibration: call once at startup while device is still.
// Caller may average N readings before passing; a single snapshot works fine.
inline ImuCalib calibrate(float gx, float gy, float gz) {
  ImuCalib c;
  c.gx0 = gx; c.gy0 = gy; c.gz0 = gz;
  return c;
}

}  // namespace synth
