#include "PluginEditor.h"

using namespace seyir;
using namespace seyir::ui;
using juce::Rectangle;
using juce::String;

namespace
{
// Section boxes (design space).
const Rectangle<float> kScaleBox { 12.0f,   84.0f, 1476.0f, 296.0f };
const Rectangle<float> kPathBox  { 12.0f,  396.0f, 1476.0f, 136.0f };
const Rectangle<float> kWalkBox  { 12.0f,  548.0f,  620.0f, 400.0f };
const Rectangle<float> kVoiceBox { 644.0f, 548.0f,  404.0f, 400.0f };
const Rectangle<float> kShapeBox { 1060.0f, 548.0f, 428.0f, 232.0f };
const Rectangle<float> kDroneBox { 1060.0f, 796.0f, 428.0f, 152.0f };
const Rectangle<float> kLcd      { 28.0f,  568.0f,  300.0f,  72.0f };
const Rectangle<float> kBlurb    { 1156.0f, 266.0f, 316.0f,  56.0f };
} // namespace

// ===========================================================================
SeyirPanel::SeyirPanel (SeyirProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    lnf.setColour (juce::ScrollBar::thumbColourId, colours::inkDim);
    setSize (designW, designH);

    // --- header ------------------------------------------------------------------
    modeChoice.getText = [this]
    {
        const int m = proc.geti (ids::mode);
        if (m >= ids::customMode())
            return "Scala  " + proc.scalaName();
        String name = String::fromUTF8 (proc.scale().name);
        return name + "   " + String (familyName (modes()[(size_t) m].family)).toLowerCase();
    };
    modeChoice.fontH = 18.0f;
    modeChoice.onOpen = [this] { openModeMenu(); };
    modeChoice.setTooltip ("The mode the melody walks in. Changing it keeps the tonic, so you can "
                           "hear one mode after another over the same drone.");
    addAndMakeVisible (modeChoice);
    modeChoice.setBounds (636, 14, 420, 46);
    choices.push_back (&modeChoice);

    addButton (prevBtn, { 590, 16, 40, 42 }, "Previous mode.", [this] (const juce::ModifierKeys&) { stepMode (-1); });
    addButton (nextBtn, { 1062, 16, 40, 42 }, "Next mode.", [this] (const juce::ModifierKeys&) { stepMode (1); });
    prevBtn.setFontHeight (18.0f);
    nextBtn.setFontHeight (18.0f);

    addButton (tipsBtn, { 1120, 20, 64, 34 }, "Show or hide these tips (remembered for every Seyir).",
               [this] (const juce::ModifierKeys&)
               {
                   settings.setTooltipsEnabled (! settings.tooltipsEnabled());
                   syncTooltips();
               });
    tipsBtn.setFontHeight (11.0f);
    addButton (panicBtn, { 1196, 20, 110, 34 }, "Stop the melody and every note.",
               [this] (const juce::ModifierKeys&) { proc.panic(); });

    // --- scale -------------------------------------------------------------------------
    addAndMakeVisible (ladder);
    ladder.setBounds (24, 100, 1112, 262);

    addChoice (lowerChoice, ids::lower, "LOWER JINS", { 1156, 104, 152, 54 },
               "The cell the maqam is built on, from the tonic up to the ghammaz.");
    addChoice (upperChoice, ids::upper, "UPPER JINS", { 1320, 104, 152, 54 },
               "The cell above the ghammaz. Swap it to modulate, the way a player does. Works mid-phrase.");
    lowerChoice.onOpen = [this] { openJinsMenu (false); };
    upperChoice.onOpen = [this] { openJinsMenu (true); };
    for (auto* c : { &lowerChoice, &upperChoice })
        c->getText = [this, c]
        {
            if (! proc.isMaqam())
                return String ("--");
            const auto* d = proc.modeDef();
            const int v = proc.geti (c == &lowerChoice ? ids::lower : ids::upper);
            return v == 0 ? String::fromUTF8 (c == &lowerChoice ? d->lower : d->upper) : ids::jinsName (v);
        };

    addChoice (tonicChoice, ids::tonic, "TONIC", { 1156, 176, 152, 54 },
               "The note the scale and the drone are built on. Live names: middle C is C3.");
    addKnob (fineKnob, ids::fine, "FINE", { 1318, 166, 76, 96 }, "Move the tonic up or down by cents.");
    fineKnob.bipolar = true;
    addKnob (rangeKnob, ids::range, "RANGE", { 1398, 166, 76, 96 },
             "How high the melody can climb, in octaves above the tonic. It always reaches a fourth below.");

    addButton (scalaBtn, { 1156, 330, 152, 34 }, "Load a Scala (.scl) tuning file. Thousands are free at "
               "huygens-fokker.org/scala.", [this] (const juce::ModifierKeys&) { loadScalaFile(); });
    addButton (resetBtn, { 1320, 330, 152, 34 }, "Undo every change made on the ladder.",
               [this] (const juce::ModifierKeys&) { proc.resetEdits(); });
    scalaBtn.setFontHeight (11.0f);
    resetBtn.setFontHeight (11.0f);

    // --- path ----------------------------------------------------------------------------
    addAndMakeVisible (path);
    path.setBounds (24, 410, 1452, 110);

    // --- walk ----------------------------------------------------------------------------
    auto modeButton = [this] (PanelButton& b, int value, Rectangle<int> r, const String& tip)
    {
        addButton (b, r, tip, [this, value] (const juce::ModifierKeys&) { proc.setParam (ids::play, (float) value); });
    };
    modeButton (walkBtn, ids::Walk, { 344, 568, 90, 34 },
                "The melody walks on its own while Live plays (or with RUN).");
    modeButton (followBtn, ids::Follow, { 440, 568, 90, 34 },
                "Hold a key: the melody starts on that note and keeps improvising until you let go.");
    modeButton (playBtn, ids::Play, { 536, 568, 82, 34 },
                "Your keys play the scale directly. SNAP: each key plays the nearest note of the scale. "
                "WHITE KEYS: C3 is the tonic and each white key is the next note.");
    addToggle (runBtn, ids::run, { 344, 608, 90, 34 }, "Walk without Live's transport, at Live's tempo.");
    addButton (newBtn, { 440, 608, 90, 34 }, "Forget the loop and start a fresh walk from the tonic.",
               [this] (const juce::ModifierKeys&) { proc.newSeed(); path.clear(); });

    addChoice (rateChoice, ids::rate, "STEP", { 28, 656, 120, 52 }, "Length of a step. T = triplet.");
    addChoice (keysChoice, ids::keys, "KEYS", { 160, 656, 160, 52 }, "How your keys map onto the scale (PLAY and FOLLOW).");
    addChoice (loopChoice, ids::loop, "LOOP", { 332, 656, 90, 52 }, "How many steps DEJA VU remembers.");

    addKnob (densityKnob, ids::density, "DENSITY", { 28, 720, 110, 100 },
             "Busier and shorter notes as you turn it up; longer notes and more space as you turn it down.");
    addKnob (leapKnob, ids::leap, "LEAP", { 146, 720, 110, 100 }, "How far one move can jump. Low: step by step.");
    addKnob (gravityKnob, ids::gravity, "GRAVITY", { 264, 720, 110, 100 },
             "How strongly the melody is drawn to the strong notes (tall bars on the ladder), "
             "and how surely phrases end on them.");
    addKnob (arcKnob, ids::arc, "ARC", { 382, 720, 110, 100 },
             "The seyir: over several phrases the melody climbs away from the tonic and comes back. "
             "Turn up for a clearer shape.");
    addKnob (phraseKnob, ids::phrase, "PHRASE", { 500, 720, 110, 100 }, "Notes per phrase.");
    addKnob (ornamentKnob, ids::ornament, "ORNAMENT", { 28, 834, 110, 100 },
             "Chance of an ornament on a note: a slide from the last note, a grace note flicked in "
             "from above, or a shake.");
    addKnob (dejavuKnob, ids::dejavu, "DEJA VU", { 146, 834, 110, 100 },
             "From Marbles: how often a step replays from the loop instead of being invented. "
             "All the way up locks the loop. Try swapping the upper jins while it's locked.");
    addAndMakeVisible (loopLeds);
    loopLeds.setBounds (270, 846, 346, 52);
    loopLeds.setTooltip ("The DEJA VU loop. Red: locked.");

    // --- voice --------------------------------------------------------------------------
    addChoice (voiceChoice, ids::voice, "MODEL", { 660, 568, 372, 52 },
               "Which Mutable Instruments model plays the melody: Plaits' synthesis engines or "
               "Rings' resonators.");
    voiceChoice.fontH = 17.0f;
    voiceChoice.onOpen = [this]
    {
        juce::PopupMenu m;
        const int cur = proc.geti (ids::voice);
        m.addSectionHeader ("PLAITS");
        for (int i = 0; i < numVoiceModels(); ++i)
        {
            const auto& v = voiceModels()[(size_t) i];
            if (i > 0 && v.kind != voiceModels()[(size_t) i - 1].kind)
                m.addSectionHeader ("RINGS");
            m.addItem (v.name, true, i == cur, [this, i] { proc.setParam (ids::voice, (float) i); });
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&voiceChoice));
    };
    addKnob (aKnob, ids::timbreA, "A", { 664, 636, 112, 108 }, "");
    addKnob (bKnob, ids::timbreB, "B", { 786, 636, 112, 108 }, "");
    addKnob (cKnob, ids::timbreC, "C", { 908, 636, 112, 108 }, "");
    addKnob (decayKnob, ids::decay, "DECAY", { 664, 760, 112, 108 },
             "How long notes ring. Also the envelope's decay and release.");
    addKnob (colorKnob, ids::color, "COLOR", { 786, 760, 112, 108 },
             "Plaits' low-pass gate: from a plucked, closing tone to an open, ringing one.");
    addKnob (motionKnob, ids::motion, "MOTION", { 908, 760, 112, 108 },
             "Lets each note's envelope move the B knob (Plaits), for timbres that open or close as they sound.");
    motionKnob.bipolar = true;

    // --- shape --------------------------------------------------------------------------
    addKnob (glideKnob, ids::glide, "GLIDE", { 1072, 564, 98, 100 }, "Slide from note to note.");
    addKnob (attackKnob, ids::attack, "ATTACK", { 1174, 564, 98, 100 }, "Fade each note in.");
    addKnob (sustainKnob, ids::sustain, "SUSTAIN", { 1276, 564, 98, 100 },
             "The level a held note settles to. Down for plucks, up for held notes.");
    addKnob (toneKnob, ids::tone, "TONE", { 1378, 564, 98, 100 }, "Low-pass filter. All the way up is open.");
    addKnob (resKnob, ids::res, "RES", { 1072, 670, 98, 100 }, "Filter resonance.");
    addKnob (driveKnob, ids::drive, "DRIVE", { 1174, 670, 98, 100 }, "Soft saturation before the filter.");
    addKnob (spaceKnob, ids::space, "SPACE", { 1276, 670, 98, 100 }, "Reverb, from Rings. Melody and drone both.");
    addKnob (levelKnob, ids::level, "LEVEL", { 1378, 670, 98, 100 }, "Melody volume.");

    // --- drone --------------------------------------------------------------------------
    addToggle (droneBtn, ids::drone, { 1076, 816, 96, 34 },
               "A drone on the tonic, so every interval has something to sit against. It sounds while "
               "the melody plays and for a few seconds after you touch a key or the ladder.");
    addChoice (droneOctChoice, ids::droneOct, "OCTAVE", { 1076, 864, 96, 50 }, "The drone's octave, from the tonic.");
    addChoice (droneNotesChoice, ids::droneNotes, "NOTES", { 1186, 816, 124, 50 },
               "The tonic alone, or with a pure fifth, fourth or octave.");
    addChoice (droneStyleChoice, ids::droneStyle, "STYLE", { 1186, 874, 124, 50 },
               "HOLD: a steady harmonic drone (Plaits' additive engine). PLUCK: strings struck in a "
               "tanpura's cycle (Plaits' string engine), one per beat.");
    addKnob (droneLevelKnob, ids::droneLevel, "LEVEL", { 1318, 808, 80, 104 }, "Drone volume.");
    addKnob (droneToneKnob, ids::droneTone, "TONE", { 1400, 808, 80, 104 }, "Darker to brighter.");

    startTimerHz (30);
    timerCallback();
}

