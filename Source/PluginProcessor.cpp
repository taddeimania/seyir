#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace seyir;

namespace
{
const juce::Identifier kEditsTag { "Edits" };
const juce::Identifier kScalaTag { "Scala" };

// How long the drone keeps sounding after the last key or audition.
constexpr double kDroneLinger = 6.0;

juce::String joinFloats (const float* v, int n)
{
    juce::StringArray a;
    for (int i = 0; i < n; ++i) a.add (juce::String (v[i], 4));
    return a.joinIntoString (",");
}

juce::String joinDoubles (const double* v, int n)
{
    juce::StringArray a;
    for (int i = 0; i < n; ++i) a.add (juce::String (v[i], 4));
    return a.joinIntoString (",");
}

int whiteIndex (int note)
{
    static const int map[12] { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    const int pc = ((note % 12) + 12) % 12;
    if (map[pc] < 0) return INT_MIN;
    const int oct = (note - pc) / 12;
    return oct * 7 + map[pc];
}

int floorDiv (int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
} // namespace

bool DegreeEdits::any() const
{
    for (size_t i = 0; i < weight.size(); ++i)
        if (weight[i] >= 0.0f || std::abs (cents[i]) > 1e-6)
            return true;
    return false;
}

// ===========================================================================
SeyirProcessor::SeyirProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SeyirState", createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            params[withId->paramID] = apvts.getRawParameterValue (withId->paramID);

    events.reserve (512);
    rebuildScale();
    startTimerHz (30);
}

SeyirProcessor::~SeyirProcessor()
{
    stopTimer();
}

bool SeyirProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SeyirProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    synth.prepare (sampleRate);
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
SynthSettings SeyirProcessor::readSynth() const
{
    SynthSettings s;
    s.voice   = geti (ids::voice);
    s.a       = getParam (ids::timbreA);
    s.b       = getParam (ids::timbreB);
    s.c       = getParam (ids::timbreC);
    s.decay   = getParam (ids::decay);
    s.color   = getParam (ids::color);
    s.glide   = getParam (ids::glide);
    s.attack  = getParam (ids::attack);
    s.sustain = getParam (ids::sustain);
    s.motion  = getParam (ids::motion);
    s.tone    = getParam (ids::tone);
    s.res     = getParam (ids::res);
    s.drive   = getParam (ids::drive);
    s.space   = getParam (ids::space);
    s.level   = getParam (ids::level);

    s.droneOn    = getParam (ids::drone) > 0.5f;
    s.droneLevel = getParam (ids::droneLevel);
    s.droneTone  = getParam (ids::droneTone);
    s.droneNotes = (DroneNotes) juce::jlimit (0, 3, geti (ids::droneNotes));
    s.droneStyle = (DroneStyle) juce::jlimit (0, 1, geti (ids::droneStyle));
    s.droneTonic = geti (ids::tonic) + getParam (ids::fine) / 100.0 + 12.0 * geti (ids::droneOct);
    return s;
}

WalkSettings SeyirProcessor::readWalk() const
{
    WalkSettings w;
    w.density   = getParam (ids::density);
    w.leap      = getParam (ids::leap);
    w.gravity   = getParam (ids::gravity);
    w.arc       = getParam (ids::arc);
    w.ornament  = getParam (ids::ornament);
    w.dejavu    = getParam (ids::dejavu);
    w.phrase    = geti (ids::phrase);
    w.loopSteps = ids::loopSteps (geti (ids::loop));
    return w;
}

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------
int SeyirProcessor::keyToRung (int note, const Ladder& lad) const
{
    if (lad.count == 0)
        return -1;
    const double tonic = geti (ids::tonic) + getParam (ids::fine) / 100.0;

    if (geti (ids::keys) == 0)
    {
        // SNAP: the nearest pitch of the scale to the key you pressed.
        const double cents = (note - tonic) * 100.0;
        int best = -1;
        double bestD = 1e9;
        for (int i = 0; i < lad.count; ++i)
        {
            if (lad[i].weight <= 0.0f) continue;
            const double d = std::abs (lad[i].cents - cents);
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    // WHITE KEYS: C3 is the tonic and each white key is the next degree.
    const int w = whiteIndex (note);
    if (w == INT_MIN)
        return -1;
    const int idx = w - whiteIndex (60);
    std::array<int, kMaxDegrees> active {};
    int k = 0;
    for (int i = 0; i < audioScale.count; ++i)
        if (audioScale[i].weight > 0.0f)
            active[(size_t) k++] = i;
    if (k == 0)
        return -1;
    const int oct = floorDiv (idx, k);
    const int deg = active[(size_t) (idx - oct * k)];
    for (int i = 0; i < lad.count; ++i)
        if (lad[i].degree == deg && lad[i].octave == oct)
            return i;
    return lad.nearest (audioScale[deg].cents + oct * audioScale.period);
}

void SeyirProcessor::noteOnRung (int offset, int rung, float velocity, int ornament, int graceRung,
                                 double stepSeconds, const Ladder& lad, bool trail, bool phraseEnd, bool replayed)
{
    if (rung < 0 || rung >= lad.count)
        return;
    const double tonic = geti (ids::tonic) + getParam (ids::fine) / 100.0;
    const auto& r = lad[rung];

    NoteEvent e;
    e.offset = offset;
    e.type = NoteEvent::On;
    e.pitch = tonic + r.cents / 100.0;
    e.velocity = velocity;
    e.ornament = ornament;
    e.stepSeconds = (float) stepSeconds;
    if (ornament == 2 && graceRung >= 0 && graceRung < lad.count)
        e.grace = tonic + lad[graceRung].cents / 100.0;
    if (ornament == 3)
    {
        const double up = rung + 1 < lad.count ? lad[rung + 1].cents - r.cents : 100.0;
        e.shake = juce::jlimit (0.25, 2.0, up / 100.0 * 0.8);
    }
    events.push_back (e);
    noteSounding = true;
    lastRung = rung;
    uiDegree.store (r.degree, std::memory_order_relaxed);
    uiCents.store (r.cents, std::memory_order_relaxed);

    if (trail)
    {
        TrailNote t;
        t.cents = r.cents;
        t.degree = r.degree;
        t.phraseEnd = phraseEnd;
        t.replayed = replayed;
        t.time = clock + offset / sampleRate;
        pushTrail (t);
    }
}

void SeyirProcessor::noteOff (int offset)
{
    if (! noteSounding)
        return;
    NoteEvent e;
    e.offset = offset;
    e.type = NoteEvent::Off;
    events.push_back (e);
    noteSounding = false;
    uiDegree.store (-1, std::memory_order_relaxed);
}

void SeyirProcessor::pushTrail (const TrailNote& t)
{
    const auto scope = trailFifo.write (1);
    if (scope.blockSize1 > 0)
        trailNotes[(size_t) scope.startIndex1] = t;
}

void SeyirProcessor::doStep (int offset, double stepSeconds)
{
    const auto st = walker.step (readWalk());
    uiLoopPos.store (walker.loopPosition(), std::memory_order_relaxed);
    if (st.kind == Step::Note)
        noteOnRung (offset, st.rung, st.velocity, (int) st.ornament, st.grace, stepSeconds,
                    walkLadder, true, st.phraseEnd, st.replayed);
    else if (st.kind == Step::Rest)
        noteOff (offset);
}

void SeyirProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = audio.getNumSamples();
    audio.clear();
    events.clear();

    if (panicRequested.exchange (false))
    {
        walker.stop();
        numHeld = 0;
        noteSounding = false;
        NoteEvent e;
        e.type = NoteEvent::AllOff;
        events.push_back (e);
        uiDegree.store (-1);
    }
    if (reseedRequested.exchange (false))
        walker.reseed (nextSeed.load());

    // --- the scale ---------------------------------------------------------------
    if (pendingVersion.load (std::memory_order_acquire) != audioVersion)
    {
        const juce::SpinLock::ScopedTryLockType lock (scaleLock);
        if (lock.isLocked())
        {
            audioScale = pending;
            audioVersion = pendingVersion.load();
            ladderRange = -1.0f;
        }
    }
    const float range = getParam (ids::range);
    if (std::abs (range - ladderRange) > 1e-4f)
    {
        ladderRange = range;
        const double p = audioScale.period;
        walkLadder.build (audioScale, -p * 0.42, p * range);
        playLadder.build (audioScale, -p * 3.0, p * 5.0);
        walker.setLadder (walkLadder);
    }

    // --- transport -----------------------------------------------------------------
    bool playing = false;
    double bpm = 100.0, ppq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            bpm = pos->getBpm().orFallback (100.0);
            ppq = pos->getPpqPosition().orFallback (0.0);
        }
    bpm = juce::jlimit (20.0, 400.0, bpm);
    const int mode = geti (ids::play);
    const double stepBeats = ids::rateBeats (geti (ids::rate));
    const double ppqPerSample = bpm / 60.0 / sampleRate;
    const double stepSeconds = stepBeats * 60.0 / bpm;

    // --- keys ---------------------------------------------------------------------------
    int followStart = -1;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int pos = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);
        if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            walker.stop();
            numHeld = 0;
            noteOff (pos);
            continue;
        }
        if (! msg.isNoteOnOrOff())
            continue;

        const int note = msg.getNoteNumber();
        lastKeyTime = clock + pos / sampleRate;
        if (msg.isNoteOn())
        {
            for (int i = 0; i < numHeld; ++i)
                if (held[(size_t) i] == note) { std::move (held.begin() + i + 1, held.begin() + numHeld, held.begin() + i); --numHeld; break; }
            if (numHeld < (int) held.size())
                held[(size_t) numHeld++] = note;

            if (mode == ids::Play)
                noteOnRung (pos, keyToRung (note, playLadder), msg.getFloatVelocity(), 0, -1, stepSeconds,
                            playLadder, true, false, false);
            else if (mode == ids::Follow)
            {
                const int r = keyToRung (note, walkLadder);
                if (r >= 0)
                {
                    walker.startFrom (r);
                    if (! playing && followStart < 0 && numHeld == 1)
                        followStart = pos;
                }
            }
        }
        else
        {
            const bool wasNewest = numHeld > 0 && held[(size_t) numHeld - 1] == note;
            for (int i = 0; i < numHeld; ++i)
                if (held[(size_t) i] == note) { std::move (held.begin() + i + 1, held.begin() + numHeld, held.begin() + i); --numHeld; break; }

            if (mode == ids::Play)
            {
                if (numHeld == 0)
                    noteOff (pos);
                else if (wasNewest)   // back to the key still held (legato)
                    noteOnRung (pos, keyToRung (held[(size_t) numHeld - 1], playLadder), 0.7f, 1, -1, stepSeconds,
                                playLadder, false, false, false);
            }
            else if (mode == ids::Follow && numHeld == 0)
            {
                walker.stop();
                noteOff (pos);
            }
        }
    }

    // --- the panel's ladder ----------------------------------------------------------------
    {
        const auto scope = panelFifo.read (panelFifo.getNumReady());
        auto apply = [&] (const PanelEvent& pe)
        {
            lastKeyTime = clock;
            if (! pe.on) { noteOff (0); return; }
            int rung = -1;
            for (int i = 0; i < playLadder.count; ++i)
                if (playLadder[i].degree == pe.degree && playLadder[i].octave == pe.octave) { rung = i; break; }
            noteOnRung (0, rung, 0.8f, 0, -1, stepSeconds, playLadder, false, false, false);
        };
        for (int i = 0; i < scope.blockSize1; ++i) apply (panelEvents[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) apply (panelEvents[(size_t) (scope.startIndex2 + i)]);
    }

    // --- the walk ---------------------------------------------------------------------------
    const bool walk = (mode == ids::Walk && (playing || getParam (ids::run) > 0.5f))
                   || (mode == ids::Follow && numHeld > 0);
    if (walk)
    {
        if (playing)
        {
            if (! wasPlaying && mode == ids::Walk)
                walker.restart();   // the seyir starts with the song
            const double end = ppq + n * ppqPerSample;
            double t = std::ceil (ppq / stepBeats - 1e-9) * stepBeats;
            for (; t < end; t += stepBeats)
                doStep (juce::jlimit (0, n - 1, (int) std::floor ((t - ppq) / ppqPerSample)), stepSeconds);
        }
        else
        {
            if (! wasRunning || wasPlaying)
                freePpq = followStart >= 0 ? -followStart * ppqPerSample : 0.0;
            const double end = freePpq + n * ppqPerSample;
            double t = std::ceil (freePpq / stepBeats - 1e-9) * stepBeats;
            for (; t < end; t += stepBeats)
                doStep (juce::jlimit (0, n - 1, (int) std::floor ((t - freePpq) / ppqPerSample)), stepSeconds);
            freePpq = end;
        }
    }
    else if (wasRunning && mode != ids::Play)
    {
        walker.stop();
        noteOff (0);
    }
    wasRunning = walk;
    wasPlaying = playing;

    // --- sound ---------------------------------------------------------------------------------
    auto s = readSynth();
    s.bpm = bpm;
    s.droneActive = playing || walk || numHeld > 0 || (clock - lastKeyTime) < kDroneLinger;

    std::stable_sort (events.begin(), events.end(),
                      [] (const NoteEvent& a, const NoteEvent& b) { return a.offset < b.offset; });

    float* left = audio.getWritePointer (0);
    float* right = audio.getNumChannels() > 1 ? audio.getWritePointer (1) : nullptr;
    if (right != nullptr)
        synth.process (left, right, n, s, events.data(), (int) events.size());
    else
    {
        juce::HeapBlock<float> tmp ((size_t) n);
        synth.process (left, tmp.get(), n, s, events.data(), (int) events.size());
        for (int i = 0; i < n; ++i) left[i] = 0.5f * (left[i] + tmp[i]);
    }
    midi.clear();

    clock += n / sampleRate;
    uiClock.store (clock, std::memory_order_relaxed);
    uiPlaying.store (playing, std::memory_order_relaxed);
    uiWalking.store (walk, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------
void SeyirProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void SeyirProcessor::selectMode (int m)
{
    setParam (ids::mode, (float) m);
    setParam (ids::lower, 0.0f);
    setParam (ids::upper, 0.0f);
    rebuildScale();
}

const ModeDef* SeyirProcessor::modeDef() const
{
    const int m = geti (ids::mode);
    return m >= 0 && m < numModes() ? &modes()[(size_t) m] : nullptr;
}

bool SeyirProcessor::isMaqam() const
{
    const auto* d = modeDef();
    return d != nullptr && d->isMaqam();
}

void SeyirProcessor::audition (int degree, bool on, int octave)
{
    const juce::SpinLock::ScopedLockType lock (panelWriteLock);
    const auto scope = panelFifo.write (1);
    if (scope.blockSize1 > 0)
        panelEvents[(size_t) scope.startIndex1] = { degree, octave, on };
}

void SeyirProcessor::newSeed()
{
    nextSeed.store ((uint32_t) juce::Random::getSystemRandom().nextInt() | 1u);
    reseedRequested.store (true);
}

void SeyirProcessor::panic() { panicRequested.store (true); }

void SeyirProcessor::collectTrail (std::vector<TrailNote>& into)
{
    const auto scope = trailFifo.read (trailFifo.getNumReady());
    for (int i = 0; i < scope.blockSize1; ++i) into.push_back (trailNotes[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i) into.push_back (trailNotes[(size_t) (scope.startIndex2 + i)]);
}

// ---------------------------------------------------------------------------
// The scale
// ---------------------------------------------------------------------------
juce::String SeyirProcessor::scaleKey() const
{
    const int m = geti (ids::mode);
    if (m >= ids::customMode())
        return "scala:" + scalaName() + ":" + juce::String (scala.count) + ":" + juce::String (scala.period, 3);
    return juce::String (m) + ":" + juce::String (geti (ids::lower)) + ":" + juce::String (geti (ids::upper));
}

void SeyirProcessor::rebuildScale()
{
    const int m = geti (ids::mode);
    if (m >= ids::customMode())
        base = hasScala() ? scala : buildScale (0);
    else
        base = buildScale (m, geti (ids::lower) - 1, geti (ids::upper) - 1);

    const auto key = scaleKey();
    if (key != degreeEdits.scaleKey)
    {
        degreeEdits = DegreeEdits();
        degreeEdits.scaleKey = key;
    }
    builtKey = key;
    builtEditVersion = editVersion;

    current = base;
    for (int i = 0; i < current.count; ++i)
    {
        auto& d = current[i];
        if (degreeEdits.weight[(size_t) i] >= 0.0f)
            d.weight = degreeEdits.weight[(size_t) i];
        if (i > 0)
            d.cents = juce::jlimit (0.5, current.period - 0.5, d.cents + degreeEdits.cents[(size_t) i]);
    }

    {
        const juce::SpinLock::ScopedLockType lock (scaleLock);
        pending = current;
        pendingVersion.fetch_add (1, std::memory_order_release);
    }
}

void SeyirProcessor::timerCallback()
{
    if (scaleKey() != builtKey || editVersion != builtEditVersion)
        rebuildScale();
}

void SeyirProcessor::setDegreeWeight (int degree, float w)
{
    if (! juce::isPositiveAndBelow (degree, kMaxDegrees)) return;
    degreeEdits.weight[(size_t) degree] = juce::jlimit (0.0f, 1.0f, w);
    ++editVersion;
    rebuildScale();
}

void SeyirProcessor::setDegreeCents (int degree, double offset)
{
    if (degree <= 0 || degree >= kMaxDegrees) return;   // the tonic stays put
    degreeEdits.cents[(size_t) degree] = juce::jlimit (-100.0, 100.0, offset);
    ++editVersion;
    rebuildScale();
}

void SeyirProcessor::resetDegree (int degree)
{
    if (! juce::isPositiveAndBelow (degree, kMaxDegrees)) return;
    degreeEdits.weight[(size_t) degree] = -1.0f;
    degreeEdits.cents[(size_t) degree] = 0.0;
    ++editVersion;
    rebuildScale();
}

void SeyirProcessor::resetEdits()
{
    const auto key = degreeEdits.scaleKey;
    degreeEdits = DegreeEdits();
    degreeEdits.scaleKey = key;
    ++editVersion;
    rebuildScale();
}

bool SeyirProcessor::loadScala (const juce::File& f, juce::String& error)
{
    Scale s;
    std::string err;
    if (! parseScala (f.loadFileAsString().toStdString(), s, err))
    {
        error = juce::String (err);
        return false;
    }
    if (juce::String::fromUTF8 (s.name).trim().isEmpty() || juce::String (s.name) == "Scala file")
        s.setName (f.getFileNameWithoutExtension().toStdString());
    scala = s;
    selectMode (ids::customMode());
    return true;
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
void SeyirProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild (i).hasType (kEditsTag) || state.getChild (i).hasType (kScalaTag))
            state.removeChild (i, nullptr);

    if (degreeEdits.any())
    {
        juce::ValueTree e (kEditsTag);
        e.setProperty ("key", degreeEdits.scaleKey, nullptr);
        e.setProperty ("weights", joinFloats (degreeEdits.weight.data(), kMaxDegrees), nullptr);
        e.setProperty ("cents", joinDoubles (degreeEdits.cents.data(), kMaxDegrees), nullptr);
        state.appendChild (e, nullptr);
    }
    if (hasScala())
    {
        juce::ValueTree sc (kScalaTag);
        sc.setProperty ("name", scalaName(), nullptr);
        sc.setProperty ("period", scala.period, nullptr);
        std::vector<double> c;
        for (int i = 0; i < scala.count; ++i) c.push_back (scala[i].cents);
        sc.setProperty ("cents", joinDoubles (c.data(), (int) c.size()), nullptr);
        state.appendChild (sc, nullptr);
    }
    state.setProperty ("stateVersion", 1, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SeyirProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType()))
        return;

    scala = Scale();
    if (auto sc = state.getChildWithName (kScalaTag); sc.isValid())
    {
        Scale s;
        s.period = (double) sc.getProperty ("period", 1200.0);
        for (auto& t : juce::StringArray::fromTokens (sc["cents"].toString(), ",", {}))
            if (s.count < kMaxDegrees)
            {
                s[s.count].cents = t.getDoubleValue();
                s[s.count].weight = s.count == 0 ? 1.0f : 0.5f;
                s[s.count].resting = s.count == 0;
                ++s.count;
            }
        s.setName (sc["name"].toString().toStdString());
        if (s.count > 0)
            scala = s;
        state.removeChild (sc, nullptr);
    }

    DegreeEdits e;
    if (auto ed = state.getChildWithName (kEditsTag); ed.isValid())
    {
        e.scaleKey = ed["key"].toString();
        const auto w = juce::StringArray::fromTokens (ed["weights"].toString(), ",", {});
        const auto c = juce::StringArray::fromTokens (ed["cents"].toString(), ",", {});
        for (int i = 0; i < kMaxDegrees; ++i)
        {
            if (i < w.size()) e.weight[(size_t) i] = w[i].getFloatValue();
            if (i < c.size()) e.cents[(size_t) i] = c[i].getDoubleValue();
        }
        state.removeChild (ed, nullptr);
    }

    apvts.replaceState (state);
    degreeEdits = e;
    // Rebuild now so the edits are kept if they belong to the restored scale.
    if (degreeEdits.scaleKey != scaleKey())
        degreeEdits = DegreeEdits();
    degreeEdits.scaleKey = scaleKey();
    ++editVersion;
    rebuildScale();
}

juce::AudioProcessorEditor* SeyirProcessor::createEditor()
{
    return new SeyirEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SeyirProcessor();
}
