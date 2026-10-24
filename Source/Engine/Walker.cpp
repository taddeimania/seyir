#include "Walker.h"

#include <algorithm>
#include <cmath>

namespace seyir
{
// ---------------------------------------------------------------------------
// Ladder
// ---------------------------------------------------------------------------
void Ladder::build (const Scale& s, double low, double high)
{
    count = 0;
    home = 0;
    if (s.count <= 0 || s.period <= 0.0)
        return;

    const int k0 = (int) std::floor (low / s.period) - 1;
    const int k1 = (int) std::ceil (high / s.period) + 1;
    for (int k = k0; k <= k1 && count < kMaxRungs; ++k)
        for (int i = 0; i < s.count && count < kMaxRungs; ++i)
        {
            const double c = s[i].cents + k * s.period;
            if (c < low - 0.5 || c > high + 0.5)
                continue;
            Rung r;
            r.cents = c;
            r.degree = i;
            r.octave = k;
            r.use = s[i].use;
            r.weight = s[i].weight;
            r.resting = s[i].resting;
            if (i == 0 && k == 0)
                home = count;
            rungs[(size_t) count++] = r;
        }
}

int Ladder::nearest (double cents) const
{
    int best = 0;
    double bestD = 1e9;
    for (int i = 0; i < count; ++i)
    {
        const double d = std::abs (rungs[(size_t) i].cents - cents);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Walker
// ---------------------------------------------------------------------------
Walker::Walker (uint32_t seed) : state (seed ? seed : 1u) {}

float Walker::rnd()
{
    // xorshift32
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return (float) (state >> 8) * (1.0f / 16777216.0f);
}

int Walker::remap (int rung, const Ladder& from) const
{
    if (rung < 0 || rung >= from.count || lad.count == 0)
        return rung < 0 ? -1 : 0;
    if (from.count == lad.count)
        return rung;
    // Same degree and octave if the new ladder has it, else the nearest pitch.
    const auto& r = from[rung];
    for (int i = 0; i < lad.count; ++i)
        if (lad[i].degree == r.degree && lad[i].octave == r.octave)
            return i;
    return lad.nearest (r.cents);
}

void Walker::setLadder (const Ladder& l)
{
    const Ladder old = lad;
    lad = l;
    if (old.count == 0)
    {
        cur = last = -1;
        return;
    }
    cur = remap (cur, old);
    last = remap (last, old);
    for (int i = 0; i < kMaxLoop; ++i)
        if (loopValid[(size_t) i] && loop[(size_t) i].kind == Step::Note)
        {
            auto& s = loop[(size_t) i];
            s.rung = remap (s.rung, old);
            if (s.grace >= 0) s.grace = remap (s.grace, old);
        }
}

void Walker::restart()
{
    cur = last = -1;
    dir = 1;
    notesInPhrase = holdLeft = restLeft = phraseIndex = 0;
    loopPos = 0;
    pendingStart = -1;
}

void Walker::reseed (uint32_t seed)
{
    state = seed ? seed : 1u;
    loopValid.fill (false);
    restart();
}

void Walker::startFrom (int rung)
{
    pendingStart = std::clamp (rung, 0, std::max (0, lad.count - 1));
}

void Walker::stop()
{
    holdLeft = restLeft = 0;
    notesInPhrase = 0;
}

Step Walker::step (const WalkSettings& s)
{
    if (lad.count == 0)
        return {};

    const int len = std::clamp (s.loopSteps, 1, kMaxLoop);
    const int pos = loopPos % len;
    loopPos = (pos + 1) % len;

    Step out;
    if (pendingStart >= 0)
    {
        // A key in FOLLOW mode: play it now, and grow a phrase from it.
        cur = pendingStart;
        pendingStart = -1;
        notesInPhrase = 1;
        holdLeft = restLeft = 0;
        dir = rnd() < 0.5f ? 1 : -1;
        out.kind = Step::Note;
        out.rung = cur;
        out.velocity = 0.85f;
    }
    else
    {
        const bool locked = s.dejavu >= 0.995f;
        const bool replay = loopValid[(size_t) pos] && (locked || rnd() < s.dejavu);
        if (replay)
        {
            out = loop[(size_t) pos];
            out.replayed = true;
            out.rung = std::clamp (out.rung, 0, lad.count - 1);
            if (out.kind == Step::Note)
            {
                last = cur;
                cur = out.rung;
            }
            // When it goes back to inventing, it starts from a clean slate.
            holdLeft = restLeft = 0;
            if (out.phraseEnd) { notesInPhrase = 0; ++phraseIndex; }
            return out;
        }
        out = generate (s);
    }

    loop[(size_t) pos] = out;
    loopValid[(size_t) pos] = true;
    return out;
}

Step Walker::generate (const WalkSettings& s)
{
    Step out;
    if (holdLeft > 0)
    {
        --holdLeft;
        out.kind = Step::Hold;
        return out;
    }
    if (restLeft > 0)
    {
        --restLeft;
        out.kind = Step::Rest;
        return out;
    }

    const int phraseLen = std::max (2, s.phrase);
    const bool cadence = notesInPhrase >= phraseLen - 1;
    const int next = choose (s, cadence);
    const auto& r = lad[next];

    // Length: busier with DENSITY; strong notes and the last note held longer.
    const float density = std::clamp (s.density, 0.0f, 1.0f);
    float mean = 1.0f + (1.0f - density) * 2.5f;
    if (r.resting) mean *= 1.0f + 0.4f * s.gravity;
    if (cadence)   mean *= 1.8f;
    int length = 1 + (int) std::floor (-std::log (std::max (1e-4f, rnd())) * (mean - 1.0f) + rnd() * 0.5f);
    length = std::clamp (length, 1, 12);

    out.kind = Step::Note;
    out.rung = next;
    out.length = length;
    out.phraseEnd = cadence;
    out.velocity = std::clamp (0.6f + 0.25f * r.weight + (notesInPhrase == 0 ? 0.1f : 0.0f) + (rnd() - 0.5f) * 0.12f,
                               0.2f, 1.0f);

    // Ornaments.
    if (cur >= 0 && rnd() < s.ornament)
    {
        const float pick = rnd();
        if (length >= 2 && pick > 0.7f)
            out.ornament = Ornament::Shake;
        else if (pick > 0.35f && std::abs (next - cur) <= 3)
            out.ornament = Ornament::Slide;
        else
        {
            // Grace: flick in from the neighbour above (or below at the top).
            const int g = next + 1 < lad.count ? next + 1 : next - 1;
            if (g >= 0 && g != next && lad[g].weight > 0.0f)
            {
                out.ornament = Ornament::Grace;
                out.grace = g;
            }
        }
    }

    last = cur;
    cur = next;
    holdLeft = length - 1;
    ++notesInPhrase;

    if (cadence)
    {
        notesInPhrase = 0;
        ++phraseIndex;
        restLeft = (int) std::floor ((1.0f - density) * 3.0f + rnd() * 2.0f);
    }
    return out;
}

int Walker::choose (const WalkSettings& s, bool cadence)
{
    const int n = lad.count;
    if (cur < 0 || cur >= n)
    {
        // The seyir starts at the tonic.
        dir = 1;
        return std::clamp (lad.home, 0, n - 1);
    }

    // Where this phrase is headed: phrases climb away from the tonic over a
    // cycle and come back down to it.
    constexpr int kArcPhrases = 6;
    const double top = lad[n - 1].cents;
    const double homeC = lad[lad.home].cents;
    const int p = phraseIndex % kArcPhrases;
    const double shape = std::sin (3.14159265 * (double) p / (double) (kArcPhrases - 1));
    const double target = homeC + shape * (top - homeC) * 0.75;
    const float arc = std::clamp (s.arc, 0.0f, 1.0f);

    // Direction: keep going the same way for a while; the arc tips the odds.
    float pUp = dir > 0 ? 0.68f : 0.32f;
    const double toTarget = target - lad[cur].cents;
    pUp += (float) std::clamp (toTarget / 600.0, -1.0, 1.0) * 0.3f * arc;
    if (cadence)
        pUp = 0.5f + (float) std::clamp ((homeC - lad[cur].cents) / 1200.0, -0.4, 0.4) * (0.5f + s.gravity);
    if (cur >= n - 1) pUp = 0.0f;
    if (cur <= 0)     pUp = 1.0f;
    int want = rnd() < pUp ? 1 : -1;

    // A phrase's last note may reach a little further to find a place to rest.
    const int maxLeap = 1 + (int) std::lround (std::clamp (s.leap, 0.0f, 1.0f) * 7.0f) + (cadence ? 2 : 0);
    const float spread = 0.45f + s.leap * 3.0f;
    const float sharp = 0.5f + s.gravity * 3.0f;
    const double width = 250.0 + (1.0 - arc) * 4000.0;

    std::array<float, kMaxRungs> w {};
    auto fill = [&] (int d) -> float
    {
        float total = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const int dist = (i - cur) * d;
            if (dist <= 0 || dist > maxLeap)
                continue;
            const auto& r = lad[i];
            if (r.weight <= 0.0f || ! (r.use & (d > 0 ? kUp : kDown)))
                continue;
            float x = std::exp (-(float) (dist - 1) / spread);
            x *= std::pow (r.weight, sharp);
            x *= (float) std::exp (-std::abs (r.cents - target) / width);
            if (cadence)
                x *= r.resting ? 1.0f + s.gravity * 10.0f : 0.12f;
            if (i == last)
                x *= 0.6f;
            w[(size_t) i] += x;
            total += x;
        }
        return total;
    };

    // A phrase's last note looks both ways for somewhere to rest.
    float total = fill (want);
    if (cadence || total <= 0.0f)
        total += fill (-want);
    if (cadence && lad[cur].resting && lad[cur].weight > 0.0f)
    {
        // Already on a resting note: staying put is a fine way to end.
        const float stay = std::pow (lad[cur].weight, sharp) * (1.0f + s.gravity * 10.0f);
        w[(size_t) cur] += stay;
        total += stay;
    }
    if (total <= 0.0f)
        return cur;   // nowhere to go: repeat the note

    int chosen = -1;
    float pick = rnd() * total;
    for (int i = 0; i < n && chosen < 0; ++i)
    {
        pick -= w[(size_t) i];
        if (pick <= 0.0f && w[(size_t) i] > 0.0f)
            chosen = i;
    }
    for (int i = n; chosen < 0 && --i >= 0;)
        if (w[(size_t) i] > 0.0f)
            chosen = i;
    if (chosen < 0)
        return cur;
    if (chosen != cur)
        dir = chosen > cur ? 1 : -1;
    return chosen;
}
} // namespace seyir
