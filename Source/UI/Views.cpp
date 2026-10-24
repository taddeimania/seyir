#include "Views.h"

#include <cmath>

namespace seyir::ui
{
using namespace juce;

namespace
{
const char* kPitchNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

void triangle (Graphics& g, Point<float> c, float size, bool up, Colour col)
{
    Path p;
    if (up) p.addTriangle (c.x - size, c.y + size * 0.6f, c.x + size, c.y + size * 0.6f, c.x, c.y - size * 0.8f);
    else    p.addTriangle (c.x - size, c.y - size * 0.6f, c.x + size, c.y - size * 0.6f, c.x, c.y + size * 0.8f);
    g.setColour (col);
    g.fillPath (p);
}
} // namespace

// ===========================================================================
// Ladder
// ===========================================================================
LadderView::LadderView (SeyirProcessor& p) : proc (p)
{
    setTooltip ("The scale, one octave from the tonic, against the piano's twelve notes (dashed). "
                "Click a note to hear it. Drag up or down to change how strongly the melody is drawn to it "
                "(all the way down takes it out). Shift-drag sideways to retune it. Double-click to undo. "
                "Arrows: used only going up, or only coming down.");
    setRepaintsOnMouseActivity (false);
}

Rectangle<float> LadderView::plot() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (22.0f).withTrimmedRight (22.0f)
                                     .withTrimmedTop (16.0f).withTrimmedBottom (58.0f);
}

float LadderView::xOf (double cents) const
{
    const auto r = plot();
    const double period = proc.scale().period;
    return r.getX() + (float) (cents / period) * r.getWidth();
}

float LadderView::barTop (float weight) const
{
    const auto r = plot();
    return r.getBottom() - 14.0f - weight * (r.getHeight() - 30.0f);
}

