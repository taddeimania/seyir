#include "Tuning.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace seyir
{
namespace
{
// Just ratios used by the ragas, in cents.
constexpr double r_ = 111.731;   // 16/15  komal re
constexpr double r2 = 90.225;    // 256/243 ati-komal re
constexpr double R_ = 203.910;   // 9/8    shuddha Re
constexpr double g_ = 315.641;   // 6/5    komal ga
constexpr double g2 = 294.135;   // 32/27  ati-komal ga
constexpr double G_ = 386.314;   // 5/4    shuddha Ga
constexpr double M_ = 498.045;   // 4/3    shuddha Ma
constexpr double Mt = 590.224;   // 45/32  tivra Ma
constexpr double P_ = 701.955;   // 3/2    Pa
constexpr double d_ = 813.686;   // 8/5    komal dha
constexpr double d2 = 792.180;   // 128/81 ati-komal dha
constexpr double D_ = 884.359;   // 5/3    shuddha Dha
constexpr double n_ = 1017.596;  // 9/5    komal ni
constexpr double N_ = 1088.269;  // 15/8   shuddha Ni

// Turkish theory divides the whole tone into 9 commas of 1/53 octave.
constexpr double kComma = 1200.0 / 53.0;
std::vector<double> commas (std::initializer_list<int> c)
{
    std::vector<double> v;
    for (int x : c) v.push_back (x * kComma);
    return v;
}
double comma (int c) { return c * kComma; }

std::vector<double> equal (int steps, double period)
{
    std::vector<double> v;
    for (int i = 0; i < steps; ++i) v.push_back (period * i / steps);
    return v;
}
} // namespace

const char* familyName (Family f)
{
    switch (f)
    {
        case Family::Arabic:    return "Arabic maqam";
        case Family::Turkish:   return "Turkish makam";
        case Family::Raga:      return "Hindustani raga";
        case Family::World:     return "Gamelan & more";
        case Family::Reference: return "Just & equal";
        case Family::Custom:    return "Scala file";
    }
    return "";
}

// ---------------------------------------------------------------------------
// Ajnas
// ---------------------------------------------------------------------------
// Arabic ajnas use the usual quarter-tone (24-EDO) spelling. Players place the
// neutral notes (the 150s and 350s) by ear and by region; drag them on the
// ladder to hear the difference.
const std::vector<Jins>& ajnas()
{
    static const std::vector<Jins> list {
        { "Rast",      Family::Arabic,  { 0, 200, 350, 500, 700 } },
        { "Nahawand",  Family::Arabic,  { 0, 200, 300, 500, 700 } },
        { "Ajam",      Family::Arabic,  { 0, 200, 400, 500, 700 } },
        { "Bayati",    Family::Arabic,  { 0, 150, 300, 500 } },
        { "Kurd",      Family::Arabic,  { 0, 100, 300, 500 } },
        { "Hijaz",     Family::Arabic,  { 0, 100, 400, 500 } },
        { "Saba",      Family::Arabic,  { 0, 150, 300, 400 } },
        { "Sikah",     Family::Arabic,  { 0, 150, 350 } },
        { "Nikriz",    Family::Arabic,  { 0, 200, 300, 600, 700 } },

        { "Rast",      Family::Turkish, commas ({ 0, 9, 17, 22, 31 }) },
        { "Uşşak",     Family::Turkish, commas ({ 0, 8, 13, 22 }) },
        { "Hüseyni",   Family::Turkish, commas ({ 0, 8, 13, 22, 31 }) },
        { "Hicaz",     Family::Turkish, commas ({ 0, 5, 17, 22 }) },
        { "Kürdi",     Family::Turkish, commas ({ 0, 4, 13, 22 }) },
        { "Buselik",   Family::Turkish, commas ({ 0, 9, 13, 22, 31 }) },
        { "Çargah",    Family::Turkish, commas ({ 0, 9, 18, 22, 31 }) },
    };
    return list;
}

int jinsIndex (const char* name, Family f)
{
    if (name == nullptr)
        return -1;
    const auto& a = ajnas();
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].family == f && std::strcmp (a[i].name, name) == 0)
            return (int) i;
    return -1;
}

