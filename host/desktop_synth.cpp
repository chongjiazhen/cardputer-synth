// Desktop audio harness — play the synth on a PC without flashing hardware.
// Reuses the pure DSP headers verbatim (oscillator/adsr/voice/filter/...); only
// the glue (audio out + keyboard in) is here, mirroring what main.cpp does on
// the Cardputer. Lets you iterate timbre/filter/sampler by ear in seconds
// instead of a 20 s flash cycle. See host/README.md.
//
// Build:  bash host/build.sh     (Windows/MinGW g++)
// Run:    host/desktop_synth [sample.wav]
//   sample.wav (optional) is loaded into the sampler buffer so grain/tape mode
//   is playable; without it, only the oscillators sound.
//
// LIMITATION: a terminal cannot see key *release*. Each key press gates a note
// that auto-releases after HOLD_MS; OS key-repeat refreshes it, so holding a key
// sustains (after the initial repeat delay). It's a dev harness, not a keybed.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "notes.h"
#include "oscillator.h"
#include "adsr.h"
#include "filter.h"
#include "lfo.h"
#include "modmatrix.h"
#include "sampler.h"
#include "voice.h"
#include "voicealloc.h"
#include "mixer.h"

#include <conio.h>      // _kbhit / _getch (Windows/MinGW)
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <vector>

using namespace synth;

static double     g_sr      = 48000.0;         // set from the audio device
static const int  N_VOICES  = 6;
static Voice      g_voices[N_VOICES];
static int        g_noteAge = 0;
static WaveShape  g_wave    = WaveShape::Sine;
static int        g_octave  = 4;
static float      g_cutoff  = 4000.0f;         // Hz; ',' / '.' adjust
static double     g_aliveUntil[N_VOICES] = {}; // ms deadline for key-repeat sustain

// Sampler
static std::vector<int16_t> g_sampleBuf;
static int   g_sampleLen    = 0;
static bool  g_samplerMode  = false;
static bool  g_samplerGrain = false;

static const double HOLD_MS = 500.0;           // note length per press (see header)

