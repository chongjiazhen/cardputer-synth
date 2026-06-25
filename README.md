# cardputer-synth

A monophonic synthesizer firmware for the **M5Stack Cardputer ADV** (ESP32-S3).
Four waveforms, an ADSR envelope, IMU tilt expression, and USB-MIDI + BLE-MIDI
output.

## Features

- **Waveforms:** sine / saw / square / triangle (keys `1`–`4`)
- **ADSR envelope** with key-held sustain
- **IMU expression** — gyro rotation maps to amplitude (gx), volume (gy), and
  pitch bend ±2 semitones (gz, self-centering); graceful no-op if IMU absent
- **MIDI out** — USB-MIDI and BLE-MIDI (device name "Cardputer Synth"); sends
  note on/off + CC 1 (mod) / CC 7 (volume) / pitch-bend from gyro expression

## Build

Requires [PlatformIO](https://platformio.org/).

```bash
pio run -e synth            # audio only
pio run -e synth-usb-midi   # audio + USB MIDI
pio run -e synth-ble-midi   # audio + BLE MIDI
```

## Flash

```bash
pio run -e synth -t upload
```

## Simulate (Wokwi)

Open this folder in VS Code with the Wokwi extension and run `wokwi.toml`, or
upload `firmware.bin` + `diagram.json` to https://wokwi.com.

**Simulation limits:** Wokwi runs the firmware on a virtual ESP32-S3 — it shows
display + keyboard and proves the firmware boots, but it does **not** emulate
audio, MIDI, or the IMU. The board model is the classic Cardputer, not the ADV,
so the key map is not 1:1. To actually hear the synth, flash real hardware.

To verify audio / IMU / MIDI on a physical Cardputer ADV, follow
[docs/hardware-verification.md](docs/hardware-verification.md).

## Host tests

The DSP/logic is pure C++ and host-testable with g++:

```bash
g++ -std=c++17 -I src/synth src/synth/oscillator_test.cpp -o /tmp/t && /tmp/t
g++ -std=c++17 -I src/synth src/synth/adsr_test.cpp       -o /tmp/t && /tmp/t
g++ -std=c++17 -I src/synth src/synth/notes_test.cpp      -o /tmp/t && /tmp/t
g++ -std=c++17 -I src/synth src/synth/imu_map_test.cpp    -o /tmp/t && /tmp/t
```

## Roadmap

Planned features (mic sampling, filter, FM, IMU expression redesign) and
hardware-imposed limits are in [docs/roadmap.md](docs/roadmap.md).

## License

MIT — see [LICENSE](LICENSE).
