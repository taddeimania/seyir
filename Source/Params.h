// Params.h — Seyir's host parameters. Everything on the panel is automatable,
// including the mode and the ajnas, so a modulation can live in an
// arrangement.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/Tuning.h"
#include "Synth/Synth.h"

namespace seyir::ids
{
// Tuning
inline constexpr const char* mode    = "mode";
inline constexpr const char* lower   = "lower";     // 0 = the maqam's own, else ajnas()[n - 1]
inline constexpr const char* upper   = "upper";
inline constexpr const char* tonic   = "tonic";
inline constexpr const char* fine    = "fine";
inline constexpr const char* range   = "range";

// Walk
inline constexpr const char* play     = "play";
inline constexpr const char* keys     = "keys";
inline constexpr const char* run      = "run";
inline constexpr const char* rate     = "rate";
inline constexpr const char* density  = "density";
inline constexpr const char* leap     = "leap";
inline constexpr const char* gravity  = "gravity";
inline constexpr const char* arc      = "arc";
inline constexpr const char* phrase   = "phrase";
inline constexpr const char* ornament = "ornament";
inline constexpr const char* dejavu   = "dejavu";
inline constexpr const char* loop     = "loop";

// Voice
inline constexpr const char* voice   = "voice";
inline constexpr const char* timbreA = "timbreA";
inline constexpr const char* timbreB = "timbreB";
inline constexpr const char* timbreC = "timbreC";
inline constexpr const char* decay   = "decay";
inline constexpr const char* color   = "color";

// Shape
inline constexpr const char* glide   = "glide";
inline constexpr const char* attack  = "attack";
inline constexpr const char* sustain = "sustain";
inline constexpr const char* motion  = "motion";
inline constexpr const char* tone    = "tone";
inline constexpr const char* res     = "res";
inline constexpr const char* drive   = "drive";
inline constexpr const char* space   = "space";
inline constexpr const char* level   = "level";

// Drone
inline constexpr const char* drone      = "drone";
inline constexpr const char* droneLevel = "droneLevel";
inline constexpr const char* droneTone  = "droneTone";
inline constexpr const char* droneNotes = "droneNotes";
inline constexpr const char* droneStyle = "droneStyle";
inline constexpr const char* droneOct   = "droneOct";

enum PlayMode { Walk, Follow, Play };

inline const juce::StringArray& playNames()       { static const juce::StringArray n { "WALK", "FOLLOW", "PLAY" }; return n; }
inline const juce::StringArray& keyNames()        { static const juce::StringArray n { "SNAP", "WHITE KEYS" }; return n; }
inline const juce::StringArray& rateNames()       { static const juce::StringArray n { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; return n; }
inline const juce::StringArray& loopNames()       { static const juce::StringArray n { "4", "8", "16", "32" }; return n; }
inline const juce::StringArray& droneNoteNames()  { static const juce::StringArray n { "TONIC", "+ FIFTH", "+ FOURTH", "+ OCTAVE" }; return n; }
inline const juce::StringArray& droneStyleNames() { static const juce::StringArray n { "HOLD", "PLUCK" }; return n; }

inline double rateBeats (int index)
{
    static const double beats[] { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    return beats[juce::jlimit (0, 5, index)];
}
inline int loopSteps (int index) { return 4 << juce::jlimit (0, 3, index); }

// Live prints MIDI 60 as C3.
inline juce::String midiName (int n)
{
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    n = juce::jlimit (0, 127, n);
    return juce::String (names[n % 12]) + juce::String (n / 12 - 2);
}

// Mode n == numModes() is the loaded Scala file.
inline int customMode() { return seyir::numModes(); }

inline juce::String modeName (int m)
{
    if (m >= customMode())
        return "Scala file";
    const auto& d = seyir::modes()[(size_t) juce::jlimit (0, seyir::numModes() - 1, m)];
    return juce::String::fromUTF8 (d.name);
}

inline juce::String jinsName (int v)
{
    if (v <= 0)
        return "Own";
    const auto& a = seyir::ajnas();
    return juce::String::fromUTF8 (a[(size_t) juce::jlimit (0, (int) a.size() - 1, v - 1)].name);
}

inline juce::String percent (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
} // namespace seyir::ids

namespace seyir
{
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto intText = [] (std::function<String (int)> f)
    {
        return AudioParameterIntAttributes().withStringFromValueFunction ([f] (int v, int) { return f (v); });
    };
    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return ids::percent (v); }));
    };

    const int numAjnas = (int) ajnas().size();

    // --- tuning ------------------------------------------------------------------
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::mode, 1 }, "Mode", 0, ids::customMode(), 1,
        intText ([] (int v) { return ids::modeName (v); })));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::lower, 1 }, "Lower Jins", 0, numAjnas, 0,
        intText ([] (int v) { return ids::jinsName (v); })));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::upper, 1 }, "Upper Jins", 0, numAjnas, 0,
        intText ([] (int v) { return ids::jinsName (v); })));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::tonic, 1 }, "Tonic", 36, 72, 50,
        intText ([] (int v) { return ids::midiName (v); })));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::fine, 1 }, "Fine", -50, 50, 0,
        intText ([] (int v) { return (v > 0 ? "+" : "") + String (v) + " ct"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::range, 1 }, "Range",
        NormalisableRange<float> (1.0f, 3.0f, 0.5f), 2.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " oct"; })));

    // --- walk --------------------------------------------------------------------------
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::play, 1 }, "Play Mode", ids::playNames(), ids::Walk));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::keys, 1 }, "Key Mapping", ids::keyNames(), 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::run, 1 }, "Run", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::rate, 1 }, "Rate", ids::rateNames(), 1));
    layout.add (pct (ids::density,  "Density",  0.6f));
    layout.add (pct (ids::leap,     "Leap",     0.25f));
    layout.add (pct (ids::gravity,  "Gravity",  0.6f));
    layout.add (pct (ids::arc,      "Arc",      0.5f));
    layout.add (pct (ids::ornament, "Ornament", 0.3f));
    layout.add (pct (ids::dejavu,   "Deja Vu",  0.0f));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::phrase, 1 }, "Phrase", 2, 16, 6,
        intText ([] (int v) { return String (v) + " notes"; })));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::loop, 1 }, "Loop", ids::loopNames(), 2));

    // --- voice -----------------------------------------------------------------------------
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::voice, 1 }, "Voice", 0, numVoiceModels() - 1,
        defaultVoiceModel(), intText ([] (int v)
        {
            return String (voiceModels()[(size_t) jlimit (0, numVoiceModels() - 1, v)].name);
        })));
    layout.add (pct (ids::timbreA, "Timbre A", 0.3f));
    layout.add (pct (ids::timbreB, "Timbre B", 0.5f));
    layout.add (pct (ids::timbreC, "Timbre C", 0.5f));
    layout.add (pct (ids::decay,   "Decay",    0.55f));
    layout.add (pct (ids::color,   "Color",    0.5f));

    // --- shape ------------------------------------------------------------------------------
    layout.add (pct (ids::glide,   "Glide",   0.0f));
    layout.add (pct (ids::attack,  "Attack",  0.0f));
    layout.add (pct (ids::sustain, "Sustain", 0.6f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::motion, 1 }, "Motion",
        NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const int p = roundToInt (v * 100.0f);
            return (p > 0 ? "+" : "") + String (p) + "%";
        })));
    layout.add (pct (ids::tone,    "Tone",    1.0f));
    layout.add (pct (ids::res,     "Resonance", 0.0f));
    layout.add (pct (ids::drive,   "Drive",   0.0f));
    layout.add (pct (ids::space,   "Space",   0.3f));
    layout.add (pct (ids::level,   "Level",   0.8f));

    // --- drone -------------------------------------------------------------------------------
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::drone, 1 }, "Drone", true));
    layout.add (pct (ids::droneLevel, "Drone Level", 0.35f));
    layout.add (pct (ids::droneTone,  "Drone Tone",  0.4f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::droneNotes, 1 }, "Drone Notes", ids::droneNoteNames(), 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::droneStyle, 1 }, "Drone Style", ids::droneStyleNames(), 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::droneOct, 1 }, "Drone Octave", -2, 0, -1,
        intText ([] (int v) { return v == 0 ? String ("0") : String (v); })));
    return layout;
}
} // namespace seyir
