// Shared Cardputer ADV hardware helpers.
// Wraps M5Unified for the bits every app on this device needs:
// display init, keyboard read (with the Fn-to-wake quirk), IMU, brightness/volume.
//
// ADV note: the ADV keyboard differs from the original Cardputer. M5Unified
// selects the driver at runtime; -DCARDPUTER_ADV (set in platformio.ini) gates
// any ADV-only paths we add here. Keep app code off raw M5.* where a helper exists.
#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>   // pulls in M5Unified + M5GFX; provides M5Cardputer.Keyboard (Cardputer + ADV)
#include <vector>

namespace cardputer {

// Call once in setup(). Brings up display, keyboard, IMU, speaker.
void begin();

// Pump M5 internals — call once at top of loop().
void update();

// --- keyboard ---
// True for the frame a key transitioned to pressed. Fn-wake is handled by M5;
// caller just reads characters.
bool keyPressed();
// Characters entered since last update() (already de-bounced by M5).
std::vector<char> keysJustPressed();
// Convenience: blocking line input rendered to screen. Returns on Enter.
String readLine(const String& prompt = "");

// --- display ---
void brightness(uint8_t level);   // 0..255, persisted in NVS by caller if wanted
uint8_t brightness();

// --- audio ---
void volume(uint8_t level);       // 0..255 speaker output
uint8_t volume();

// --- imu (gyro/accel) — present on ADV ---
struct Imu { float ax, ay, az, gx, gy, gz; };
bool imuRead(Imu& out);           // false if no IMU / not ready

}  // namespace cardputer