std::vector<int> ajnasOf (Family f)
{
    std::vector<int> v;
    const auto& a = ajnas();
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].family == f)
            v.push_back ((int) i);
    return v;
}

// ---------------------------------------------------------------------------
// Modes
// ---------------------------------------------------------------------------
namespace
{
ModeDef maqam (const char* name, Family f, const char* blurb, const char* lower, double ghammaz,
               const char* upper, const char* upperDown = nullptr, std::vector<double> tail = {},
               std::vector<double> strong = {})
{
    ModeDef m;
    m.name = name; m.family = f; m.blurb = blurb;
    m.lower = lower; m.upper = upper; m.ghammaz = ghammaz; m.upperDown = upperDown;
    m.tail = std::move (tail);
    m.strong = std::move (strong);
    m.strong.push_back (ghammaz);
    return m;
}

ModeDef scale (const char* name, Family f, const char* blurb, std::vector<double> up,
               std::vector<double> down = {}, std::vector<double> strong = {}, double period = 1200.0)
{
    ModeDef m;
    m.name = name; m.family = f; m.blurb = blurb;
    m.up = std::move (up); m.down = std::move (down); m.strong = std::move (strong);
    m.period = period;
    return m;
}
} // namespace

const std::vector<ModeDef>& modes()
{
    using F = Family;
    static const std::vector<ModeDef> list {
        // --- Arabic -----------------------------------------------------------
        maqam ("Rast", F::Arabic, "Rast on the tonic, Rast above; descends through Nahawand",
               "Rast", 700, "Rast", "Nahawand"),
        maqam ("Bayati", F::Arabic, "Bayati on the tonic, Nahawand on the fourth",
               "Bayati", 500, "Nahawand"),
        maqam ("Hijaz", F::Arabic, "Hijaz on the tonic, Rast on the fourth; descends through Nahawand",
               "Hijaz", 500, "Rast", "Nahawand"),
        maqam ("Saba", F::Arabic, "Saba on the tonic, Hijaz on the third; doesn't close on the octave",
               "Saba", 300, "Hijaz", nullptr, { 1000, 1100 }),
        maqam ("Sikah", F::Arabic, "Sikah on the tonic, Rast on the third",
               "Sikah", 350, "Rast"),
        maqam ("Huzam", F::Arabic, "Sikah on the tonic, Hijaz on the third",
               "Sikah", 350, "Hijaz", nullptr, { 1050 }),
        maqam ("Nahawand", F::Arabic, "Nahawand on the tonic, Hijaz up and Kurd down on the fifth",
               "Nahawand", 700, "Hijaz", "Kurd"),
        maqam ("Kurd", F::Arabic, "Kurd on the tonic, Nahawand on the fourth",
               "Kurd", 500, "Nahawand"),
        maqam ("Ajam", F::Arabic, "Ajam on the tonic and the fifth: the major scale",
               "Ajam", 700, "Ajam"),
        maqam ("Nikriz", F::Arabic, "Nikriz on the tonic, Nahawand on the fifth",
               "Nikriz", 700, "Nahawand"),

        // --- Turkish (53 commas to the octave) ----------------------------------
        maqam ("Rast", F::Turkish, "Rast pentachord with Rast above; the third is high, near 5/4",
               "Rast", comma (31), "Rast"),
        maqam ("Uşşak", F::Turkish, "Uşşak tetrachord with Buselik on the fourth",
               "Uşşak", comma (22), "Buselik"),
        maqam ("Hüseyni", F::Turkish, "Hüseyni pentachord with Uşşak on the fifth",
               "Hüseyni", comma (31), "Uşşak"),
        maqam ("Hicaz", F::Turkish, "Hicaz tetrachord with Rast on the fourth",
               "Hicaz", comma (22), "Rast"),
        maqam ("Kürdi", F::Turkish, "Kürdi tetrachord with Buselik on the fourth",
               "Kürdi", comma (22), "Buselik"),
        maqam ("Nihavend", F::Turkish, "Buselik pentachord with Kürdi on the fifth",
               "Buselik", comma (31), "Kürdi"),

        // --- Ragas: just intonation, ascending and descending notes, vadi/samvadi
        scale ("Yaman", F::Raga, "Tivra Ma; climbs N R G M D N, Pa only on the way down",
               { 0, R_, G_, Mt, D_, N_ }, { 0, R_, G_, Mt, P_, D_, N_ }, { G_, N_ }),
        scale ("Bhupali", F::Raga, "Five notes, no Ma or Ni; leans on Ga and Dha",
               { 0, R_, G_, P_, D_ }, {}, { G_, D_ }),
        scale ("Bhairav", F::Raga, "Komal Re and Dha, sung low and slow; dawn",
               { 0, r_, G_, M_, P_, d_, N_ }, {}, { d_, r_ }),
        scale ("Bhimpalasi", F::Raga, "Skips Re and Dha going up; afternoon",
               { 0, g_, M_, P_, n_ }, { 0, R_, g_, M_, P_, D_, n_ }, { M_ }),
        scale ("Malkauns", F::Raga, "Five notes, all flat; midnight",
               { 0, g_, M_, d_, n_ }, {}, { M_ }),
        scale ("Todi", F::Raga, "Very low Re, Ga and Dha; Pa barely touched",
               { 0, r2, g2, Mt, d2, N_ }, { 0, r2, g2, Mt, P_, d2, N_ }, { d2, g2 }),
        scale ("Marwa", F::Raga, "Komal Re, tivra Ma, no Pa; sunset",
               { 0, r_, G_, Mt, D_, N_ }, {}, { r_, D_ }),
        scale ("Bhairavi", F::Raga, "Every flat note; often the last raga of a concert",
               { 0, r_, g_, M_, P_, d_, n_ }, {}, { M_ }),

        // --- Gamelan and more ----------------------------------------------------
        scale ("Slendro", F::World, "Five near-equal steps; every gamelan is tuned its own way",
               { 0, 228, 484, 728, 960 }, {}, { 728 }),
        scale ("Pelog", F::World, "Seven uneven steps; every gamelan is tuned its own way",
               { 0, 120, 270, 540, 670, 785, 950 }, {}, { 540, 670 }),
        scale ("Thai 7-equal", F::World, "Seven equal steps of about 171 cents",
               equal (7, 1200), {}, { equal (7, 1200)[4] }),

        // --- Just & equal ----------------------------------------------------------
        scale ("Harmonics 8-16", F::Reference, "Partials 8 to 16 of one string",
               { 0, 203.910, 386.314, 551.318, 701.955, 840.528, 968.826, 1088.269 }, {}, { 701.955 }),
        scale ("Just major", F::Reference, "Major scale from 5-limit ratios",
               { 0, R_, G_, M_, P_, D_, N_ }, {}, { P_ }),
        scale ("12-TET major", F::Reference, "The piano, for comparison",
               { 0, 200, 400, 500, 700, 900, 1100 }, {}, { 700 }),
        scale ("24-EDO", F::Reference, "Every quarter tone. Lower the weights to carve a scale",
               equal (24, 1200), {}, { 700 }),
        scale ("Bohlen-Pierce", F::Reference, "13 steps to the tritave (3/1), no octave at all",
               equal (13, 1901.955), {}, { equal (13, 1901.955)[6] }, 1901.955),
    };
    return list;
}

