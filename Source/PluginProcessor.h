#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

#include "Engine/Tuning.h"
#include "Engine/Walker.h"
#include "Params.h"
#include "Synth/Synth.h"

// A note the panel draws in the PATH strip.
struct TrailNote
{
    double cents = 0.0;     // above the tonic
    int degree = 0;
    bool phraseEnd = false;
    bool replayed = false;
    double time = 0.0;      // seconds, on the processor's clock
};

// Per-degree changes made on the ladder: how strongly each degree pulls, and
// how far it's been retuned. They belong to one scale; changing the mode or a
// jins starts again from the scale's own values.
struct DegreeEdits
{
    juce::String scaleKey;
    std::array<float, seyir::kMaxDegrees>  weight;   // < 0: the scale's own
    std::array<double, seyir::kMaxDegrees> cents {}; // retuning, in cents

    DegreeEdits() { weight.fill (-1.0f); }
    bool any() const;
};

class SeyirProcessor final : public juce::AudioProcessor,
                             private juce::Timer
{
public:
    SeyirProcessor();
    ~SeyirProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- parameters (message thread) -------------------------------------------
    void setParam (const juce::String& id, float plainValue);
    float getParam (const juce::String& id) const { return params.at (id)->load(); }
    int   geti (const char* id) const { return juce::roundToInt (getParam (id)); }

    // Picks a mode and goes back to its own ajnas.
    void selectMode (int mode);

    // --- the scale (message thread) -----------------------------------------------
    // The scale as played: the mode with its ajnas, then the ladder edits.
    const seyir::Scale& scale() const noexcept { return current; }
    const seyir::Scale& baseScale() const noexcept { return base; }
    const seyir::ModeDef* modeDef() const;   // null for a Scala file
    bool isMaqam() const;

    void setDegreeWeight (int degree, float weight);
    void setDegreeCents (int degree, double offset);   // retune by `offset` cents from the scale's own
    void resetDegree (int degree);
    void resetEdits();
    const DegreeEdits& edits() const noexcept { return degreeEdits; }

    bool loadScala (const juce::File&, juce::String& error);
    bool hasScala() const noexcept { return scala.count > 0; }
    juce::String scalaName() const { return juce::String::fromUTF8 (scala.name); }

    // --- playing from the panel ---------------------------------------------------------
    void audition (int degree, bool on, int octave = 0);
    void newSeed();      // NEW: forget the loop and start a fresh walk
    void panic();

    // --- read by the panel ----------------------------------------------------------------
    int  soundingDegree() const noexcept   { return uiDegree.load (std::memory_order_relaxed); }
    double soundingCents() const noexcept  { return uiCents.load (std::memory_order_relaxed); }
    bool hostPlaying() const noexcept      { return uiPlaying.load (std::memory_order_relaxed); }
    bool walking() const noexcept          { return uiWalking.load (std::memory_order_relaxed); }
    int  loopPosition() const noexcept     { return uiLoopPos.load (std::memory_order_relaxed); }
    double clockSeconds() const noexcept   { return uiClock.load (std::memory_order_relaxed); }
    // Notes played since the panel last asked, oldest first.
    void collectTrail (std::vector<TrailNote>& into);

    juce::AudioProcessorValueTreeState apvts;

private:
    void timerCallback() override;    // rebuilds the scale when the mode, a jins or an edit changes
    void rebuildScale();
    juce::String scaleKey() const;
    seyir::SynthSettings readSynth() const;
    seyir::WalkSettings readWalk() const;

    int keyToRung (int note, const seyir::Ladder&) const;
    void doStep (int offset, double stepSeconds);
    void noteOnRung (int offset, int rung, float velocity, int ornament, int graceRung, double stepSeconds,
                     const seyir::Ladder&, bool trail, bool phraseEnd, bool replayed);
    void noteOff (int offset);
    void pushTrail (const TrailNote&);

    std::map<juce::String, std::atomic<float>*> params;

    // --- scale, built on the message thread ---------------------------------------
    seyir::Scale base, current, scala;
    DegreeEdits degreeEdits;
    juce::String builtKey;
    uint32_t editVersion = 1, builtEditVersion = 0;

    juce::SpinLock scaleLock;
    seyir::Scale pending;
    std::atomic<uint32_t> pendingVersion { 0 };

    // --- audio thread ------------------------------------------------------------------
    seyir::Synth synth;
    seyir::Walker walker;
    seyir::Scale audioScale;
    seyir::Ladder walkLadder, playLadder;
    uint32_t audioVersion = 0;
    float ladderRange = -1.0f;
    std::vector<seyir::NoteEvent> events;

    double sampleRate = 44100.0;
    double freePpq = 0.0;
    double clock = 0.0;
    bool wasPlaying = false, wasRunning = false;
    bool noteSounding = false;
    int lastRung = -1;
    double lastKeyTime = -100.0;

    // Keys held (PLAY / FOLLOW), newest last.
    std::array<int, 16> held {};
    int numHeld = 0;

    // Panel -> audio thread.
    struct PanelEvent { int degree; int octave; bool on; };
    juce::AbstractFifo panelFifo { 64 };
    std::array<PanelEvent, 64> panelEvents {};
    juce::SpinLock panelWriteLock;
    std::atomic<bool> panicRequested { false }, reseedRequested { false };
    std::atomic<uint32_t> nextSeed { 1 };

    // Audio thread -> panel.
    juce::AbstractFifo trailFifo { 256 };
    std::array<TrailNote, 256> trailNotes {};
    std::atomic<int> uiDegree { -1 };
    std::atomic<double> uiCents { 0.0 };
    std::atomic<bool> uiPlaying { false }, uiWalking { false };
    std::atomic<int> uiLoopPos { 0 };
    std::atomic<double> uiClock { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeyirProcessor)
};
