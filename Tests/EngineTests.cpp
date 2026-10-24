// EngineTests.cpp — checks for the scales, the walker and the synth, without
// JUCE. Run by ctest.
#include "Engine/Tuning.h"
#include "Engine/Walker.h"
#include "Synth/Synth.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace seyir;

static int failures = 0;
#define CHECK(cond) do { if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define NEAR(a, b, tol) CHECK (std::abs ((double) (a) - (double) (b)) <= (tol))

static int modeNamed (const char* name, Family f)
{
    for (int i = 0; i < numModes(); ++i)
        if (std::string (modes()[(size_t) i].name) == name && modes()[(size_t) i].family == f)
            return i;
    return -1;
}

static void testScales()
{
    // Rast: Rast on C and on G, with the descending B-flat.
    const auto rast = buildScale (modeNamed ("Rast", Family::Arabic));
    const double want[] { 0, 200, 350, 500, 700, 900, 1000, 1050 };
    CHECK (rast.count == 8);
    for (int i = 0; i < 8 && i < rast.count; ++i)
        NEAR (rast[i].cents, want[i], 0.01);
    CHECK (rast[6].use == kDown);   // B-flat, coming down
    CHECK (rast[7].use == kUp);     // B half-flat, going up
    CHECK (rast[0].weight == 1.0f && rast[4].resting);

    // Saba doesn't close on the octave.
    const auto saba = buildScale (modeNamed ("Saba", Family::Arabic));
    CHECK (saba.count == 8);
    NEAR (saba[saba.count - 1].cents, 1100, 0.01);

    // Swapping the upper jins of Bayati to Hijaz.
    const int bayati = modeNamed ("Bayati", Family::Arabic);
    const auto swapped = buildScale (bayati, -1, jinsIndex ("Hijaz", Family::Arabic));
    CHECK (std::string (swapped.name) == "Bayati + Hijaz");
    NEAR (swapped[4].cents, 600, 0.01);
    NEAR (swapped[5].cents, 900, 0.01);

    // Turkish Hüseyni in commas of 1/53 octave.
    const auto h = buildScale (modeNamed ("Hüseyni", Family::Turkish));
    CHECK (h.count == 7);
    NEAR (h[1].cents, 8 * 1200.0 / 53, 0.01);
    NEAR (h[5].cents, 39 * 1200.0 / 53, 0.01);

    // Yaman: Pa only on the way down.
    const auto yaman = buildScale (modeNamed ("Yaman", Family::Raga));
    bool foundPa = false;
    for (int i = 0; i < yaman.count; ++i)
        if (std::abs (yaman[i].cents - 701.955) < 0.01) { foundPa = true; CHECK (yaman[i].use == kDown); }
    CHECK (foundPa);

    // Every mode builds something sensible.
    for (int m = 0; m < numModes(); ++m)
    {
        const auto s = buildScale (m);
        CHECK (s.count >= 5);
        CHECK (s[0].cents == 0.0);
        for (int i = 1; i < s.count; ++i)
            CHECK (s[i].cents > s[i - 1].cents && s[i].cents < s.period);
    }

    // Bohlen-Pierce repeats at the tritave.
    const auto bp = buildScale (modeNamed ("Bohlen-Pierce", Family::Reference));
    NEAR (bp.period, 1901.955, 0.001);
    CHECK (bp.count == 13);
}

static void testScala()
{
    Scale s;
    std::string err;
    const char* text =
        "! slendro.scl\n"
        "!\n"
        "Test slendro\n"
        " 5\n"
        "!\n"
        " 231.0\n"
        " 474.0 a comment\n"
        " 717.\n"
        " 955.0\n"
        " 2/1\n";
    CHECK (parseScala (text, s, err));
    CHECK (s.count == 5);
    NEAR (s.period, 1200.0, 1e-6);
    NEAR (s[3].cents, 717.0, 1e-6);
    CHECK (std::string (s.name) == "Test slendro");

    double c = 0;
    CHECK (parseScalaPitch ("3/2", c));  NEAR (c, 701.955, 0.001);
    CHECK (parseScalaPitch ("2", c));    NEAR (c, 1200.0, 0.001);
    CHECK (! parseScalaPitch ("abc", c));
    CHECK (! parseScala ("only one line", s, err));

    CHECK (nearestNoteName (62, 0, 150) == "E -50");   // D + 150 cents
    CHECK (nearestNoteName (60, 0, 0) == "C");
}

