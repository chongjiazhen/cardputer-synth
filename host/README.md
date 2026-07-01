# host — desktop audio harness

Play the synth on a PC without flashing hardware. Reuses the pure DSP headers in
`src/synth/` verbatim (`oscillator.h`, `voice.h`, `filter.h`, `sampler.h`, …);
only the glue — audio out + keyboard in — lives here, mirroring `main.cpp`.

Purpose: iterate timbre / filter / granular sampler **by ear in seconds** instead
of a ~20 s flash cycle. Flash the real firmware only to verify hardware-specific
behaviour (physical speaker, IMU, USB/BLE MIDI).

## Build & run
```
bash host/build.sh                 # fetches miniaudio.h on first run, compiles
host/desktop_synth                 # oscillators only
host/desktop_synth some.wav        # loads a wav into the sampler (grain/tape)
```
Windows/MinGW g++. `miniaudio.h` (public domain / MIT-0) is fetched by the build
script and gitignored — not committed.

## Keys
```
z x c v b n m   white notes      1-4  waveform (sine/saw/square/tri)
s d g h j       black notes      [ ]  octave down / up
                                 , .  filter cutoff down / up
5  sampler on (needs a wav)      8    sampler pitch: TAPE (varispeed) / GRAN (grain)
q / Esc quit
```

## Known limitation — no key release
A terminal can't detect key *release*. Each press gates a note that auto-releases
after `HOLD_MS` (500 ms); OS key-repeat refreshes it, so holding a key sustains
(after the initial repeat delay). Good enough to audition tone/filter/sampler;
it is not a real keybed. For true note-off you'd swap the terminal input for a
windowed backend (SDL/raylib) — deliberately not done to keep zero install deps.

## Not the source of truth
This harness runs at the PC device rate (usually 48 kHz), not the Cardputer's
32 kHz, and has no analog speaker stage — absolute loudness/aliasing/EQ differ.
Use it for *relative* DSP work (does grain track pitch, does the filter sweep
sound right); confirm final feel on hardware.