SeyirPanel::~SeyirPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ---------------------------------------------------------------------------
void SeyirPanel::addKnob (Knob& k, const char* id, const String& caption, Rectangle<int> r, const String& tip)
{
    k.caption = caption;
    k.setTooltip (tip);
    if (auto* param = proc.apvts.getParameter (id))
        k.format = [param] (double v) { return param->getText (param->convertTo0to1 ((float) v), 32); };
    addAndMakeVisible (k);
    k.setBounds (r);
    attachments.push_back (std::make_unique<SliderAttachment> (proc.apvts, id, k));
}

void SeyirPanel::addToggle (PanelButton& b, const char* id, Rectangle<int> r, const String& tip)
{
    addButton (b, r, tip, [this, id] (const juce::ModifierKeys&)
    {
        proc.setParam (id, proc.getParam (id) > 0.5f ? 0.0f : 1.0f);
    });
    toggles.push_back ({ &b, id });
}

void SeyirPanel::addButton (PanelButton& b, Rectangle<int> r, const String& tip,
                            std::function<void (const juce::ModifierKeys&)> click)
{
    b.setTooltip (tip);
    b.onClick = std::move (click);
    addAndMakeVisible (b);
    b.setBounds (r);
}

void SeyirPanel::addChoice (ChoiceDisplay& c, const char* id, const String& caption, Rectangle<int> r, const String& tip)
{
    c.caption = caption;
    c.setTooltip (tip);
    if (! c.getText)
        c.getText = [this, id]
        {
            if (auto* param = proc.apvts.getParameter (id))
                return param->getCurrentValueAsText();
            return String();
        };
    c.onOpen = [this, &c, id] { chooseParam (c, id); };
    addAndMakeVisible (c);
    c.setBounds (r);
    choices.push_back (&c);
}