int numModes() { return (int) modes().size(); }

int defaultLower (int m)
{
    if (m < 0 || m >= numModes()) return -1;
    const auto& d = modes()[(size_t) m];
    return jinsIndex (d.lower, d.family);
}

int defaultUpper (int m)
{
    if (m < 0 || m >= numModes()) return -1;
    const auto& d = modes()[(size_t) m];
    return jinsIndex (d.upper, d.family);
}

// ---------------------------------------------------------------------------
// Building
// ---------------------------------------------------------------------------
void Scale::setName (const std::string& s)
{
    std::memset (name, 0, sizeof (name));
    std::strncpy (name, s.c_str(), sizeof (name) - 1);
}

void Scale::normalise()
{
    if (period <= 1.0) period = 1200.0;
    for (int i = 0; i < count; ++i)
    {
        auto& c = degrees[(size_t) i].cents;
        c = std::fmod (c, period);
        if (c < 0.0) c += period;
        if (period - c < 0.5) c = 0.0;
    }
    std::sort (degrees.begin(), degrees.begin() + count,
               [] (const Degree& a, const Degree& b) { return a.cents < b.cents; });

    int out = 0;
    for (int i = 0; i < count; ++i)
    {
        const auto& d = degrees[(size_t) i];
        if (out > 0 && std::abs (d.cents - degrees[(size_t) out - 1].cents) < 1.0)
        {
            auto& prev = degrees[(size_t) out - 1];
            prev.use |= d.use;
            prev.weight = std::max (prev.weight, d.weight);
            prev.resting = prev.resting || d.resting;
            continue;
        }
        degrees[(size_t) out++] = d;
    }
    count = out;
}

