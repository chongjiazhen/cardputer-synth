// Synth — keyboard-driven tone generator on the Cardputer ADV. Hold a note key
// and the tone sustains with an ADSR envelope; release it and the tone fades on
// the release tail. Monophonic, last-note priority. Audio is streamed to the 1W
// speaker in small chunks via M5.Speaker.playRaw(); the ADSR + gate math lives in
// adsr.h (host-tested), note math in notes.h, oscillator in oscillator.h.
//
// Controls (Fn + key to wake keyboard):
//   z x c v b n m  - white keys C D E F G A B (current octave)
//   s d g h j       - black keys C# D# F# G# A#  (hold to sustain)
//   [  / ]          - octave down / up (clamp 1..7); also ; / ' as fallback
//   -  / =          - volume down / up (step 16, 0..255)
//   1 2 3 4         - waveform: Sine / Saw / Square / Tri
//   IMU (gyro)      - rotate gx → amp scale, gy → vol, gz → pitch bend (±2
//                     semitones, self-centering spring-loaded wheel)
//
// Build envs:
//   synth             — standalone audio only
//   synth-usb-midi    — audio + USB MIDI (SYNTH_USB_MIDI=1)
//   synth-ble-midi    — audio + BLE MIDI (SYNTH_BLE_MIDI=1, device "Cardputer Synth")
#include "cardputer_hw.h"
#include "notes.h"
#include "adsr.h"
#include "oscillator.h"
#include "imu_map.h"
#include <cmath>

// --- USB MIDI (TinyUSB device + FortySevenEffects MIDI over it) ---
#ifdef SYNTH_USB_MIDI
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
static Adafruit_USBD_MIDI usbMidiTransport;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usbMidiTransport, usbMidi);
#endif

// --- BLE MIDI ---
#ifdef SYNTH_BLE_MIDI
#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_ESP32.h>
BLEMIDI_CREATE_INSTANCE("Cardputer Synth", MIDI)
#endif

static constexpr uint32_t SR    = 16000;  // sample rate (matches recorder)
static constexpr size_t   CHUNK = 256;    // samples per streamed block (~16ms)
static constexpr int      AMP   = 28000;  // full-scale amplitude, headroom < int16 max

static int16_t g_buf[CHUNK];

static int     g_octave = 4;              // base octave, clamp 1..7
static uint8_t g_vol    = 128;            // 0..255 (speaker output)

// Waveform state
static synth::WaveShape g_wave = synth::WaveShape::Sine;

// IMU state
static synth::ImuCalib g_calib;
static float           g_ampScale  = 1.0f;  // gyro gx → amplitude multiplier
static double          g_bendRatio = 1.0;   // gyro gz → pitch-bend freq ratio

// MIDI last-sent tracking — only transmit on change (prevents controller floods).
// gx→CC1 (mod), gy→CC7 (vol), gz→pitch bend (14-bit, center 0).
#if defined(SYNTH_USB_MIDI) || defined(SYNTH_BLE_MIDI)
static uint8_t g_cc1_last = 0xFF, g_cc7_last = 0xFF;
static int     g_pb_last  = 0x7FFFFFFF;     // sentinel: force first send
#endif

// Voice state — monophonic. Phase runs continuously across chunks (no per-chunk
// reset) so sustained tones don't click.
static synth::Adsr g_env;
static double      g_phaseInc = 0.0;      // radians per sample for current note
static double      g_phase    = 0.0;      // running oscillator phase
static char        g_gateKey  = 0;        // note key currently held, 0 = none

// TWO_PI comes from Arduino.h (6.283185...).

static void redraw(const char* lastNote) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.printf("SYNTH\noct %d  vol %d\nlast: %s\nwave: %s\namp:  %.2f",
                    g_octave, g_vol, lastNote,
                    synth::shapeName(g_wave), g_ampScale);
#ifdef SYNTH_USB_MIDI
  M5.Display.printf("\nUSB MIDI");
#endif
#ifdef SYNTH_BLE_MIDI
  M5.Display.printf("\nBLE MIDI");
#endif
}

// Start (or retrigger) the voice on a semitone in the current octave.
static void noteOn(int semitone) {
  int midi   = synth::noteToMidi(semitone, g_octave);
  g_phaseInc = TWO_PI * synth::midiToHz(midi) / SR;
  g_env.gateOn();
}

// Render one CHUNK of the sustained voice into g_buf and queue it. Phase and
// envelope advance per sample; envelope shapes amplitude (attack/decay/sustain/
// release). g_ampScale from IMU modulates overall output level.
static void renderChunk() {
  for (size_t i = 0; i < CHUNK; i++) {
    double amp = g_env.step();
    g_buf[i] = (int16_t)(synth::osc(g_wave, g_phase) * amp * AMP * g_ampScale);
    g_phase += g_phaseInc * g_bendRatio;   // gz pitch bend
    if (g_phase >= TWO_PI) g_phase -= TWO_PI;
  }
  M5.Speaker.playRaw(g_buf, CHUNK, SR, false, 1, 0);   // channel 0, no repeat
}

static bool heldContains(const std::vector<char>& held, char c) {
  for (char h : held) if (h == c) return true;
  return false;
}

