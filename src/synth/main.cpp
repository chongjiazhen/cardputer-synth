// Synth — keyboard-driven tone generator on the Cardputer ADV. Press a note key
// (tracker layout, see notes.h) -> synthesize a short sine burst and play it out
// the 1W speaker via M5.Speaker.playRaw().
//
// Controls (Fn + key to wake keyboard):
//   z x c v b n m  - white keys C D E F G A B (current octave)
//   s d g h j       - black keys C# D# F# G# A#
//   [  / ]          - octave down / up (clamp 1..7); also ; / ' as fallback
//   -  / =          - volume down / up (step 16, 0..255)
#include "cardputer_hw.h"
#include "notes.h"

static constexpr uint32_t SR  = 16000;   // sample rate (matches recorder)
static constexpr int      MS  = 120;     // tone length, milliseconds
static constexpr size_t   N   = SR * MS / 1000;   // samples per burst

static int16_t g_buf[N];

static int     g_octave = 4;             // base octave, clamp 1..7
static uint8_t g_vol     = 128;          // 0..255

// Fill g_buf with a sine at hz, with a short linear fade in/out to kill clicks.
static void fillSine(double hz) {
  const double twoPiF = 2.0 * 3.14159265358979323846 * hz / SR;
  const size_t fade = SR / 200;          // ~5ms ramp each end
  for (size_t i = 0; i < N; i++) {
    double s = sin(twoPiF * i);
    double env = 1.0;
    if (i < fade)        env = (double)i / fade;
    else if (i > N - fade) env = (double)(N - i) / fade;
    g_buf[i] = (int16_t)(s * env * 28000);   // headroom below int16 max
  }
}

static void redraw(const char* lastNote) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.printf("SYNTH\noct %d  vol %d\n\nlast: %s", g_octave, g_vol, lastNote);
}

static void playKey(int semitone) {
  int midi = synth::noteToMidi(semitone, g_octave);
  fillSine(synth::midiToHz(midi));
  M5.Speaker.playRaw(g_buf, N, SR);
  // non-blocking: let it ring while loop keeps polling keys
}

void setup() {
  cardputer::begin();
  M5.Speaker.begin();
  cardputer::volume(g_vol);
  redraw("-");
}

void loop() {
  cardputer::update();
  for (char c : cardputer::keysJustPressed()) {
    int s = synth::keyToSemitone(c);
    if (s >= 0) {
      playKey(s);
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
  }
  delay(2);
}
