#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "Settings.h"
#include "UI/Views.h"
#include "UI/Widgets.h"

// The panel is laid out in a fixed design space and scaled to fit the window,
// as on Beadwork and Dicey.
class SeyirPanel final : public juce::Component, private juce::Timer
{
public:
    static constexpr int designW = 1500;
    static constexpr int designH = 960;

    explicit SeyirPanel (SeyirProcessor&);
    ~SeyirPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

    std::function<void (bool)> onTooltips;
    void syncTooltips();

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;

    void addKnob (seyir::ui::Knob&, const char* id, const juce::String& caption,
                  juce::Rectangle<int>, const juce::String& tip);
    void addToggle (seyir::ui::PanelButton&, const char* id, juce::Rectangle<int>, const juce::String& tip);
    void addButton (seyir::ui::PanelButton&, juce::Rectangle<int>, const juce::String& tip,
                    std::function<void (const juce::ModifierKeys&)> click);
    void addChoice (seyir::ui::ChoiceDisplay&, const char* id, const juce::String& caption,
                    juce::Rectangle<int>, const juce::String& tip);
    void chooseParam (juce::Component& target, const char* id);

    void openModeMenu();
    void openJinsMenu (bool upperJins);
    void stepMode (int delta);
    void loadScalaFile();

    SeyirProcessor& proc;
    seyir::ui::PanelLookAndFeel lnf;
    seyir::Settings settings;

    // Header
    seyir::ui::ChoiceDisplay modeChoice;
    seyir::ui::PanelButton prevBtn { "<" }, nextBtn { ">" }, tipsBtn { "TIPS" }, panicBtn { "ALL OFF" };

    // Scale
    seyir::ui::LadderView ladder { proc };
    seyir::ui::ChoiceDisplay lowerChoice, upperChoice, tonicChoice;
    seyir::ui::Knob fineKnob, rangeKnob;
    seyir::ui::PanelButton scalaBtn { "LOAD .SCL" }, resetBtn { "RESET NOTES" };

    // Path
    seyir::ui::PathView path { proc };

    // Walk
    seyir::ui::PanelButton walkBtn { "WALK" }, followBtn { "FOLLOW" }, playBtn { "PLAY" },
                           runBtn { "RUN" }, newBtn { "NEW" };
    seyir::ui::ChoiceDisplay rateChoice, loopChoice, keysChoice;
    seyir::ui::Knob densityKnob, leapKnob, gravityKnob, arcKnob, phraseKnob, ornamentKnob, dejavuKnob;
    seyir::ui::LoopLeds loopLeds { proc };

    // Voice
    seyir::ui::ChoiceDisplay voiceChoice;
    seyir::ui::Knob aKnob, bKnob, cKnob, decayKnob, colorKnob, motionKnob;

    // Shape
    seyir::ui::Knob glideKnob, attackKnob, sustainKnob, toneKnob, resKnob, driveKnob, spaceKnob, levelKnob;

    // Drone
    seyir::ui::PanelButton droneBtn { "DRONE" };
    seyir::ui::ChoiceDisplay droneNotesChoice, droneStyleChoice, droneOctChoice;
    seyir::ui::Knob droneLevelKnob, droneToneKnob;

    std::vector<std::pair<seyir::ui::PanelButton*, const char*>> toggles;
    std::vector<seyir::ui::ChoiceDisplay*> choices;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::unique_ptr<juce::FileChooser> chooser;

    int tooltipsApplied = -1;
    int shownVoice = -1, shownMode = -1;
    bool shownMaqam = true;
    juce::String lcdTop, lcdBottom, blurb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeyirPanel)
};

class SeyirEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SeyirEditor (SeyirProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SeyirPanel panel;
    std::unique_ptr<juce::TooltipWindow> tooltips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeyirEditor)
};