static void testWalker()
{
    const auto scale = buildScale (modeNamed ("Rast", Family::Arabic));
    Ladder lad;
    lad.build (scale, -500, 2400);
    CHECK (lad.count > 16);
    NEAR (lad[lad.home].cents, 0.0, 1e-9);

    Walker w (1234);
    w.setLadder (lad);
    WalkSettings s;
    s.density = 0.7f;
    s.ornament = 0.4f;

    int notes = 0, cadences = 0, restingCadences = 0, wrongWay = 0, prev = -1;
    for (int i = 0; i < 4000; ++i)
    {
        const auto st = w.step (s);
        if (st.kind != Step::Note)
            continue;
        CHECK (st.rung >= 0 && st.rung < lad.count);
        ++notes;
        if (prev >= 0)
        {
            const auto& r = lad[st.rung];
            if (st.rung > prev && ! (r.use & kUp)) ++wrongWay;
            if (st.rung < prev && ! (r.use & kDown)) ++wrongWay;
        }
        if (st.phraseEnd)
        {
            ++cadences;
            if (lad[st.rung].resting) ++restingCadences;
        }
        prev = st.rung;
    }
    CHECK (notes > 800);
    CHECK (wrongWay == 0);
    CHECK (cadences > 50);
    CHECK (restingCadences * 10 > cadences * 6);   // most phrases end on a strong note
    std::printf ("walker: %d notes, %d phrases, %d end on a resting note\n", notes, cadences, restingCadences);

    // Locked deja vu repeats the loop exactly.
    s.dejavu = 1.0f;
    s.loopSteps = 8;
    std::vector<int> first, second;
    for (int i = 0; i < 8; ++i) w.step (s);   // settle onto the loop
    for (int i = 0; i < 8; ++i) { auto st = w.step (s); first.push_back (st.kind == Step::Note ? st.rung : -1 - st.kind); }
    for (int i = 0; i < 8; ++i) { auto st = w.step (s); second.push_back (st.kind == Step::Note ? st.rung : -1 - st.kind); }
    CHECK (first == second);

    // A zero weight takes a degree out entirely.
    Scale thin = scale;
    thin[2].weight = 0.0f;   // the E half-flat
    Ladder lt;
    lt.build (thin, -500, 2400);
    Walker w2 (99);
    w2.setLadder (lt);
    s.dejavu = 0.0f;
    for (int i = 0; i < 2000; ++i)
    {
        const auto st = w2.step (s);
        if (st.kind == Step::Note && ! st.replayed)
            CHECK (lt[st.rung].degree != 2 || lt[st.rung].weight > 0.0f);
    }
}

// Estimates the fundamental of a signal by autocorrelation.
static double estimatePitch (const std::vector<float>& x, double rate, double lo, double hi)
{
    const int minLag = (int) (rate / hi), maxLag = (int) (rate / lo);
    const int n = (int) x.size() - maxLag - 1;
    std::vector<double> ac ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag - 1; lag <= maxLag + 1; ++lag)
    {
        double sum = 0;
        for (int i = 0; i < n; ++i) sum += x[(size_t) i] * x[(size_t) (i + lag)];
        ac[(size_t) lag] = sum;
    }
    int best = minLag;
    for (int lag = minLag; lag <= maxLag; ++lag)
        if (ac[(size_t) lag] > ac[(size_t) best]) best = lag;
    const double a = ac[(size_t) best - 1], b = ac[(size_t) best], c = ac[(size_t) best + 1];
    const double shift = 0.5 * (a - c) / (a - 2 * b + c);
    return rate / (best + shift);
}

static void testSynth()
{
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        Synth synth;
        synth.prepare (rate);
        SynthSettings s;
        s.voice = 11;          // Harmonic: close to a sine
        s.a = 0.0f; s.b = 0.0f; s.c = 0.0f;
        s.sustain = 1.0f;
        s.space = 0.0f;
        s.droneOn = false;
        s.tone = 1.0f;

        NoteEvent on;
        on.pitch = 69.5;   // A4 a quarter tone sharp
        on.velocity = 1.0f;
        std::vector<float> l ((size_t) rate), r ((size_t) rate);
        synth.process (l.data(), r.data(), 512, s, &on, 1);
        synth.process (l.data() + 512, r.data() + 512, (int) rate - 512, s, nullptr, 0);

        std::vector<float> tail (l.begin() + (long) (rate * 0.5), l.end());
        const double f = estimatePitch (tail, rate, 300, 600);
        const double cents = 1200.0 * std::log2 (f / (440.0 * std::pow (2.0, 0.5 / 12.0)));
        std::printf ("pitch at %.0f Hz: %.2f Hz (%.2f cents off)\n", rate, f, cents);
        NEAR (cents, 0.0, 2.0);
    }

    // Every voice makes sound, and nothing blows up.
    for (int v = 0; v < numVoiceModels(); ++v)
    {
        Synth synth;
        synth.prepare (44100.0);
        SynthSettings s;
        s.voice = v;
        s.droneOn = true;
        s.droneActive = true;
        NoteEvent ev[3];
        ev[0].offset = 0;   ev[0].pitch = 57.5;
        ev[1].offset = 6000; ev[1].type = NoteEvent::Off;
        ev[2].offset = 8000; ev[2].pitch = 62.0; ev[2].ornament = 2; ev[2].grace = 63.5;
        std::vector<float> l (44100), r (44100);
        synth.process (l.data(), r.data(), 22050, s, ev, 3);
        synth.process (l.data() + 22050, r.data() + 22050, 22050, s, nullptr, 0);
        double sum = 0; bool finite = true; float peak = 0;
        for (size_t i = 0; i < l.size(); ++i)
        {
            finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]);
            sum += l[i] * l[i];
            peak = std::max (peak, std::abs (l[i]));
        }
        const double rms = std::sqrt (sum / (double) l.size());
        if (rms < 0.005 || ! finite || peak > 1.01f)
            std::printf ("voice %d (%s): rms %.4f peak %.3f\n", v, voiceModels()[(size_t) v].name, rms, peak);
        CHECK (finite);
        CHECK (rms > 0.005);
        CHECK (peak <= 1.01f);
    }
}

int main()
{
    testScales();
    testScala();
    testWalker();
    testSynth();
    if (failures == 0)
        std::printf ("all engine tests passed\n");
    return failures == 0 ? 0 : 1;
}
