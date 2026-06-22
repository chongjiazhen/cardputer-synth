# CLAUDE.md — cardputer-synth

Monophonic synth firmware for the M5Stack Cardputer ADV (ESP32-S3). PlatformIO.

## Layout

- `src/synth/` — app. `main.cpp` = hardware glue; `oscillator.h` / `adsr.h` /
  `notes.h` / `imu_map.h` = pure DSP/logic, host-testable; `*_test.cpp` = g++ host tests.
- `lib/cardputer_hw/` — thin display/keyboard/volume wrapper over M5Cardputer.

## Conventions

- Board `m5stack-stamps3`, framework arduino, `M5Cardputer`.
- Build one env: `pio run -e synth` (also `synth-usb-midi`, `synth-ble-midi`).
- Pure-logic headers stay free of Arduino/M5 includes so they host-test with g++.
- Secrets (`secrets.h`) gitignored.
