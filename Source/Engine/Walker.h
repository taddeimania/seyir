// Walker.h — the melody generator. It walks a ladder of pitches (the scale
// laid out over a few octaves) one clock step at a time, the way a player
// improvises in a mode: mostly by step, keeping its direction for a while,
// leaning on the strong notes, ending phrases on a resting note, and over
// several phrases climbing away from the tonic and coming back (the seyir).
//
// DEJA VU borrows the idea from Mutable Instruments' Marbles: every step is
// remembered in a loop, and the knob sets how often a step is replayed from
// the loop instead of invented. All the way up, the loop is locked.
#pragma once

#include "Tuning.h"

#include <array>
#include <cstdint>

namespace seyir
{
constexpr int kMaxRungs = 256;
constexpr int kMaxLoop  = 64;

struct Rung
{
    double cents = 0.0;   // above the tonic; negative below it
    int degree = 0;
    int octave = 0;
    uint8_t use = kBoth;
    float weight = 0.5f;
    bool resting = false;
};

struct Ladder
{
    std::array<Rung, kMaxRungs> rungs {};
    int count = 0;
    int home = 0;          // the rung at the tonic

    // Lays the scale out from `low` to `high` cents around the tonic.
    void build (const Scale&, double low, double high);
    int nearest (double cents) const;
    const Rung& operator[] (int i) const { return rungs[(size_t) i]; }
};

enum class Ornament : uint8_t { None, Slide, Grace, Shake };

struct WalkSettings
{
    float density  = 0.6f;   // busier and shorter notes as it rises
    float leap     = 0.3f;   // how far a single move can jump
    float gravity  = 0.6f;   // pull of the strong notes
    float arc      = 0.5f;   // how strongly phrases climb and return
    float ornament = 0.3f;   // chance of a slide, grace note or shake
    float dejavu   = 0.0f;   // 0 = always new, 1 = locked loop
    int   phrase   = 6;      // notes per phrase
    int   loopSteps = 16;
};

struct Step
{
    enum Kind : uint8_t { Rest, Hold, Note };
    Kind kind = Rest;
    int rung = 0;              // Note: the rung to play
    int grace = -1;            // Grace: the rung it starts from
    Ornament ornament = Ornament::None;
    float velocity = 0.8f;
    int length = 1;            // Note: steps it's meant to last
    bool phraseEnd = false;    // Note: the last note of a phrase
    bool replayed = false;     // came from the loop
};

class Walker
{
public:
    explicit Walker (uint32_t seed = 0x5e7e1u);

    // Changes the ladder; the current note and the loop move to the nearest
    // pitches of the new one.
    void setLadder (const Ladder&);
    const Ladder& ladder() const noexcept { return lad; }

    Step step (const WalkSettings&);

    void restart();                // back to the tonic and the first phrase; keeps the loop
    void reseed (uint32_t seed);   // forget the loop and start over
    void startFrom (int rung);     // FOLLOW: the next step plays this rung and a phrase grows from it
    void stop();                   // let the current note go; the next step starts fresh

    int current() const noexcept { return cur; }
    int phraseNumber() const noexcept { return phraseIndex; }
    int loopPosition() const noexcept { return loopPos; }   // the step the loop plays next

private:
    Step generate (const WalkSettings&);
    int choose (const WalkSettings&, bool cadence);
    float rnd();
    int remap (int rung, const Ladder& from) const;

    Ladder lad;
    uint32_t state;

    int cur = -1, last = -1, dir = 1;
    int notesInPhrase = 0, holdLeft = 0, restLeft = 0, phraseIndex = 0;
    int pendingStart = -1;

    std::array<Step, kMaxLoop> loop {};
    std::array<bool, kMaxLoop> loopValid {};
    int loopPos = 0;
};
} // namespace seyir