void SeyirPanel::chooseParam (juce::Component& target, const char* id)
{
    auto* param = proc.apvts.getParameter (id);
    if (param == nullptr)
        return;
    const auto range = proc.apvts.getParameterRange (id);
    const int lo = juce::roundToInt (range.start), hi = juce::roundToInt (range.end);
    const int cur = juce::roundToInt (proc.getParam (id));

    juce::PopupMenu m;
    for (int v = lo; v <= hi; ++v)
    {
        const auto text = param->getText (param->convertTo0to1 ((float) v), 32);
        m.addItem (text, true, v == cur, [this, id, v] { proc.setParam (id, (float) v); });
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target));
}

// ---------------------------------------------------------------------------
void SeyirPanel::openModeMenu()
{
    juce::PopupMenu m;
    const int cur = proc.geti (ids::mode);
    const Family families[] { Family::Arabic, Family::Turkish, Family::Raga, Family::World, Family::Reference };
    for (auto f : families)
    {
        juce::PopupMenu sub;
        bool ticked = false;
        for (int i = 0; i < numModes(); ++i)
        {
            const auto& d = modes()[(size_t) i];
            if (d.family != f) continue;
            sub.addItem (String::fromUTF8 (d.name) + "   " + String::fromUTF8 (d.blurb), true, i == cur,
                         [this, i] { proc.selectMode (i); });
            ticked = ticked || i == cur;
        }
        m.addSubMenu (familyName (f), sub, true, nullptr, ticked);
    }
    m.addSeparator();
    if (proc.hasScala())
        m.addItem ("Scala file: " + proc.scalaName(), true, cur >= ids::customMode(),
                   [this] { proc.selectMode (ids::customMode()); });
    m.addItem ("Load a .scl file...", [this] { loadScalaFile(); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&modeChoice));
}