void setup() {
  cardputer::begin();
#ifdef SYNTH_USB_MIDI
  usbMidiTransport.setStringDescriptor("Cardputer Synth");
  usbMidi.begin(MIDI_CHANNEL_OMNI);
#endif
  M5.Speaker.begin();
  cardputer::volume(g_vol);
#ifdef SYNTH_BLE_MIDI
  MIDI.begin(MIDI_CHANNEL_OMNI);
#endif
  // Calibrate IMU: snapshot at startup while device is still.
  {
    cardputer::Imu raw;
    if (cardputer::imuRead(raw)) g_calib = synth::calibrate(raw.gx, raw.gy, raw.gz);
  }
  // Snappy organ-ish default: 8ms attack, 40ms decay, hold at 0.7, 120ms release.
  g_env.configMs(SR, 8.0, 40.0, 0.7, 120.0);
  redraw("-");
}

void loop() {
#ifdef SYNTH_USB_MIDI
  usbMidi.read();
#endif
#ifdef SYNTH_BLE_MIDI
  MIDI.read();
#endif
  cardputer::update();

  // --- note edges: a fresh press (re)triggers the voice (last-note priority) ---
  for (char c : cardputer::keysJustPressed()) {
    int s = synth::keyToSemitone(c);
    if (s >= 0) {
      noteOn(s);
      g_gateKey = c;
      int midiNote = synth::noteToMidi(s, g_octave);
#ifdef SYNTH_USB_MIDI
      usbMidi.sendNoteOn(midiNote, 100, 1);
#endif
#ifdef SYNTH_BLE_MIDI
      MIDI.sendNoteOn(midiNote, 100, 1);
#endif
      char label[8];
      snprintf(label, sizeof(label), "%s%d", synth::semitoneName(s), g_octave);
      redraw(label);
      continue;
    }
    // octave controls
    if (c == '[' || c == ';') {
      if (g_octave > 1) g_octave--;
      redraw("-");
    } else if (c == ']' || c == '\'') {
      if (g_octave < 7) g_octave++;
      redraw("-");
    }
    // volume controls
    else if (c == '-') {
      g_vol = (g_vol >= 16) ? g_vol - 16 : 0;
      cardputer::volume(g_vol);
      redraw("-");
    } else if (c == '=') {
      g_vol = (g_vol <= 239) ? g_vol + 16 : 255;
      cardputer::volume(g_vol);
      redraw("-");
    }
    // waveform select
    else if (c == '1') { g_wave = synth::WaveShape::Sine;   redraw("-"); }
    else if (c == '2') { g_wave = synth::WaveShape::Saw;    redraw("-"); }
    else if (c == '3') { g_wave = synth::WaveShape::Square; redraw("-"); }
    else if (c == '4') { g_wave = synth::WaveShape::Tri;    redraw("-"); }
  }

  // --- gate release: the held note key let go -> enter release tail ---
  if (g_gateKey && !heldContains(cardputer::keysHeld(), g_gateKey)) {
    int midiNote = synth::noteToMidi(synth::keyToSemitone(g_gateKey), g_octave);
    g_env.gateOff();
#ifdef SYNTH_USB_MIDI
    usbMidi.sendNoteOff(midiNote, 0, 1);
#endif
#ifdef SYNTH_BLE_MIDI
    MIDI.sendNoteOff(midiNote, 0, 1);
#endif
    g_gateKey = 0;
  }

  // --- IMU: sample gyro → amp scale, vol, pitch bend ---
  {
    cardputer::Imu raw;
    if (cardputer::imuRead(raw)) {
      float cgx = raw.gx - g_calib.gx0;
      float cgy = raw.gy - g_calib.gy0;
      float cgz = raw.gz - g_calib.gz0;
      auto r = synth::mapImu(cgx, cgy, cgz);  // dead-zone applied inside mapImu
      g_ampScale  = r.ampScale;
      g_bendRatio = std::pow(2.0, r.pitchBend / 12.0);  // semitones → freq ratio
      if (r.vol != g_vol) { g_vol = r.vol; cardputer::volume(g_vol); }
#if defined(SYNTH_USB_MIDI) || defined(SYNTH_BLE_MIDI)
      // Controller flood guard: only transmit when a value changes.
      uint8_t cc1 = (uint8_t)((r.ampScale - 0.2f) / 0.8f * 127.0f);
      uint8_t cc7 = (uint8_t)(r.vol / 2);
      // 14-bit pitch bend centered at 0: full gz → ±8191.
      int bend = (int)(r.pitchBend / synth::IMU_BEND_SEMITONES * 8191.0f);
      if (bend < -8192) bend = -8192; else if (bend > 8191) bend = 8191;
      if (cc1 != g_cc1_last || cc7 != g_cc7_last || bend != g_pb_last) {
        g_cc1_last = cc1; g_cc7_last = cc7; g_pb_last = bend;
#ifdef SYNTH_USB_MIDI
        usbMidi.sendControlChange(1, cc1, 1);
        usbMidi.sendControlChange(7, cc7, 1);
        usbMidi.sendPitchBend(bend, 1);
#endif
#ifdef SYNTH_BLE_MIDI
        MIDI.sendControlChange(1, cc1, 1);
        MIDI.sendControlChange(7, cc7, 1);
        MIDI.sendPitchBend(bend, 1);
#endif
      }
#endif
    }
  }

  // --- keep the speaker fed while the voice is sounding (incl. release tail) ---
  while (g_env.active() && M5.Speaker.isPlaying(0) < 2) {
    renderChunk();
  }

  delay(1);
}
