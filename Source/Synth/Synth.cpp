#include "Synth.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "RingsBox.h"
#include "plaits/dsp/voice.h"
#include "stmlib/dsp/filter.h"

namespace seyir
{
namespace
{
constexpr int    kBlock = 12;          // Plaits' block size
constexpr double kRate  = 48000.0;
constexpr float  kBlockSeconds = (float) (kBlock / kRate);

// Plaits is tuned for the module's real clock (47872.34 Hz). At an exact
// 48 kHz it would play 4.6 cents sharp, which matters here.
const float kPlaitsTune = (float) (12.0 * std::log2 (47872.34 / 48000.0));

constexpr int kRing = 128;             // resampler history, a power of two

inline float softClip (float x)
{
    x = std::clamp (x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

inline float timeToCoef (float seconds)
{
    // One-pole coefficient per block for a time constant.
    return 1.0f - std::exp (-kBlockSeconds / std::max (1e-4f, seconds));
}
} // namespace

// ---------------------------------------------------------------------------
// Voices
// ---------------------------------------------------------------------------
const std::vector<VoiceModel>& voiceModels()
{
    using K = VoiceKind;
    static const std::vector<VoiceModel> list {
        // name, kind, engine, knob captions, LPG colour applies, level trim
    { "Plucked string",   K::Plaits, 19, "MATERIAL", "BRIGHT",  "DAMPING",  "DECAY",   false, 2.0f },
        { "Modal",            K::Plaits, 20, "MATERIAL", "BRIGHT",  "DAMPING",  "DECAY",   false, 1.0f },
        { "Classic analog",   K::Plaits, 8,  "DETUNE",   "SQUARE",  "SAW",      "DECAY",   true, 1.0f },
        { "Analog + filter",  K::Plaits, 0,  "RESO",     "CUTOFF",  "SHAPE",    "DECAY",   true, 1.0f },
        { "Waveshaper",       K::Plaits, 9,  "SHAPE",    "FOLD",    "ASYM",     "DECAY",   true, 1.0f },
        { "Two-op FM",        K::Plaits, 10, "RATIO",    "INDEX",   "FEEDBACK", "DECAY",   true, 1.0f },
        { "Six-op FM 1",      K::Plaits, 2,  "PATCH",    "BRIGHT",  "ENV",      "DECAY",   false, 1.0f },
        { "Six-op FM 2",      K::Plaits, 3,  "PATCH",    "BRIGHT",  "ENV",      "DECAY",   false, 1.0f },
        { "Six-op FM 3",      K::Plaits, 4,  "PATCH",    "BRIGHT",  "ENV",      "DECAY",   false, 1.0f },
        { "Phase distortion", K::Plaits, 1,  "RATIO",    "AMOUNT",  "SHAPE",    "DECAY",   true, 1.0f },
        { "Formant",          K::Plaits, 11, "RATIO",    "FORMANT", "WIDTH",    "DECAY",   true, 1.0f },
        { "Harmonic",         K::Plaits, 12, "SPREAD",   "PEAK",    "SHAPE",    "DECAY",   true, 1.0f },
        { "Wavetable",        K::Plaits, 13, "BANK",     "ROW",     "COLUMN",   "DECAY",   true, 1.0f },
        { "Wave terrain",     K::Plaits, 5,  "TERRAIN",  "RADIUS",  "OFFSET",   "DECAY",   true, 1.0f },
        { "Swarm",            K::Plaits, 16, "SPREAD",   "DENSITY", "GRAIN",    "DECAY",   true, 1.0f },
        { "Particle",         K::Plaits, 18, "SPREAD",   "DENSITY", "FILTER",   "DECAY",   true, 1.0f },
        { "Speech",           K::Plaits, 15, "SET",      "SPECIES", "PHONEME",  "DECAY",   true, 1.0f },

        { "Resonator",           K::Rings, RingsBox::Modal,             "STRUCTURE", "BRIGHT", "POSITION", "DAMPING", false, 1.4f },
        { "Sympathetic strings", K::Rings, RingsBox::SympatheticString, "STRUCTURE", "BRIGHT", "POSITION", "DAMPING", false, 1.4f },
        { "Inharmonic string",   K::Rings, RingsBox::String,            "STRUCTURE", "BRIGHT", "POSITION", "DAMPING", false, 2.2f },
        { "FM voice",            K::Rings, RingsBox::FmVoice,          "RATIO",     "BRIGHT", "FEEDBACK", "DAMPING", false, 1.4f },
    };
    return list;
}

int numVoiceModels() { return (int) voiceModels().size(); }
int defaultVoiceModel() { return 0; }

// ---------------------------------------------------------------------------
struct Synth::Impl
{
    // --- Mutable voices -------------------------------------------------------
    plaits::Voice melody;
    RingsBox rings;   // the Rings voice, and its reverb for SPACE
    plaits::Voice droneA, droneB;

    alignas (16) char melodyRam[16384];
    alignas (16) char droneARam[16384];
    alignas (16) char droneBRam[16384];

    stmlib::Svf svfL, svfR;

    // --- melody state ----------------------------------------------------------
    bool gate = false;
    int  gateLowBlocks = 0;   // a retrigger: hold the gate low this many blocks
    bool strum = false;       // Rings: strike on the next block
    float env = 0.0f;
    enum Stage { Idle, Attack, Decay, Release } stage = Idle;
    float velocity = 0.8f;

    bool   hasPitch = false;
    double pitch = 60.0;      // sounding, before shake
    double slideFrom = 60.0, slideTo = 60.0;
    float  slideLen = 0.0f, slideT = 0.0f, holdT = 0.0f;
    double shakeDepth = 0.0;
    float  shakePhase = 0.0f, shakeT = 0.0f;

    int lastVoice = -1;

    // --- drone -------------------------------------------------------------------
    float droneGain = 0.0f;
    double pluckClock = 0.0;
    int pluckIndex = 0;
    int pluckA = 0, pluckB = 0;     // blocks of trigger left
    double pluckPitchA = 38.0, pluckPitchB = 45.0;
    DroneStyle lastStyle = DroneStyle::Hold;

    // --- resampler ---------------------------------------------------------------
    float ringL[kRing] {}, ringR[kRing] {};
    int64_t written = 0;    // internal frames produced
    double  readPos = 0.0;  // internal frame position of the next output sample
    double  ratio = 1.0;    // internal frames per host sample

    SynthSettings s;

    void init()
    {
        stmlib::BufferAllocator a1 (melodyRam, sizeof (melodyRam));
        melody.Init (&a1);
        stmlib::BufferAllocator a2 (droneARam, sizeof (droneARam));
        droneA.Init (&a2);
        stmlib::BufferAllocator a3 (droneBRam, sizeof (droneBRam));
        droneB.Init (&a3);

        svfL.Init();
        svfR.Init();
    }

    // ---------------------------------------------------------------------------
    void noteOn (const NoteEvent& e)
    {
        const double target = e.pitch;
        const double from = hasPitch ? currentPitch() : target;
        velocity = e.velocity;

        // Portamento from the GLIDE knob; ornaments override it.
        float glideSec = s.glide > 0.001f ? 0.6f * s.glide * s.glide : 0.0f;
        holdT = 0.0f;
        shakeDepth = 0.0;
        slideFrom = from;
        slideTo = target;

        switch (e.ornament)
        {
            case 1:   // slide in from the last note: a meend
                glideSec = std::max (glideSec, e.stepSeconds * 0.5f);
                break;
            case 2:   // grace: start on the neighbour, then drop into the note
                if (e.grace >= 0.0)
                {
                    slideFrom = e.grace;
                    holdT = std::min (0.07f, e.stepSeconds * 0.22f);
                    glideSec = std::min (0.05f, e.stepSeconds * 0.15f);
                }
                break;
            case 3:   // shake: an oscillation above the note (gamak)
                shakeDepth = e.shake;
                shakeT = 0.0f;
                shakePhase = 0.0f;
                break;
            default:
                break;
        }
        if (! hasPitch)
            slideFrom = e.ornament == 2 && e.grace >= 0.0 ? e.grace : target;

        slideLen = glideSec;
        slideT = 0.0f;
        pitch = slideFrom;
        hasPitch = true;

        if (gate)
            gateLowBlocks = 2;   // let Plaits see a new edge
        gate = true;
        strum = true;
        stage = Attack;
    }

    void noteOff()
    {
        gate = false;
        if (stage != Idle)
            stage = Release;
    }

    void allOff()
    {
        gate = false;
        stage = Idle;
        env = 0.0f;
        shakeDepth = 0.0;
    }

    double currentPitch() const
    {
        return pitch + (shakeDepth > 0.0 ? shakeOffset() : 0.0);
    }

    double shakeOffset() const
    {
        // Rises in over 80 ms, then swings between the note and above it.
        const float in = std::min (1.0f, shakeT / 0.08f);
        return shakeDepth * in * 0.5 * (1.0 - std::cos (shakePhase));
    }

    void advancePitch()
    {
        if (holdT > 0.0f)
        {
            holdT -= kBlockSeconds;
            pitch = slideFrom;
            return;
        }
        if (slideLen > 0.0f && slideT < slideLen)
        {
            slideT = std::min (slideLen, slideT + kBlockSeconds);
            const double x = slideT / slideLen;
            const double eased = 0.5 - 0.5 * std::cos (3.14159265358979 * x);
            pitch = slideFrom + (slideTo - slideFrom) * eased;
        }
        else
        {
            pitch = slideTo;
        }
        if (shakeDepth > 0.0)
        {
            shakeT += kBlockSeconds;
            shakePhase += 2.0f * 3.14159265f * 6.5f * kBlockSeconds;
        }
    }

    void advanceEnvelope()
    {
        const float attackSec = 0.001f + 2.0f * s.attack * s.attack;
        const float decaySec = 0.01f + 4.0f * s.decay * s.decay;
        switch (stage)
        {
            case Idle: env = 0.0f; break;
            case Attack:
                env += (1.15f - env) * timeToCoef (attackSec * 0.5f);
                if (env >= 1.0f) { env = 1.0f; stage = Decay; }
                break;
            case Decay:
                env += (s.sustain - env) * timeToCoef (decaySec * 0.35f);
                break;
            case Release:
                env += (0.0f - env) * timeToCoef (decaySec * 0.35f);
                if (env < 1e-4f) { env = 0.0f; stage = Idle; }
                break;
        }
    }

    // One block of the melody voice into l/r.
    void renderMelody (float* l, float* r)
    {
        const auto& models = voiceModels();
        const int vi = std::clamp (s.voice, 0, (int) models.size() - 1);
        const auto& m = models[(size_t) vi];

        if (vi != lastVoice)
        {
            if (m.kind == VoiceKind::Rings)
                rings.setModel (m.engine);
            lastVoice = vi;
        }

        advancePitch();
        advanceEnvelope();
        const double p = currentPitch();

        if (m.kind == VoiceKind::Plaits)
        {
            plaits::Patch patch {};
            patch.note = (float) p + kPlaitsTune;
            patch.harmonics = s.a;
            patch.timbre = s.b;
            patch.morph = s.c;
            patch.engine = m.engine;
            patch.decay = s.decay;
            patch.lpg_colour = s.color;
            patch.frequency_modulation_amount = 0.0f;
            patch.timbre_modulation_amount = s.motion;
            patch.morph_modulation_amount = 0.0f;

            plaits::Modulations mod {};
            const bool high = gate && gateLowBlocks == 0;
            if (gateLowBlocks > 0) --gateLowBlocks;
            mod.trigger = high ? 1.0f : 0.0f;
            mod.trigger_patched = true;
            mod.level_patched = true;
            // Engines with a low-pass gate follow the envelope. The ones that
            // make their own envelope (strings, modal, six-op FM) take the
            // level as how hard they're struck.
            mod.level = (m.usesColor ? env : 1.0f) * (0.35f + 0.65f * velocity);

            plaits::Voice::Frame f[kBlock];
            melody.Render (patch, mod, f, kBlock);
            for (int i = 0; i < kBlock; ++i)
            {
                const float x = (float) f[i].out / 32768.0f;
                l[i] = x * m.gain;
                r[i] = x * m.gain;
            }
        }
        else
        {
            const float bright = std::clamp (s.b + (velocity - 0.8f) * 0.25f, 0.0f, 1.0f);
            rings.resonate (strum, (float) p, s.a, bright, s.decay, s.c, l, r, kBlock);
            for (int i = 0; i < kBlock; ++i)
            {
                l[i] *= m.gain;
                r[i] *= m.gain;
            }
        }
        strum = false;
    }

    // One block of the drone, added into l/r.
    void renderDrone (float* l, float* r)
    {
        const float wanted = s.droneOn && s.droneActive ? s.droneLevel : 0.0f;
        const float start = droneGain;
        droneGain += (wanted - droneGain) * timeToCoef (0.6f);
        if (start < 1e-4f && droneGain < 1e-4f)
            return;

        double second = 7.01955;
        switch (s.droneNotes)
        {
            case DroneNotes::Tonic:  second = 12.0; break;
            case DroneNotes::Fifth:  second = 7.01955; break;
            case DroneNotes::Fourth: second = 4.98045; break;
            case DroneNotes::Octave: second = 12.0; break;
        }
        const double t = s.droneTonic;

        plaits::Patch pa {}, pb {};
        plaits::Modulations ma {}, mb {};
        const float tone = std::clamp (s.droneTone, 0.0f, 1.0f);

        if (s.droneStyle != lastStyle)
        {
            lastStyle = s.droneStyle;
            pluckClock = 0.0;
            pluckIndex = 0;
        }

        if (s.droneStyle == DroneStyle::Hold)
        {
            // Harmonic (additive) engine, held: an organ-like tanpura bed.
            for (auto* p : { &pa, &pb })
            {
                p->engine = 12;
                p->harmonics = 0.15f + 0.55f * tone;
                p->timbre = 0.25f + 0.5f * tone;
                p->morph = 0.15f;
                p->decay = 0.5f;
                p->lpg_colour = 0.5f;
            }
            pa.note = (float) t + kPlaitsTune;
            pb.note = (float) (t + second) + kPlaitsTune;
            if (s.droneNotes == DroneNotes::Tonic)
                pb.note = pa.note;   // doubled, a little detuned below
            pb.note += 0.03f;
        }
        else
        {
            // Plucked string, struck in a tanpura's cycle: the second string,
            // the tonic twice, then the low tonic.
            const double beat = 60.0 / std::max (40.0, s.bpm);
            const double interval = s.bpm > 110.0 ? beat * 2.0 : beat;
            pluckClock -= kBlockSeconds;
            if (pluckClock <= 0.0)
            {
                pluckClock += interval;
                switch (pluckIndex & 3)
                {
                    case 0: pluckPitchB = t + second - (s.droneNotes == DroneNotes::Tonic ? 0.0 : 12.0); pluckB = 8; break;
                    case 1: pluckPitchA = t; pluckA = 8; break;
                    case 2: pluckPitchA = t; pluckA = 8; break;
                    case 3: pluckPitchA = t - 12.0; pluckA = 8; break;
                }
                ++pluckIndex;
            }
            for (auto* p : { &pa, &pb })
            {
                p->engine = 19;
                p->harmonics = 0.05f + 0.3f * tone;
                p->timbre = 0.3f + 0.55f * tone;
                p->morph = 0.82f;
                p->decay = 0.8f;
                p->lpg_colour = 0.5f;
            }
            pa.note = (float) pluckPitchA + kPlaitsTune;
            pb.note = (float) pluckPitchB + kPlaitsTune;
            ma.trigger_patched = mb.trigger_patched = true;
            ma.trigger = pluckA > 0 ? 1.0f : 0.0f;
            mb.trigger = pluckB > 0 ? 1.0f : 0.0f;
            if (pluckA > 0) --pluckA;
            if (pluckB > 0) --pluckB;
        }

        plaits::Voice::Frame fa[kBlock], fb[kBlock];
        droneA.Render (pa, ma, fa, kBlock);
        droneB.Render (pb, mb, fb, kBlock);
        const float g = 0.5f / 32768.0f;
        for (int i = 0; i < kBlock; ++i)
        {
            const float gi = (start + (droneGain - start) * (float) (i + 1) / kBlock) * g;
            const float a = fa[i].out * gi, b = fb[i].out * gi;
            l[i] += a * 0.75f + b * 0.25f;
            r[i] += a * 0.25f + b * 0.75f;
        }
    }

    // One internal block: voices, shaping, space.
    void renderBlock (float* l, float* r)
    {
        renderMelody (l, r);

        // Drive, then the low-pass.
        const float drive = s.drive;
        const float pre = 1.0f + drive * 7.0f;
        const float post = 1.0f / (1.0f + drive * 1.5f);
        const float cutoff = 60.0f * std::pow (2.0f, 8.4f * std::clamp (s.tone, 0.0f, 1.0f));
        const float f = std::min (0.45f, cutoff / (float) kRate);
        const float q = 0.5f + s.res * s.res * 12.0f;
        svfL.set_f_q<stmlib::FREQUENCY_FAST> (f, q);
        svfR.set_f_q<stmlib::FREQUENCY_FAST> (f, q);
        const bool open = s.tone > 0.995f && s.res < 0.01f;
        for (int i = 0; i < kBlock; ++i)
        {
            float x = l[i], y = r[i];
            if (drive > 0.001f)
            {
                x = softClip (x * pre) * post;
                y = softClip (y * pre) * post;
            }
            if (! open)
            {
                x = svfL.Process<stmlib::FILTER_MODE_LOW_PASS> (x);
                y = svfR.Process<stmlib::FILTER_MODE_LOW_PASS> (y);
            }
            l[i] = x * s.level;
            r[i] = y * s.level;
        }

        renderDrone (l, r);

        // Space: Rings' reverb (a Griesinger/Dattorro plate).
        if (s.space > 0.001f)
        {
            rings.reverb (l, r, kBlock, 0.08f + 0.5f * s.space, 0.45f + 0.5f * s.space, 0.75f - 0.25f * s.space);
        }

        for (int i = 0; i < kBlock; ++i)
        {
            l[i] = softClip (l[i]);
            r[i] = softClip (r[i]);
        }
    }

    void produce()
    {
        float l[kBlock], r[kBlock];
        renderBlock (l, r);
        for (int i = 0; i < kBlock; ++i)
        {
            const auto k = (size_t) ((written + i) & (kRing - 1));
            ringL[k] = l[i];
            ringR[k] = r[i];
        }
        written += kBlock;
    }

    static float hermite (float y0, float y1, float y2, float y3, float t)
    {
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    void nextSample (float& outL, float& outR)
    {
        const auto i = (int64_t) std::floor (readPos);
        while (written < i + 3)
            produce();
        const float t = (float) (readPos - (double) i);
        auto at = [this] (const float* ring, int64_t k) { return ring[(size_t) (std::max<int64_t> (0, k) & (kRing - 1))]; };
        outL = hermite (at (ringL, i - 1), at (ringL, i), at (ringL, i + 1), at (ringL, i + 2), t);
        outR = hermite (at (ringR, i - 1), at (ringR, i), at (ringR, i + 1), at (ringR, i + 2), t);
        readPos += ratio;
    }
};

// ---------------------------------------------------------------------------
Synth::Synth() : impl (std::make_unique<Impl>())
{
    impl->init();
}

Synth::~Synth() = default;

void Synth::prepare (double hostRate)
{
    impl->ratio = kRate / (hostRate > 0.0 ? hostRate : kRate);
}

void Synth::reset()
{
    impl->allOff();
    impl->hasPitch = false;
    impl->droneGain = 0.0f;
    sounding = -1.0;
}

void Synth::process (float* left, float* right, int n, const SynthSettings& settings,
                     const NoteEvent* events, int numEvents)
{
    auto& m = *impl;
    m.s = settings;
    int e = 0;
    for (int i = 0; i < n; ++i)
    {
        while (e < numEvents && events[e].offset <= i)
        {
            const auto& ev = events[e++];
            switch (ev.type)
            {
                case NoteEvent::On:     m.noteOn (ev); break;
                case NoteEvent::Off:    m.noteOff(); break;
                case NoteEvent::AllOff: m.allOff(); break;
            }
        }
        m.nextSample (left[i], right[i]);
    }
    for (; e < numEvents; ++e)
    {
        const auto& ev = events[e];
        if (ev.type == NoteEvent::On) m.noteOn (ev);
        else if (ev.type == NoteEvent::Off) m.noteOff();
        else m.allOff();
    }
    sounding = (m.stage != Impl::Idle || m.gate) ? m.currentPitch() : -1.0;
}
} // namespace seyir