int LadderView::degreeAt (Point<float> pos) const
{
    const auto& s = proc.scale();
    int best = -1;
    float bestD = 16.0f;
    for (int i = 0; i < s.count; ++i)
    {
        const float d = std::abs (xOf (s[i].cents) - pos.x);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void LadderView::refresh()
{
    const auto& s = proc.scale();
    uint32_t h = (uint32_t) s.count * 2654435761u;
    for (int i = 0; i < s.count; ++i)
    {
        h ^= (uint32_t) std::lround (s[i].cents * 10.0) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= (uint32_t) std::lround (s[i].weight * 1000.0f) + (h << 6) + (h >> 2);
    }
    h ^= (uint32_t) (proc.geti (ids::tonic) * 131 + proc.geti (ids::fine) * 7919);
    lit = proc.soundingDegree();
    h ^= (uint32_t) (lit + 7) * 40503u;
    h ^= (uint32_t) (hover + 3) * 9176u;
    if (h != shownHash)
    {
        shownHash = h;
        repaint();
    }
}

void LadderView::paint (Graphics& g)
{
    const auto r = plot();
    const auto& s = proc.scale();
    const auto& base = proc.baseScale();
    const auto& edits = proc.edits();
    const int tonic = proc.geti (ids::tonic);
    const double fine = proc.getParam (ids::fine);
    const double tonicAbs = tonic + fine / 100.0;

    // The piano's notes: dashed lines and names along the bottom.
    const float dash[] { 3.0f, 4.0f };
    for (int j = (int) std::ceil (tonicAbs - 1e-6); (j - tonicAbs) * 100.0 <= s.period + 0.5; ++j)
    {
        const double c = (j - tonicAbs) * 100.0;
        const float x = xOf (c);
        g.setColour (colours::inkDim.withAlpha (0.35f));
        g.drawDashedLine ({ x, r.getY(), x, r.getBottom() }, dash, 2, 1.0f);
        drawText (g, kPitchNames[((j % 12) + 12) % 12], { x - 14.0f, r.getY() - 15.0f, 28.0f, 13.0f }, 10.5f,
                  colours::inkDim, true);
    }

    // Baseline, and the octave (period) at the right edge.
    g.setColour (colours::ink.withAlpha (0.8f));
    g.drawLine (r.getX() - 10.0f, r.getBottom(), r.getRight() + 10.0f, r.getBottom(), 1.5f);
    const float xp = xOf (s.period);
    g.setColour (colours::cream.withAlpha (0.35f));
    g.drawLine (xp, barTop (1.0f), xp, r.getBottom(), 3.0f);
    drawLed (g, { xp, barTop (1.0f) }, 7.0f, colours::cream, false);
    drawText (g, std::abs (s.period - 1200.0) < 0.5 ? String ("OCTAVE") : String (roundToInt (s.period)),
              { xp - 30.0f, r.getBottom() + 6.0f, 60.0f, 13.0f }, 10.5f, colours::inkDim, true);

    // Labels alternate rows when the degrees sit close together.
    float minGap = 1e9f;
    for (int i = 1; i < s.count; ++i)
        minGap = jmin (minGap, xOf (s[i].cents) - xOf (s[i - 1].cents));
    const bool stagger = minGap < 46.0f;

    for (int i = 0; i < s.count; ++i)
    {
        const auto& d = s[i];
        const float x = xOf (d.cents);
        const float top = barTop (d.weight);
        const bool on = d.weight > 0.0f;

        // Bar: how strongly the melody leans on this note.
        if (on)
        {
            g.setColour (d.resting ? colours::cream : colours::ink.withAlpha (0.55f));
            g.fillRect (Rectangle<float> (x - 3.0f, top, 6.0f, r.getBottom() - top));
        }

        // Head.
        const bool sounding = i == lit;
        const auto head = Point<float> (x, on ? top : r.getBottom() - 14.0f);
        if (sounding)
            drawLed (g, head, 11.0f, colours::brand, true, 0.6f);
        else if (on)
        {
            g.setColour (colours::cream);
            g.fillEllipse (Rectangle<float> (20.0f, 20.0f).withCentre (head));
            g.setColour (colours::creamShade.darker (0.3f));
            g.drawEllipse (Rectangle<float> (20.0f, 20.0f).withCentre (head), 1.0f);
        }
        else
            drawLed (g, head, 9.0f, colours::cream, false);
        if (d.resting && on)
        {
            g.setColour (colours::ink);
            g.drawEllipse (Rectangle<float> (28.0f, 28.0f).withCentre (head), 1.5f);
        }
        if (i == hover || i == dragging)
        {
            g.setColour (colours::brand);
            g.drawEllipse (Rectangle<float> (32.0f, 32.0f).withCentre (head), 2.0f);
        }

        // Direction marks.
        if (d.use == kUp)   triangle (g, { x, head.y - 22.0f }, 5.0f, true,  colours::ink);
        if (d.use == kDown) triangle (g, { x, head.y - 22.0f }, 5.0f, false, colours::ink);

        // Labels: the nearest piano note and its offset, then the cents.
        const float ly = r.getBottom() + 6.0f + (stagger && (i & 1) ? 24.0f : 0.0f);
        const auto name = String::fromUTF8 (nearestNoteName (tonic, fine, d.cents).c_str());
        drawText (g, name, { x - 30.0f, ly, 60.0f, 13.0f }, 12.0f, sounding ? colours::brand : colours::lcd, true);
        String cents = String (roundToInt (d.cents));
        const double off = i < kMaxDegrees ? edits.cents[(size_t) i] : 0.0;
        const bool retuned = std::abs (off) > 0.05 && i < base.count;
        if (retuned)
            cents += (off > 0 ? " +" : " ") + String (roundToInt (off));
        drawText (g, cents, { x - 30.0f, ly + 13.0f, 60.0f, 12.0f }, 10.5f, retuned ? colours::brand : colours::inkDim, true);
    }
}

void LadderView::mouseMove (const MouseEvent& e)
{
    const int h = degreeAt (e.position);
    if (h != hover)
    {
        hover = h;
        refresh();
    }
}

void LadderView::mouseExit (const MouseEvent&)
{
    if (hover != -1)
    {
        hover = -1;
        refresh();
    }
}

void LadderView::mouseDown (const MouseEvent& e)
{
    dragging = degreeAt (e.position);
    if (dragging < 0)
        return;
    retuning = e.mods.isShiftDown();
    dragWeight = proc.scale()[dragging].weight;
    dragCents = proc.edits().cents[(size_t) dragging];
    proc.audition (dragging, true);
    repaint();
}

void LadderView::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0)
        return;
    const auto r = plot();
    if (retuning)
    {
        if (dragging == 0)
            return;   // the tonic is the reference
        const double perPixel = proc.scale().period / r.getWidth();
        const double before = proc.edits().cents[(size_t) dragging];
        const double now = std::round (dragCents + e.getDistanceFromDragStartX() * perPixel * 0.5);
        if (std::abs (now - before) >= 1.0)
        {
            proc.setDegreeCents (dragging, now);
            if (std::abs (now - before) >= 3.0 || std::fmod (std::abs (now), 5.0) < 1.0)
                proc.audition (dragging, true);   // hear the new pitch
        }
    }
    else
    {
        const float w = jlimit (0.0f, 1.0f, dragWeight - (float) e.getDistanceFromDragStartY() / (r.getHeight() - 30.0f));
        proc.setDegreeWeight (dragging, w);
    }
}

void LadderView::mouseUp (const MouseEvent&)
{
    if (dragging >= 0)
        proc.audition (dragging, false);
    dragging = -1;
    repaint();
}