namespace
{
void add (Scale& s, double cents, uint8_t use)
{
    if (s.count >= kMaxDegrees) return;
    Degree d;
    d.cents = cents;
    d.use = use;
    s.degrees[(size_t) s.count++] = d;
}

const Jins* jinsAt (int index)
{
    const auto& a = ajnas();
    return index >= 0 && index < (int) a.size() ? &a[(size_t) index] : nullptr;
}
} // namespace

Scale buildScale (int modeIndex, int lower, int upper)
{
    Scale s;
    if (modeIndex < 0 || modeIndex >= numModes())
        modeIndex = 0;
    const auto& m = modes()[(size_t) modeIndex];
    s.period = m.period;

    if (m.isMaqam())
    {
        const auto* lo = jinsAt (lower >= 0 ? lower : jinsIndex (m.lower, m.family));
        const auto* hi = jinsAt (upper >= 0 ? upper : jinsIndex (m.upper, m.family));
        // The descending variant only applies to the maqam's own upper jins:
        // once you swap it, you hear what you chose both ways.
        const bool ownUpper = upper < 0 || upper == jinsIndex (m.upper, m.family);
        const auto* down = ownUpper && m.upperDown ? jinsAt (jinsIndex (m.upperDown, m.family)) : nullptr;

        std::string name = m.name;
        if (! ownUpper || (lower >= 0 && lower != jinsIndex (m.lower, m.family)))
            name = std::string (lo ? lo->name : "?") + " + " + (hi ? hi->name : "?");
        s.setName (name);

        if (lo)
            for (double c : lo->cents)
                if (c < m.ghammaz - 0.5)
                    add (s, c, kBoth);
        if (hi)
            for (double c : hi->cents)
                if (m.ghammaz + c < s.period - 0.5)
                    add (s, m.ghammaz + c, down ? kUp : kBoth);
        if (down)
            for (double c : down->cents)
                if (m.ghammaz + c < s.period - 0.5)
                    add (s, m.ghammaz + c, kDown);
        for (double c : m.tail)
            add (s, c, kBoth);
        if (s.count == 0)
            add (s, 0.0, kBoth);
    }
    else
    {
        s.setName (m.name);
        const bool split = ! m.down.empty();
        for (double c : m.up)   add (s, c, split ? kUp : kBoth);
        for (double c : m.down) add (s, c, kDown);
    }

    s.normalise();

    // Weights: the tonic strongest, the mode's own strong notes next.
    for (int i = 0; i < s.count; ++i)
    {
        auto& d = s[i];
        d.weight = 0.45f;
        if (d.cents < 0.5)
        {
            d.weight = 1.0f;
            d.resting = true;
        }
        for (double c : m.strong)
            if (std::abs (std::fmod (c, s.period) - d.cents) < 1.0)
            {
                d.weight = 0.8f;
                d.resting = true;
            }
    }
    return s;
}