static double nowMs() {
  using namespace std::chrono;
  return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

// --- audio thread: sum voices → soft clip → stereo float ---
static void dataCallback(ma_device*, void* out, const void*, ma_uint32 frames) {
  float* o = (float*)out;
  bool smpl = g_samplerMode && g_sampleLen > 0;
  for (ma_uint32 i = 0; i < frames; i++) {
    float bus = 0.0f;
    for (int v = 0; v < N_VOICES; v++)
      bus += smpl
        ? voiceSampleBuf(g_voices[v], g_sampleBuf.data(), g_sampleLen,
                         1.0f, 1.0f, g_sr, g_samplerGrain)
        : voiceSample(g_voices[v], g_wave, 1.0f, 1.0f, g_sr);
    float s = softClip(bus);
    o[2 * i] = s;          // L
    o[2 * i + 1] = s;      // R
  }
}

// --- note-on: fresh voice, or refresh an existing one held via key-repeat ---
static void keyNoteOn(char key, int semitone) {
  int idx = findVoiceByKey(g_voices, N_VOICES, key);
  if (idx >= 0 && g_voices[idx].active) { g_aliveUntil[idx] = nowMs() + HOLD_MS; return; }

  int midi = noteToMidi(semitone, g_octave);
  int i = allocVoice(g_voices, N_VOICES, ++g_noteAge);
  Voice& v = g_voices[i];
  v.active = true; v.key = key; v.midi = midi; v.phase = 0.0f;
  v.phaseInc   = TWO_PI_F * (float)midiToHz(midi) / g_sr;
  v.vel        = 1.0f;
  v.samplePos  = 0.0f;
  v.sampleStep = (float)sampleStep(midi);
  v.env1.configMs(g_sr, 8.0, 40.0, 0.7, 120.0);
  v.env2.configMs(g_sr, 5.0, 80.0, 0.5, 200.0);
  v.env1.gateOn(); v.env2.gateOn();
  v.filterMode = FilterMode::LP;
  v.baseCutoff = g_cutoff;
  v.baseResonance = 0.707f;
  v.filter.reset(); v.filterCtr = 0;
  v.mod.clear();
  v.mod.set(ModSrc::Env2, ModDst::FilterCut, 1.0f);
  v.grain.reset(0.0f);
  g_aliveUntil[i] = nowMs() + HOLD_MS;
}

// --- release notes whose key-repeat deadline has passed ---
static void releaseExpired() {
  double t = nowMs();
  for (int i = 0; i < N_VOICES; i++) {
    Voice& v = g_voices[i];
    if (v.active && v.key && t > g_aliveUntil[i]) {
      v.env1.gateOff(); v.env2.gateOff(); v.key = 0;
    }
  }
}

static void status(const char* note) {
  const char* wname = g_samplerMode ? (g_samplerGrain ? "GRAN" : "TAPE") : shapeName(g_wave);
  std::printf("\r[%s] oct %d  cut %.0fHz  src %s  last %-4s   ",
              g_samplerMode ? "SMPL" : "OSC", g_octave, g_cutoff, wname, note);
  std::fflush(stdout);
}

static bool loadWav(const char* path) {
  ma_decoder_config cfg = ma_decoder_config_init(ma_format_s16, 1, (ma_uint32)g_sr);
  ma_decoder dec;
  if (ma_decoder_init_file(path, &cfg, &dec) != MA_SUCCESS) return false;
  g_sampleBuf.assign((size_t)g_sr, 0);                 // up to ~1 s
  ma_uint64 read = 0;
  ma_decoder_read_pcm_frames(&dec, g_sampleBuf.data(), (ma_uint64)g_sr, &read);
  ma_decoder_uninit(&dec);
  g_sampleLen = (int)read;
  return g_sampleLen > 0;
}

int main(int argc, char** argv) {
  ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
  cfg.playback.format   = ma_format_f32;
  cfg.playback.channels = 2;
  cfg.sampleRate        = 48000;
  cfg.dataCallback      = dataCallback;

  ma_device device;
  if (ma_device_init(nullptr, &cfg, &device) != MA_SUCCESS) {
    std::printf("audio init failed\n"); return 1;
  }
  g_sr = device.sampleRate;                            // actual device rate

  if (argc > 1) {
    if (loadWav(argv[1])) { g_samplerMode = true;
      std::printf("loaded %s (%d samples) → sampler on\n", argv[1], g_sampleLen); }
    else std::printf("could not load %s (osc only)\n", argv[1]);
  }

  ma_device_start(&device);
  std::printf("desktop_synth @ %.0f Hz. keys: z x c v b n m / s d g h j = notes,\n"
              "  1-4 wave, [ ] octave, , . cutoff, 5 sampler, 8 tape/grain, q quit\n", g_sr);
  status("-");

  bool run = true;
  while (run) {
    while (_kbhit()) {
      char c = (char)_getch();
      int semi = keyToSemitone(c);
      if (semi >= 0) { keyNoteOn(c, semi); status(semitoneName(semi)); continue; }
      switch (c) {
        case '1': g_wave = WaveShape::Sine;   g_samplerMode = false; break;
        case '2': g_wave = WaveShape::Saw;    g_samplerMode = false; break;
        case '3': g_wave = WaveShape::Square; g_samplerMode = false; break;
        case '4': g_wave = WaveShape::Tri;    g_samplerMode = false; break;
        case '[': if (g_octave > 1) g_octave--; break;
        case ']': if (g_octave < 7) g_octave++; break;
        case ',': g_cutoff *= 0.8f; if (g_cutoff < 120.f) g_cutoff = 120.f; break;
        case '.': g_cutoff /= 0.8f; if (g_cutoff > 16000.f) g_cutoff = 16000.f; break;
        case '5': if (g_sampleLen > 0) g_samplerMode = true; break;
        case '8': g_samplerGrain = !g_samplerGrain; break;
        case 'q': case 27: run = false; break;
        default: break;
      }
      status("-");
    }
    releaseExpired();
    ma_sleep(5);
  }

  ma_device_uninit(&device);
  std::printf("\nbye\n");
  return 0;
}