void LadderView::mouseDoubleClick (const MouseEvent& e)
{
    const int d = degreeAt (e.position);
    if (d >= 0)
        proc.resetDegree (d);
}

// ===========================================================================
// Path
// ===========================================================================
namespace
{
constexpr double kPathSeconds = 14.0;
}

PathView::PathView (SeyirProcessor& p) : proc (p)
{
    setTooltip ("The melody over the last few seconds, against the scale's notes (tonics brighter). "
                "Rings mark the ends of phrases. Amber notes were replayed from the DEJA VU loop.");
}

void PathView::refresh()
{
    const size_t before = notes.size();
    proc.collectTrail (notes);
    const double t = proc.clockSeconds();
    while (! notes.empty() && notes.front().time < t - kPathSeconds - 4.0)
        notes.erase (notes.begin());
    if (notes.size() != before || std::abs (t - now) > 1e-3)
    {
        now = t;
        repaint();
    }
}

void PathView::paint (Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::black);
    g.fillRect (bounds);
    g.setColour (colours::inkDim);
    g.drawRect (bounds, 1.0f);

    const auto r = bounds.reduced (12.0f, 10.0f);
    const auto& s = proc.scale();
    const double p = s.period;
    const double lo = -p * 0.42 - 40.0;
    const double hi = p * proc.getParam (ids::range) + 40.0;
    auto yOf = [&] (double c) { return r.getBottom() - (float) ((c - lo) / (hi - lo)) * r.getHeight(); };
    auto xOf = [&] (double t) { return r.getRight() - (float) ((now - t) / kPathSeconds) * r.getWidth(); };

    // The scale's notes as faint lines; the tonics brighter.
    for (int k = -1; k <= 4; ++k)
        for (int i = 0; i < s.count; ++i)
        {
            const double c = s[i].cents + k * p;
            if (c < lo || c > hi || s[i].weight <= 0.0f) continue;
            const float y = yOf (c);
            g.setColour (i == 0 ? colours::inkDim.withAlpha (0.8f) : colours::socket);
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        }

    if (notes.empty())
    {
        drawText (g, proc.walking() ? "" : "Press play in Live, RUN, or play some keys.", r, 13.0f, colours::inkDim, false);
        return;
    }

    // The contour, as steps.
    g.reduceClipRegion (r.expanded (8.0f).toNearestInt());
    Path line;
    for (size_t i = 0; i < notes.size(); ++i)
    {
        const float x = xOf (notes[i].time), y = yOf (notes[i].cents);
        if (i == 0) line.startNewSubPath (x, y);
        else
        {
            line.lineTo (x, line.getCurrentPosition().y);
            line.lineTo (x, y);
        }
    }
    line.lineTo (r.getRight(), line.getCurrentPosition().y);
    g.setColour (colours::cream.withAlpha (0.55f));
    g.strokePath (line, PathStrokeType (1.6f));

    for (const auto& n : notes)
    {
        const auto c = Point<float> (xOf (n.time), yOf (n.cents));
        const auto col = n.replayed ? colours::lcd : colours::cream;
        g.setColour (col);
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (c));
        if (n.phraseEnd)
        {
            g.setColour (colours::brand);
            g.drawEllipse (Rectangle<float> (14.0f, 14.0f).withCentre (c), 1.8f);
        }
    }
}

// ===========================================================================
// Loop LEDs
// ===========================================================================
void LoopLeds::refresh()
{
    const int len = ids::loopSteps (proc.geti (ids::loop));
    const int pos = proc.walking() ? (proc.loopPosition() + len - 1) % len : -1;
    const float amount = proc.getParam (ids::dejavu);
    if (pos != shownPos || len != shownLen || std::abs (amount - shownAmount) > 0.01f)
    {
        shownPos = pos;
        shownLen = len;
        shownAmount = amount;
        repaint();
    }
}

void LoopLeds::paint (Graphics& g)
{
    const int len = shownLen;
    const int perRow = jmin (16, len);
    const int rows = (len + perRow - 1) / perRow;
    const auto r = getLocalBounds().toFloat();
    const float cw = r.getWidth() / (float) perRow;
    const float ch = r.getHeight() / 2.0f;
    for (int i = 0; i < len; ++i)
    {
        const int row = i / perRow, col = i % perRow;
        const auto c = Point<float> (r.getX() + cw * (col + 0.5f),
                                     r.getY() + ch * (row + 0.5f) + (rows == 1 ? ch * 0.5f : 0.0f));
        const bool on = i == shownPos;
        drawLed (g, c, 6.0f, shownAmount >= 0.995f ? colours::ledRed : colours::ledYellow, on, on ? 0.4f : 0.0f);
    }
}
} // namespace seyir::ui
