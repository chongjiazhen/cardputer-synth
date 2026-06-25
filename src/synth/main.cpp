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
//   ,  / .          - low-pass cutoff down / up (toward open)
//   IMU accel tilt  - fwd/back → note velocity (latched at press); left/right
//                     → vibrato depth (LFO on pitch, → CC1)
//   IMU gyro twist  - gz → pitch bend (±2 semitones, self-centering wheel)
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
#include "filter.h"
#include "voice.h"
#include "voicealloc.h"
#include "mixer.h"
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
static constexpr int      AMP   = 14000;  // amplitude headroom — keep analog stage
                                          // (ES8311 + NS4150B + 1W spkr) out of
                                          // clipping; pure sine/tri reveal overdrive

static int16_t g_buf[CHUNK];

static int     g_octave = 4;              // base octave, clamp 1..7
static uint8_t g_vol    = 128;            // 0..255 (speaker output)

// Waveform state
static synth::WaveShape g_wave = synth::WaveShape::Sine;

// Low-pass filter (runtime, ',' / '.' keys). Default open = no tone change.
static constexpr double FC_MIN = 120.0;        // Hz
static constexpr double FC_MAX = SR * 0.5;     // Nyquist → "open"/bypass
static synth::OnePole g_filter;
static double         g_cutoff = FC_MAX;        // start open

// IMU state. Sensor split: accel tilt (persists) → velocity + vibrato;
// gyro rate (self-centers) → pitch bend.
static synth::ImuCalib g_calib;
static float  g_tiltFwd     = 0.0f;  // last calibrated fwd/back tilt (for latch)
static float  g_vibratoDepth = 0.0f; // 0..1, accel side-tilt → LFO depth
static double g_bendRatio    = 1.0;  // gyro gz → pitch-bend freq ratio
static double g_lfoPhase     = 0.0;  // vibrato LFO phase
static double g_lfoInc       = 0.0;  // set in setup from LFO_HZ / SR

// MIDI last-sent tracking — only transmit on change (prevents controller floods).
// vibrato→CC1 (mod), pitch bend→14-bit (center 0). Velocity rides each note-on.
#if defined(SYNTH_USB_MIDI) || defined(SYNTH_BLE_MIDI)
static uint8_t g_cc1_last = 0xFF;
static int     g_pb_last  = 0x7FFFFFFF;     // sentinel: force first send
#endif

// Voice state — polyphonic. Fixed pool; note-on allocates, note-off gates.
static constexpr int N_VOICES = 6;
static synth::Voice  g_voices[N_VOICES];
static int           g_noteAge = 0;       // monotonic allocation counter

// TWO_PI comes from Arduino.h (6.283185...).

static float g_lastVel = 1.0f;   // velocity of the most recent note-on (for display)

static void redraw(const char* lastNote) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  char cut[12];
  if (g_cutoff >= FC_MAX) snprintf(cut, sizeof(cut), "open");
  else                    snprintf(cut, sizeof(cut), "%dHz", (int)g_cutoff);
  M5.Display.printf("SYNTH\noct %d  vol %d\nlast: %s\nwave: %s\nvel: %.2f\nvib: %.2f\ncut: %s",
                    g_octave, g_vol, lastNote,
                    synth::shapeName(g_wave), g_lastVel, g_vibratoDepth, cut);
#ifdef SYNTH_USB_MIDI
  M5.Display.printf("\nUSB MIDI");
#endif
#ifdef SYNTH_BLE_MIDI
  M5.Display.printf("\nBLE MIDI");
#endif
}

// Allocate a voice for a key press at a semitone + velocity (0..127).
static void noteOn(char key, int semitone, uint8_t velocity) {
  int midi = synth::noteToMidi(semitone, g_octave);
  int i    = synth::allocVoice(g_voices, N_VOICES, ++g_noteAge);
  synth::Voice& v = g_voices[i];
  v.active   = true;
  v.key      = key;
  v.midi     = midi;
  v.phase    = 0.0f;
  v.phaseInc = synth::TWO_PI_F * (float)synth::midiToHz(midi) / SR;
  v.vel      = (float)velocity / 127.0f;
  v.env.configMs(SR, 8.0, 40.0, 0.7, 120.0);
  v.env.gateOn();
  g_lastVel  = v.vel;
}

// Render one CHUNK: sum all active voices, apply the global vibrato LFO + pitch
// bend (per voice) and the master low-pass + soft limiter (on the mix).
static void renderChunk() {
  for (size_t i = 0; i < CHUNK; i++) {
    float vibSemi  = g_vibratoDepth * synth::VIB_MAX_SEMITONES * sinf((float)g_lfoPhase);
    float vibRatio = 1.0f + vibSemi * 0.0577623f;
    float bus = 0.0f;
    for (int vch = 0; vch < N_VOICES; vch++)
      bus += synth::voiceSample(g_voices[vch], g_wave, (float)g_bendRatio, vibRatio);
    bus = (float)g_filter.process(bus);        // master low-pass (open = passthrough)
    g_buf[i] = synth::mixToInt16(bus, 1.0f, AMP);
    g_lfoPhase += g_lfoInc;
    if (g_lfoPhase >= TWO_PI) g_lfoPhase -= TWO_PI;
  }
  M5.Speaker.playRaw(g_buf, CHUNK, SR, false, 1, 0);   // channel 0, no repeat
}

