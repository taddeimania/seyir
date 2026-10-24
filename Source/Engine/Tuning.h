// Tuning.h — Seyir's scales: where the notes are, which way they're used and
// which ones the melody leans on. No JUCE here, so the engine tests can run
// without a plugin host.
//
// A Scale is one period (usually an octave) of pitches in cents above the
// tonic. Each degree says whether it's used going up, going down or both, and
// how strongly the melody is drawn to it (its weight).
//
// Maqams are built the way they're taught: a lower jins (a 3-5 note cell) on
// the tonic and an upper jins on the ghammaz, the note where the two meet.
// Swapping the upper jins is how a maqam modulates.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace seyir
{
constexpr int kMaxDegrees = 48;

enum DegreeFlags : uint8_t
{
    kUp   = 1,   // used when the melody is rising
    kDown = 2,   // used when it's falling
    kBoth = kUp | kDown,
};

struct Degree
{
    double cents = 0.0;      // above the tonic, 0 <= cents < period
    uint8_t use = kBoth;
    float weight = 0.5f;     // 0 = never played, 1 = strongest pull
    bool resting = false;    // a note phrases like to end on
};

// Plain data so the audio thread can copy it under a spin lock without
// allocating.
struct Scale
{
    char name[48] {};
    double period = 1200.0;
    int count = 0;
    std::array<Degree, kMaxDegrees> degrees {};

    const Degree& operator[] (int i) const { return degrees[(size_t) i]; }
    Degree& operator[] (int i) { return degrees[(size_t) i]; }

    // Sorts by pitch and merges degrees closer than a cent.
    void normalise();
    void setName (const std::string&);
};

// ---------------------------------------------------------------------------
// Library
// ---------------------------------------------------------------------------
enum class Family : uint8_t { Arabic, Turkish, Raga, World, Reference, Custom };
const char* familyName (Family);

struct Jins
{
    const char* name;
    Family family;
    std::vector<double> cents;   // from the jins' own root, ascending, starts at 0
};

const std::vector<Jins>& ajnas();
int jinsIndex (const char* name, Family);
// The ajnas of one family, as indices into ajnas().
std::vector<int> ajnasOf (Family);

struct Segment
{
    const char* jins;
    double at;   // cents above the tonic where the jins sits
};

struct ModeDef
{
    const char* name;
    Family family;
    const char* blurb;            // one line for the panel

    // Maqam: a lower jins on the tonic and an upper one on the ghammaz.
    const char* lower = nullptr;
    const char* upper = nullptr;
    double ghammaz = 0.0;
    const char* upperDown = nullptr;   // the upper jins used going down, if different
    std::vector<double> tail;          // notes above the upper jins (cents)

    // Anything else: the notes going up and, if different, going down.
    std::vector<double> up;
    std::vector<double> down;

    std::vector<double> strong;   // degrees the melody leans on (cents), besides the tonic
    double period = 1200.0;
    int defaultTonic = 50;        // MIDI note (Live: D2)

    bool isMaqam() const { return lower != nullptr; }
};

const std::vector<ModeDef>& modes();
int numModes();

// Builds a mode with the given lower/upper ajnas (indices into ajnas(), -1 =
// the mode's own). For non-maqam modes the ajnas are ignored.
Scale buildScale (int modeIndex, int lower = -1, int upper = -1);

// Default lower / upper ajnas of a maqam (indices into ajnas()); -1 if none.
int defaultLower (int modeIndex);
int defaultUpper (int modeIndex);

// ---------------------------------------------------------------------------
// Scala (.scl) files
// ---------------------------------------------------------------------------
// Parses the text of a .scl file. Returns false (and sets `error`) when it
// can't be read. The last pitch of the file is the period.
bool parseScala (const std::string& text, Scale& out, std::string& error);

// Cents of a ratio or cents value as written in a .scl file ("3/2", "701.955",
// "2"). Returns false when it isn't one.
bool parseScalaPitch (const std::string& token, double& cents);

// Name of the nearest 12-TET note and the offset from it, e.g. "E -49".
// `tonicMidi` is the MIDI note of the tonic (C3 = 60 as in Live).
std::string nearestNoteName (int tonicMidi, double fineCents, double cents);
} // namespace seyir
