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
//   IMU (gyro)      - tilt gx → amp scale, gy → vol, gz → release time
#include "cardputer_hw.h"
#include "notes.h"
#include "adsr.h"
#include "oscillator.h"
#include "imu_map.h"
#include <cmath>

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
static float           g_ampScale = 1.0f;  // gyro gx → amplitude multiplier

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
    g_phase += g_phaseInc;
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
  M5.Speaker.begin();
  cardputer::volume(g_vol);
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
  cardputer::update();

  // --- note edges: a fresh press (re)triggers the voice (last-note priority) ---
  for (char c : cardputer::keysJustPressed()) {
    int s = synth::keyToSemitone(c);
    if (s >= 0) {
      noteOn(s);
      g_gateKey = c;
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
    g_env.gateOff();
    g_gateKey = 0;
  }

  // --- IMU: sample gyro → amp scale, vol, release time ---
  {
    cardputer::Imu raw;
    if (cardputer::imuRead(raw)) {
      float cgx = raw.gx - g_calib.gx0;
      float cgy = raw.gy - g_calib.gy0;
      float cgz = raw.gz - g_calib.gz0;
      auto r = synth::mapImu(cgx, cgy, cgz);  // dead-zone applied inside mapImu
      g_ampScale = r.ampScale;
      if (r.vol != g_vol) { g_vol = r.vol; cardputer::volume(g_vol); }
      g_env.configMs(SR, 8.0, 40.0, 0.7, r.releaseMs);
    }
  }

  // --- keep the speaker fed while the voice is sounding (incl. release tail) ---
  while (g_env.active() && M5.Speaker.isPlaying(0) < 2) {
    renderChunk();
  }

  delay(1);
}
