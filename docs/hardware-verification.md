# On-device verification checklist

Everything below needs a physical **Cardputer ADV** — host tests + Wokwi cover
DSP math and boot, but **audio, IMU, and MIDI are unverified until flashed**
(Wokwi emulates none of them; its board model is the classic Cardputer, not the
ADV). This is the gate for declaring a hardware-verified v1.

Tick each box on real hardware. Note the firmware/git SHA you tested against.

## 0. Flash

```bash
pio run -e synth            -t upload   # audio only
pio run -e synth-usb-midi   -t upload   # audio + USB MIDI
pio run -e synth-ble-midi   -t upload   # audio + BLE MIDI
```

- Hold **G0 / BtnA at boot** only if the board won't enter download mode on its
  own (esptool usually auto-resets the Stamp-S3).
- After upload the Cardputer reboots into the synth; the display shows
  `SYNTH / oct N / vol N / last / wave / amp`.

### Serial-monitor caveat (USB MIDI build only)

`synth-usb-midi` sets `ARDUINO_USB_MODE=0` (TinyUSB OTG) so the device can
enumerate as a MIDI class. That **moves USB-CDC serial under TinyUSB** — the
serial monitor behaves differently than the `synth` / `synth-ble-midi` builds
(which use the default HWCDC). If `pio device monitor` shows nothing on the USB
MIDI build, that's expected, not a hang. Flashing still works (bootloader is
separate from the app USB stack).

## 1. Audio (env: `synth`)

- [ ] Headphones / powered speaker in the 3.5 mm jack **and** the on-board 1 W
      speaker both produce tone.
- [ ] White keys `z x c v b n m` = C D E F G A B; black keys `s d g h j` =
      C# D# F# G# A#.
- [ ] Hold a key → tone **sustains** (does not decay to silence); release →
      short release tail, then silence. (This was the original "120 ms bell, no
      sustain" bug — confirm it's gone.)
- [ ] `1 2 3 4` switch sine / saw / square / tri — audibly different timbres,
      roughly matched in loudness (no shape jumps out far louder).
- [ ] `,` darkens the tone (low-pass closes), `.` opens it back to bright;
      `cut:` on screen shows the cutoff Hz / "open". Most obvious on saw/square.
- [ ] `[` / `]` (or `;` / `'`) octave down / up, clamp 1..7.
- [ ] `-` / `=` volume down / up.

## 2. IMU expression (env: `synth`)

Two sensors: **accel** (absolute tilt, persists when held) drives velocity +
vibrato; **gyro** (angular rate, self-centers) drives pitch bend. Both zero to
the boot pose — hold the board **still and flat at power-on** for a clean center.

- [ ] **Velocity** — tilt board **fwd/back**, then press a key: tilt one way →
      note hits hard, the other way → soft, flat → medium. `vel:` on screen
      shows the latched value. (The keyboard has no velocity sensing; tilt is
      the dynamics source.)
- [ ] **Vibrato** — tilt board **left/right** while holding a note → pitch
      wobble (~5.5 Hz) deepens; `vib:` tracks 0..1. Resting/flat → no vibrato.
- [ ] **Pitch bend** — **twist** (gz) → pitch bends ±2 semitones and **springs
      back** when you stop twisting (self-centering wheel).
- [ ] Board still + flat → vel mid, vib 0, no bend drift (calibration good).

> **Axis check:** if fwd/back and left/right feel swapped (velocity reacts to
> side tilt, vibrato to fwd tilt), the accel axes are mapped the other way for
> this board — swap `raw.ay`/`raw.ax` in the IMU block of `main.cpp`. If a
> direction is inverted, negate that axis.

## 3. USB MIDI → REAPER (env: `synth-usb-midi`)

- [ ] Plug USB-C into the computer. Device enumerates as a MIDI device named
      **"Cardputer Synth"** (Windows: *Device Manager → Sound* / *MIDI*; macOS:
      *Audio MIDI Setup → MIDI Studio*).
- [ ] REAPER → *Preferences → MIDI Devices* → "Cardputer Synth" input shows up;
      **enable** it.
- [ ] Arm a track with a virtual instrument, input = the device. Pressing synth
      keys triggers **note on/off**, with **velocity** that tracks your fwd/back
      tilt at the moment of each press (not a fixed value).
- [ ] Tilt left/right → **CC 1** (mod = vibrato depth); twist → **pitch-bend**
      arrive — watch REAPER's MIDI monitor / the instrument's mod + bend
      response. (Only CC1 + pitch-bend now; CC7/CC11 are gone.)
- [ ] Hold the board still → no CC/bend flood (the change-only guard works).

## 4. BLE MIDI → REAPER (env: `synth-ble-midi`)

- [ ] Pair the host to BLE device **"Cardputer Synth"**. On Windows, REAPER does
      not see BLE-MIDI natively — bridge it (e.g. **loopMIDI + a BLE-MIDI
      connector**, or use **macOS** *Audio MIDI Setup → Bluetooth* which is
      first-class).
- [ ] Same velocity + CC1 (vibrato) + pitch-bend checks as §3, over BLE.
- [ ] Latency is usable for playing (BLE adds a few ms over USB).

## Controls reference

| Keys | Action |
|------|--------|
| `z x c v b n m` | white keys C D E F G A B |
| `s d g h j` | black keys C# D# F# G# A# |
| `[` `]` (or `;` `'`) | octave down / up (1..7) |
| `-` `=` | volume down / up |
| `1 2 3 4` | waveform: sine / saw / square / tri |
| `,` `.` | low-pass cutoff down / up (toward open) |
| tilt fwd/back | note velocity (latched at press) |
| tilt left/right | vibrato depth (→ CC1) |
| twist (gz) | pitch bend ±2 semitones |

> Wake the keyboard with **Fn + key** first if it isn't responding.

## Known untested / gotchas

- USB and BLE MIDI **compile** but have never been exercised on hardware — this
  checklist is exactly the first run.
- USB MIDI uses Adafruit TinyUSB at `ARDUINO_USB_MODE=0`; if enumeration fails,
  suspect the USB mode / descriptor before the app logic.
- Pitch-bend range is fixed at ±2 semitones (`IMU_BEND_SEMITONES` in
  `imu_map.h`); REAPER instruments must have matching bend range to sound in
  tune.
