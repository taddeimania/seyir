// Views.h — the two displays only Seyir has: the LADDER (one period of the
// scale against the piano's twelve notes, which you can play, thin out and
// retune) and the PATH (the melody's shape over the last few seconds).
#pragma once

#include "../PluginProcessor.h"
#include "Widgets.h"

namespace seyir::ui
{
class LadderView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LadderView (SeyirProcessor&);

    void refresh();   // from the panel's timer
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plot() const;
    float xOf (double cents) const;
    float barTop (float weight) const;
    int degreeAt (juce::Point<float>) const;

    SeyirProcessor& proc;
    int lit = -1, hover = -1, dragging = -1;
    float dragWeight = 0.0f;
    double dragCents = 0.0;
    bool retuning = false;
    uint32_t shownHash = 0;
};

class PathView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PathView (SeyirProcessor&);

    void refresh();
    void paint (juce::Graphics&) override;
    void clear() { notes.clear(); repaint(); }

private:
    SeyirProcessor& proc;
    std::vector<TrailNote> notes;
    double now = 0.0;
};

// A row of LEDs for the DEJA VU loop.
class LoopLeds final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LoopLeds (SeyirProcessor& p) : proc (p) {}
    void refresh();
    void paint (juce::Graphics&) override;

private:
    SeyirProcessor& proc;
    int shownPos = -1, shownLen = 0;
    float shownAmount = -1.0f;
};
} // namespace seyir::ui
