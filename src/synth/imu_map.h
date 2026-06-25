// IMU expression mapping. Pure math, no Arduino/M5 includes — host-testable.
// Two sensors, two roles:
//   accel (absolute tilt, persists when held) → velocity (latched per note-on)
//                                              + vibrato depth (continuous)
//   gyro  (angular rate, self-centering)       → pitch bend (continuous)
// Caller samples imuRead() and passes CALIBRATED readings (raw minus the
// boot-pose zero captured by calibrate()).
#pragma once
#include <cmath>
#include <cstdint>

namespace synth {

struct ImuCalib {
  float ax0 = 0, ay0 = 0;  // accel tilt zero (boot pose = flat reference)
  float gz0 = 0;           // gyro z zero offset (drift)
};

// --- Gyro → pitch bend ---
constexpr float IMU_DEAD  = 3.0f;          // deg/s deadzone
constexpr float IMU_RANGE = 60.0f;         // deg/s full-scale rotation
constexpr float IMU_BEND_SEMITONES = 2.0f; // ±2 = MIDI default bend

// --- Accel tilt → velocity + vibrato. Units = g; ~0.5 g ≈ 30° tilt = full. ---
constexpr float   IMU_TILT_RANGE    = 0.5f;
constexpr uint8_t VEL_MIN           = 30;   // floor so soft notes stay audible
constexpr float   VIB_DEADZONE      = 0.05f;

// --- Vibrato LFO ---
constexpr float VIB_MAX_SEMITONES = 0.5f;   // depth 1.0 → ±0.5 semitone
constexpr float LFO_HZ            = 5.5f;

inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

inline float deadzone(float v) {
  return (std::fabs(v) < IMU_DEAD) ? 0.0f : v;
}

// Forward/back tilt (calibrated accel) → MIDI velocity VEL_MIN..127.
// Flat board (0) → midpoint; tilt one way harder, the other softer.
inline uint8_t tiltVelocity(float tiltFwd) {
  float n = clampf(tiltFwd / IMU_TILT_RANGE, -1.0f, 1.0f);
  float u = (n + 1.0f) * 0.5f;                       // 0..1, flat = 0.5
  int   vel = VEL_MIN + (int)((127 - VEL_MIN) * u + 0.5f);
  return (uint8_t)clampf((float)vel, 1.0f, 127.0f);
}

// Left/right tilt (calibrated accel) → vibrato depth 0..1, one direction (like
// pushing a mod wheel up). Resting board / opposite tilt = 0.
inline float tiltVibrato(float tiltSide) {
  float d = clampf(tiltSide / IMU_TILT_RANGE, 0.0f, 1.0f);
  return (d < VIB_DEADZONE) ? 0.0f : d;
}

// Gyro z rate → pitch bend in semitones (self-centering via deadzone).
inline float gyroBend(float gz) {
  float nz = clampf(deadzone(gz) / IMU_RANGE, -1.0f, 1.0f);
  return IMU_BEND_SEMITONES * nz;
}

// Calibration: call once at startup while the device is still + flat.
inline ImuCalib calibrate(float ax, float ay, float gz) {
  ImuCalib c;
  c.ax0 = ax; c.ay0 = ay; c.gz0 = gz;
  return c;
}

}  // namespace synth