// ---------------------------------------------------------------------------
// Scala
// ---------------------------------------------------------------------------
namespace
{
std::string trim (const std::string& s)
{
    const auto a = s.find_first_not_of (" \t\r\n");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of (" \t\r\n");
    return s.substr (a, b - a + 1);
}
} // namespace

bool parseScalaPitch (const std::string& tokenIn, double& cents)
{
    const auto token = trim (tokenIn);
    if (token.empty()) return false;
    try
    {
        if (token.find ('.') != std::string::npos)
        {
            size_t used = 0;
            cents = std::stod (token, &used);
            return used > 0;
        }
        const auto slash = token.find ('/');
        size_t used = 0;
        const double num = std::stod (token.substr (0, slash), &used);
        if (used == 0) return false;
        double den = 1.0;
        if (slash != std::string::npos)
            den = std::stod (token.substr (slash + 1));
        if (num <= 0.0 || den <= 0.0) return false;
        cents = 1200.0 * std::log2 (num / den);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool parseScala (const std::string& text, Scale& out, std::string& error)
{
    std::istringstream in (text);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline (in, line))
    {
        if (! line.empty() && line[0] == '!')
            continue;
        lines.push_back (line);
    }
    if (lines.size() < 2)
    {
        error = "Not a Scala file: it needs a description line and a note count.";
        return false;
    }
    const std::string description = trim (lines[0]);
    int count = 0;
    try { count = std::stoi (trim (lines[1])); }
    catch (...) { error = "The note count line isn't a number."; return false; }
    if (count < 1 || count >= kMaxDegrees)
    {
        error = "Seyir reads scales of 1 to " + std::to_string (kMaxDegrees - 1) + " notes.";
        return false;
    }
    if ((int) lines.size() < 2 + count)
    {
        error = "The file lists fewer notes than it says it has.";
        return false;
    }

    std::vector<double> pitches;
    for (int i = 0; i < count; ++i)
    {
        std::istringstream tok (lines[(size_t) (2 + i)]);
        std::string first;
        tok >> first;
        double c = 0.0;
        if (! parseScalaPitch (first, c))
        {
            error = "Couldn't read note " + std::to_string (i + 1) + ": \"" + trim (lines[(size_t) (2 + i)]) + "\".";
            return false;
        }
        pitches.push_back (c);
    }

    Scale s;
    s.period = pitches.back();
    if (s.period < 10.0)
    {
        error = "The last note (the period) must be above the first.";
        return false;
    }
    add (s, 0.0, kBoth);
    for (size_t i = 0; i + 1 < pitches.size(); ++i)
        add (s, pitches[i], kBoth);
    s.normalise();
    for (int i = 0; i < s.count; ++i)
    {
        s[i].weight = s[i].cents < 0.5 ? 1.0f : 0.5f;
        s[i].resting = s[i].cents < 0.5;
    }
    s.setName (description.empty() ? "Scala file" : description);
    out = s;
    return true;
}

std::string nearestNoteName (int tonicMidi, double fineCents, double cents)
{
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double pitch = tonicMidi + (fineCents + cents) / 100.0;
    const int nearest = (int) std::lround (pitch);
    const int off = (int) std::lround ((pitch - nearest) * 100.0);
    std::string s = names[((nearest % 12) + 12) % 12];
    if (off != 0)
        s += (off > 0 ? " +" : " ") + std::to_string (off);
    return s;
}
} // namespace seyir