void SeyirPanel::openJinsMenu (bool upperJins)
{
    const auto* d = proc.modeDef();
    if (d == nullptr || ! d->isMaqam())
        return;
    const char* id = upperJins ? ids::upper : ids::lower;
    const int cur = proc.geti (id);

    juce::PopupMenu m;
    m.addItem (String ("Own (") + String::fromUTF8 (upperJins ? d->upper : d->lower) + ")", true, cur == 0,
               [this, id] { proc.setParam (id, 0.0f); });
    m.addSeparator();
    for (int j : ajnasOf (d->family))
    {
        const auto& a = ajnas()[(size_t) j];
        String cents;
        for (double c : a.cents) cents << (cents.isEmpty() ? "" : " ") << juce::roundToInt (c);
        m.addItem (String::fromUTF8 (a.name) + "   " + cents, true, cur == j + 1,
                   [this, id, j] { proc.setParam (id, (float) (j + 1)); });
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (upperJins ? &upperChoice : &lowerChoice));
}

void SeyirPanel::stepMode (int delta)
{
    const int count = numModes() + (proc.hasScala() ? 1 : 0);
    const int cur = juce::jmin (proc.geti (ids::mode), count - 1);
    proc.selectMode ((cur + delta + count) % count);
}

void SeyirPanel::loadScalaFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a Scala tuning", settings.scalaFolder(), "*.scl");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile())
            return;
        settings.setScalaFolder (f.getParentDirectory());
        String error;
        if (! proc.loadScala (f, error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load " + f.getFileName(), error);
    });
}

