// Synth.h — Seyir's sound: Mutable Instruments' Plaits and Rings for the
// melody, two more Plaits voices for the drone, and a small chain to shape
// it (glide, envelope, filter, drive, and Rings' reverb for space).
//
// Everything runs at 48 kHz, the rate the modules were designed for, and is
// resampled to the host's rate on the way out. No JUCE in here: this file is
// compiled with the Mutable code (which needs -DTEST on desktop).
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace seyir
{
enum class VoiceKind : uint8_t { Plaits, Rings };

struct VoiceModel
{
    const char* name;
    VoiceKind kind;
    int engine;            // Plaits engine number, or Rings resonator model
    const char* a;         // captions for the three timbre knobs
    const char* b;
    const char* c;
    const char* decay;
    bool usesColor;        // Plaits' low-pass gate colour applies
    float gain;            // level trim, so the models sit at similar loudness
};

const std::vector<VoiceModel>& voiceModels();
int numVoiceModels();
int defaultVoiceModel();

enum class DroneNotes : uint8_t { Tonic, Fifth, Fourth, Octave };
enum class DroneStyle : uint8_t { Hold, Pluck };

struct SynthSettings
{
    int   voice = 0;
    float a = 0.5f, b = 0.5f, c = 0.5f;
    float decay = 0.5f, color = 0.5f;

    float glide = 0.0f;      // 0..1 -> 0..600 ms portamento
    float attack = 0.0f;     // 0..1 -> 1 ms .. 2 s
    float sustain = 0.6f;    // 0..1
    float motion = 0.0f;     // -1..1: envelope to timbre (Plaits)
    float tone = 1.0f;       // low-pass cutoff, 0..1
    float res = 0.0f;
    float drive = 0.0f;
    float space = 0.25f;
    float level = 0.8f;

    bool  droneOn = true;
    float droneLevel = 0.35f;
    float droneTone = 0.4f;
    DroneNotes droneNotes = DroneNotes::Fifth;
    DroneStyle droneStyle = DroneStyle::Hold;
    double droneTonic = 38.0;   // MIDI note, fractional
    bool  droneActive = true;   // the processor fades the drone out when nothing's going on

    double bpm = 100.0;
};

struct NoteEvent
{
    enum Type : uint8_t { On, Off, AllOff };
    int offset = 0;               // host samples into the block
    Type type = On;
    double pitch = 60.0;          // MIDI note, fractional
    double grace = -1.0;          // Grace: the pitch it flicks in from (< 0: none)
    double shake = 0.0;           // Shake: depth in semitones
    int ornament = 0;             // 0 none, 1 slide, 2 grace, 3 shake
    float stepSeconds = 0.125f;   // length of a clock step, for ornament timing
    float velocity = 0.8f;
};

class Synth
{
public:
    Synth();
    ~Synth();

    void prepare (double hostSampleRate);
    void reset();

    // Renders `n` host samples. Events must be sorted by offset.
    void process (float* left, float* right, int n, const SynthSettings&,
                  const NoteEvent* events, int numEvents);

    // The pitch the melody voice is sounding (MIDI note), or < 0 when silent.
    double soundingPitch() const noexcept { return sounding; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    double sounding = -1.0;
};
} // namespace seyir
