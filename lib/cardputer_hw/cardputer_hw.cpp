#include "cardputer_hw.h"

namespace cardputer {

static uint8_t g_brightness = 128;
static uint8_t g_volume = 128;

void begin() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg);          // inits M5Unified + the Cardputer/ADV keyboard
  M5.Display.setRotation(1);
  M5.Display.setTextSize(2);
  brightness(g_brightness);
  volume(g_volume);
}

void update() { M5Cardputer.update(); }

bool keyPressed() {
  return M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed();
}

std::vector<char> keysJustPressed() {
  std::vector<char> out;
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    auto status = M5Cardputer.Keyboard.keysState();
    for (auto c : status.word) out.push_back(c);
  }
  return out;
}

std::vector<char> keysHeld() {
  std::vector<char> out;
  auto status = M5Cardputer.Keyboard.keysState();   // current matrix, every frame
  for (auto c : status.word) out.push_back(c);
  return out;
}

bool fnHeld() {
  return M5Cardputer.Keyboard.keysState().fn;
}

String readLine(const String& prompt) {
  String buf;
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.print(prompt);
  for (;;) {
    update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      auto status = M5Cardputer.Keyboard.keysState();
      if (status.enter) break;
      if (status.del && buf.length()) buf.remove(buf.length() - 1);
      for (auto c : status.word) buf += c;
      M5.Display.fillScreen(TFT_BLACK);
      M5.Display.setCursor(0, 0);
      M5.Display.print(prompt);
      M5.Display.print(buf);
    }
    delay(5);
  }
  return buf;
}

void brightness(uint8_t level) { g_brightness = level; M5.Display.setBrightness(level); }
uint8_t brightness() { return g_brightness; }

void volume(uint8_t level) { g_volume = level; M5.Speaker.setVolume(level); }
uint8_t volume() { return g_volume; }

bool imuRead(Imu& out) {
  if (!M5.Imu.isEnabled()) return false;
  M5.Imu.update();
  auto d = M5.Imu.getImuData();
  out = {d.accel.x, d.accel.y, d.accel.z, d.gyro.x, d.gyro.y, d.gyro.z};
  return true;
}

}  // namespace cardputer