void SeyirPanel::syncTooltips()
{
    const bool on = settings.tooltipsEnabled();
    tipsBtn.setLit (on);
    if ((int) on != tooltipsApplied && onTooltips)
    {
        tooltipsApplied = (int) on;
        onTooltips (on);
    }
}

// ---------------------------------------------------------------------------
void SeyirPanel::timerCallback()
{
    ladder.refresh();
    path.refresh();
    loopLeds.refresh();
    for (auto* c : choices)
        c->refresh();
    for (auto& [b, id] : toggles)
        b->setLit (proc.getParam (id) > 0.5f);

    const int playMode = proc.geti (ids::play);
    walkBtn.setLit (playMode == ids::Walk);
    followBtn.setLit (playMode == ids::Follow);
    playBtn.setLit (playMode == ids::Play);

    // Jins pickers only mean something for a maqam.
    const bool maqam = proc.isMaqam();
    if (maqam != shownMaqam)
    {
        shownMaqam = maqam;
        lowerChoice.setAlpha (maqam ? 1.0f : 0.35f);
        upperChoice.setAlpha (maqam ? 1.0f : 0.35f);
    }

    // Knob captions follow the voice.
    const int v = proc.geti (ids::voice);
    if (v != shownVoice)
    {
        shownVoice = v;
        const auto& m = voiceModels()[(size_t) juce::jlimit (0, numVoiceModels() - 1, v)];
        aKnob.caption = m.a;
        bKnob.caption = m.b;
        cKnob.caption = m.c;
        decayKnob.caption = m.decay;
        const bool plaits = m.kind == VoiceKind::Plaits;
        aKnob.setTooltip (plaits ? "Plaits' HARMONICS knob for this engine." : "Rings' STRUCTURE knob: the resonator's "
                                   "harmonic structure (for sympathetic strings, how the extra strings are tuned).");
        bKnob.setTooltip (plaits ? "Plaits' TIMBRE knob for this engine." : "Rings' BRIGHTNESS knob.");
        cKnob.setTooltip (plaits ? "Plaits' MORPH knob for this engine." : "Rings' POSITION knob: where the string "
                                   "or plate is struck.");
        colorKnob.setAlpha (m.usesColor ? 1.0f : 0.35f);
        motionKnob.setAlpha (plaits ? 1.0f : 0.35f);
        for (auto* k : { &aKnob, &bKnob, &cKnob, &decayKnob })
            k->repaint();
        repaint (kVoiceBox.toNearestInt().expanded (2, 10));
    }

    // The LCD and the mode's blurb.
    String top = "--", bottom;
    const int deg = proc.soundingDegree();
    if (deg >= 0 && deg < proc.scale().count)
    {
        const double c = proc.scale()[deg].cents;
        top = String::fromUTF8 (nearestNoteName (proc.geti (ids::tonic), proc.getParam (ids::fine), c).c_str())
            + "   " + String (juce::roundToInt (c)) + String::fromUTF8 ("\xc2\xa2");
    }
    if (playMode == ids::Walk)
        bottom = proc.walking() ? (proc.hostPlaying() ? "WALKING  /  SYNCED TO LIVE" : "WALKING  /  FREE")
                                : "STOPPED  /  PLAY IN LIVE OR RUN";
    else if (playMode == ids::Follow)
        bottom = proc.walking() ? "FOLLOWING YOUR KEY" : "HOLD A KEY TO START A PHRASE";
    else
        bottom = "YOUR KEYS PLAY THE SCALE";

    String b;
    if (const auto* d = proc.modeDef())
        b = String::fromUTF8 (d->blurb);
    else
        b = "From a Scala file: " + String (proc.scale().count) + " notes to the "
          + (std::abs (proc.scale().period - 1200.0) < 0.5 ? String ("octave")
                                                            : String (juce::roundToInt (proc.scale().period)) + "-cent period");
    if (top != lcdTop || bottom != lcdBottom || b != blurb)
    {
        lcdTop = top;
        lcdBottom = bottom;
        blurb = b;
        repaint (kLcd.toNearestInt().expanded (2));
        repaint (kBlurb.toNearestInt());
    }
}

