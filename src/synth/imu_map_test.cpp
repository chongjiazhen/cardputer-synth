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

  // --- center (0,0,0): ampScale≈0.6, vol≈128, releaseMs≈275 ---
  {
    auto r = mapImu(0, 0, 0);
    check(near(r.ampScale, 0.6f, 0.01f), "center ampScale");
    check(r.vol == 128,                  "center vol");
    check(near(r.releaseMs, 275.0f),     "center releaseMs");
  }

  // --- gx = +IMU_RANGE → ampScale≈1.0 ---
  {
    auto r = mapImu(IMU_RANGE, 0, 0);
    check(near(r.ampScale, 1.0f, 0.01f), "gx+ ampScale");
  }

  // --- gx = -IMU_RANGE → ampScale≈0.2 ---
  {
    auto r = mapImu(-IMU_RANGE, 0, 0);
    check(near(r.ampScale, 0.2f, 0.01f), "gx- ampScale");
  }

  // --- gy = +IMU_RANGE → vol==255 ---
  {
    auto r = mapImu(0, IMU_RANGE, 0);
    check(r.vol == 255, "gy+ vol");
  }

  // --- gy = -IMU_RANGE → vol==0 ---
  {
    auto r = mapImu(0, -IMU_RANGE, 0);
    check(r.vol == 0, "gy- vol");
  }

  // --- gz = +IMU_RANGE → releaseMs≈500 ---
  {
    auto r = mapImu(0, 0, IMU_RANGE);
    check(near(r.releaseMs, 500.0f), "gz+ release");
  }

  // --- gz = -IMU_RANGE → releaseMs≈50 ---
  {
    auto r = mapImu(0, 0, -IMU_RANGE);
    check(near(r.releaseMs, 50.0f), "gz- release");
  }

  // --- dead zone: (1,1,1) all within ±IMU_DEAD → same as (0,0,0) ---
  {
    auto r0 = mapImu(0, 0, 0);
    auto rd = mapImu(1.0f, 1.0f, 1.0f);
    check(near(rd.ampScale,  r0.ampScale,  0.01f), "deadzone ampScale");
    check(rd.vol       == r0.vol,                  "deadzone vol");
    check(near(rd.releaseMs, r0.releaseMs, 0.5f),  "deadzone releaseMs");
  }

  // --- calibrate: offsets stored ---
  {
    auto c = calibrate(5.0f, -2.0f, 3.0f);
    check(near(c.gx0,  5.0f, 0.001f), "calib gx0");
    check(near(c.gy0, -2.0f, 0.001f), "calib gy0");
    check(near(c.gz0,  3.0f, 0.001f), "calib gz0");
  }

  puts("ok");
  return 0;
}
