// Host test for imu_map.h. Compile and run:
//   g++ -std=c++17 imu_map_test.cpp -o t && ./t
#include "imu_map.h"
#include <cassert>
#include <cstdio>
#include <cmath>

static void check(bool cond, const char* msg) {
  if (!cond) { fprintf(stderr, "FAIL: %s\n", msg); assert(false); }
}

static bool near(float a, float b, float tol = 1.0f) {
  return std::fabs(a - b) <= tol;
}

int main() {
  using namespace synth;

  // --- velocity: flat (0) → midpoint ---
  {
    uint8_t v = tiltVelocity(0.0f);
    int mid = VEL_MIN + (int)((127 - VEL_MIN) * 0.5f + 0.5f);
    check(v == (uint8_t)mid, "velocity flat = midpoint");
  }
  // --- velocity: full forward tilt → 127 ---
  check(tiltVelocity(IMU_TILT_RANGE)  == 127, "velocity +full = 127");
  check(tiltVelocity(2 * IMU_TILT_RANGE) == 127, "velocity clamps high");
  // --- velocity: full back tilt → VEL_MIN ---
  check(tiltVelocity(-IMU_TILT_RANGE) == VEL_MIN, "velocity -full = VEL_MIN");
  check(tiltVelocity(-2 * IMU_TILT_RANGE) == VEL_MIN, "velocity clamps low");

  // --- vibrato: bidirectional (magnitude), resting → 0 ---
  check(tiltVibrato(0.0f)  == 0.0f, "vibrato flat = 0");
  check(tiltVibrato(0.01f) == 0.0f, "vibrato below deadzone = 0");
  // --- both directions deepen vibrato symmetrically ---
  check(near(tiltVibrato(IMU_TILT_RANGE),  1.0f, 0.001f), "vibrato +full = 1");
  check(near(tiltVibrato(-IMU_TILT_RANGE), 1.0f, 0.001f), "vibrato -full = 1");
  check(near(tiltVibrato(2 * IMU_TILT_RANGE), 1.0f, 0.001f), "vibrato clamps");
  check(near(tiltVibrato(IMU_TILT_RANGE * 0.5f),  0.5f, 0.001f), "vibrato +half");
  check(near(tiltVibrato(-IMU_TILT_RANGE * 0.5f), 0.5f, 0.001f), "vibrato -half");

  // --- pitch bend: center, ±full (self-centering via deadzone) ---
  check(near(gyroBend(0.0f), 0.0f, 0.001f), "bend center = 0");
  check(near(gyroBend(1.0f), 0.0f, 0.001f), "bend within deadzone = 0");
  check(near(gyroBend(IMU_RANGE),  IMU_BEND_SEMITONES, 0.001f), "bend +full");
  check(near(gyroBend(-IMU_RANGE), -IMU_BEND_SEMITONES, 0.001f), "bend -full");

  // --- calibrate: offsets stored ---
  {
    auto c = calibrate(0.1f, -0.2f, 3.0f);
    check(near(c.ax0,  0.1f, 0.001f), "calib ax0");
    check(near(c.ay0, -0.2f, 0.001f), "calib ay0");
    check(near(c.gz0,  3.0f, 0.001f), "calib gz0");
  }

  puts("ok");
  return 0;
}