// ---------------------------------------------------------------------------
void SeyirPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    // Mounting slots and the title, as on Dicey and Beadwork.
    drawScrewSlot (g, { 50.0f, 37.0f });
    drawScrewSlot (g, { (float) designW - 50.0f, 37.0f });
    drawText (g, "SEYIR", { 100.0f, 14.0f, 130.0f, 46.0f }, 38.0f, colours::brand, true,
              juce::Justification::centredLeft);
    drawText (g, "microtonal melody explorer", { 226.0f, 22.0f, 300.0f, 34.0f }, 19.0f,
              colours::ink, true, juce::Justification::centredLeft);
    drawText (g, "MODE", { 520.0f, 20.0f, 62.0f, 34.0f }, 13.0f, colours::ink, true, juce::Justification::centredRight);

    // Sections.
    const auto& vm = voiceModels()[(size_t) juce::jlimit (0, numVoiceModels() - 1, proc.geti (ids::voice))];
    drawSection (g, kScaleBox, "SCALE");
    drawSection (g, kPathBox, "PATH");
    drawSection (g, kWalkBox, "WALK");
    drawSection (g, kVoiceBox, vm.kind == VoiceKind::Plaits ? "VOICE  /  PLAITS" : "VOICE  /  RINGS");
    drawSection (g, kShapeBox, "SHAPE");
    drawSection (g, kDroneBox, "DRONE");

    // LCD.
    g.setColour (colours::black);
    g.fillRect (kLcd);
    g.setColour (colours::inkDim);
    g.drawRect (kLcd, 1.0f);
    drawText (g, lcdTop, kLcd.withHeight (42.0f).translated (0.0f, 4.0f), 24.0f, colours::lcd, true);
    drawText (g, lcdBottom, kLcd.withTrimmedTop (42.0f).withTrimmedBottom (8.0f), 12.0f, colours::lcd.withAlpha (0.8f), true);

    // Blurb under the scale controls.
    g.setColour (colours::inkDim);
    g.setFont (panelFont (13.0f, false));
    g.drawFittedText (blurb, kBlurb.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);

    drawText (g, "DEJA VU LOOP", { 270.0f, 900.0f, 346.0f, 16.0f }, 11.0f, colours::inkDim, true);
    drawText (g, "Sound engines: Mutable Instruments", { 660.0f, 884.0f, 372.0f, 16.0f }, 11.0f, colours::inkDim, false);
    drawText (g, "Plaits and Rings by Emilie Gillet, MIT licence", { 660.0f, 902.0f, 372.0f, 16.0f }, 11.0f,
              colours::inkDim, false);
}

// ===========================================================================
SeyirEditor::SeyirEditor (SeyirProcessor& p)
    : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);

    panel.onTooltips = [this] (bool on)
    {
        if (on && tooltips == nullptr)
            tooltips = std::make_unique<juce::TooltipWindow> (this, 700);
        else if (! on)
            tooltips.reset();
    };
    panel.syncTooltips();

    setResizable (true, true);
    const double ratio = (double) SeyirPanel::designW / SeyirPanel::designH;
    setResizeLimits (900, (int) (900 / ratio), 2600, (int) (2600 / ratio));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (ratio);
    setSize (1200, (int) (1200 / ratio));
}

void SeyirEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
}

void SeyirEditor::resized()
{
    const float s = (float) getWidth() / (float) SeyirPanel::designW;
    panel.setTransform (juce::AffineTransform::scale (s));
    panel.setBounds (0, 0, SeyirPanel::designW, SeyirPanel::designH);
}