// True while any voice is still sounding (gate or release tail).
static bool anyVoiceActive() {
  for (int i = 0; i < N_VOICES; i++) if (g_voices[i].active) return true;
  return false;
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
  // Calibrate IMU: snapshot at startup while device is still + flat. Captures
  // the accel tilt zero (ax/ay) and the gyro-z drift offset.
  {
    cardputer::Imu raw;
    if (cardputer::imuRead(raw)) g_calib = synth::calibrate(raw.ax, raw.ay, raw.gz);
  }
  g_lfoInc = TWO_PI * synth::LFO_HZ / SR;   // vibrato LFO step per sample
  g_filter.setCutoff(g_cutoff, SR);         // open by default
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
      // Latch velocity from the current fwd/back tilt (last IMU sample).
      uint8_t vel = synth::tiltVelocity(g_tiltFwd);
      noteOn(c, s, vel);
      int midiNote = synth::noteToMidi(s, g_octave);
#ifdef SYNTH_USB_MIDI
      usbMidi.sendNoteOn(midiNote, vel, 1);
#endif
#ifdef SYNTH_BLE_MIDI
      MIDI.sendNoteOn(midiNote, vel, 1);
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
    // filter cutoff: ',' down (more filtering), '.' up (toward open)
    else if (c == ',') {
      g_cutoff = g_cutoff * 0.7; if (g_cutoff < FC_MIN) g_cutoff = FC_MIN;
      g_filter.setCutoff(g_cutoff, SR); redraw("-");
    } else if (c == '.') {
      g_cutoff = g_cutoff / 0.7; if (g_cutoff > FC_MAX) g_cutoff = FC_MAX;
      g_filter.setCutoff(g_cutoff, SR); redraw("-");
    }
  }

  // --- gate release: any voice whose key is no longer held enters its tail ---
  {
    auto held = cardputer::keysHeld();
    for (int i = 0; i < N_VOICES; i++) {
      synth::Voice& v = g_voices[i];
      if (v.active && v.key && !heldContains(held, v.key)) {
        v.env.gateOff();
#ifdef SYNTH_USB_MIDI
        usbMidi.sendNoteOff(v.midi, 0, 1);
#endif
#ifdef SYNTH_BLE_MIDI
        MIDI.sendNoteOff(v.midi, 0, 1);
#endif
        v.key = 0;   // no longer gated; env releases, then voice frees itself
      }
    }
  }

  // --- IMU: accel tilt → velocity (latched on next note) + vibrato depth;
  //         gyro twist → pitch bend ---
  {
    cardputer::Imu raw;
    if (cardputer::imuRead(raw)) {
      g_tiltFwd  = raw.ay - g_calib.ay0;                 // fwd/back → velocity
      float side = raw.ax - g_calib.ax0;                 // left/right → vibrato
      float gz   = raw.gz - g_calib.gz0;                 // twist → pitch bend
      g_vibratoDepth = synth::tiltVibrato(side);
      float bendSemi = synth::gyroBend(gz);
      g_bendRatio    = std::pow(2.0, bendSemi / 12.0);   // semitones → freq ratio
#if defined(SYNTH_USB_MIDI) || defined(SYNTH_BLE_MIDI)
      // Controller flood guard: only transmit when a value changes.
      uint8_t cc1 = (uint8_t)(g_vibratoDepth * 127.0f);  // vibrato depth → mod
      // 14-bit pitch bend centered at 0: full gz → ±8191.
      int bend = (int)(bendSemi / synth::IMU_BEND_SEMITONES * 8191.0f);
      if (bend < -8192) bend = -8192; else if (bend > 8191) bend = 8191;
      if (cc1 != g_cc1_last || bend != g_pb_last) {
        g_cc1_last = cc1; g_pb_last = bend;
#ifdef SYNTH_USB_MIDI
        usbMidi.sendControlChange(1, cc1, 1);
        usbMidi.sendPitchBend(bend, 1);
#endif
#ifdef SYNTH_BLE_MIDI
        MIDI.sendControlChange(1, cc1, 1);
        MIDI.sendPitchBend(bend, 1);
#endif
      }
#endif
    }
  }

  // --- keep the speaker fed while the voice is sounding (incl. release tail) ---
  while (anyVoiceActive() && M5.Speaker.isPlaying(0) < 2) {
    renderChunk();
  }

  delay(1);
}
