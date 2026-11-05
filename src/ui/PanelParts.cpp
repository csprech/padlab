#include "PanelParts.h"
#include "Materials.h"
#include "../core/Project.h"

namespace padlab
{
using namespace theme;

// ------------------------------------------------------------------ CURSOR

namespace
{
    juce::Path zonePath (juce::Rectangle<float> r, int zone)
    {
        // four trapezoids meeting at a small centre square, separated by grooves
        const auto c = r.getCentre();
        const float in = juce::jmin (r.getWidth(), r.getHeight()) * 0.16f;
        juce::Rectangle<float> centre (c.x - in, c.y - in, in * 2, in * 2);
        juce::Path p;
        switch (zone)
        {
            case 0: p.addQuadrilateral (r.getX(), r.getY(), r.getRight(), r.getY(), centre.getRight(), centre.getY(), centre.getX(), centre.getY()); break;
            case 1: p.addQuadrilateral (r.getRight(), r.getY(), r.getRight(), r.getBottom(), centre.getRight(), centre.getBottom(), centre.getRight(), centre.getY()); break;
            case 2: p.addQuadrilateral (r.getRight(), r.getBottom(), r.getX(), r.getBottom(), centre.getX(), centre.getBottom(), centre.getRight(), centre.getBottom()); break;
            default: p.addQuadrilateral (r.getX(), r.getBottom(), r.getX(), r.getY(), centre.getX(), centre.getY(), centre.getX(), centre.getBottom()); break;
        }
        return p;
    }
}

int CursorPad::zoneAt (juce::Point<float> p) const
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float dx = (p.x - c.x) / juce::jmax (1.0f, getWidth() * 0.5f), dy = (p.y - c.y) / juce::jmax (1.0f, getHeight() * 0.5f);
    if (std::abs (dx) > std::abs (dy))
        return dx > 0 ? 1 : 3;
    return dy > 0 ? 2 : 0;
}

void CursorPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    const float corner = 10.0f;
    // the opening in the panel and the block's cast shadow
    g.setColour (juce::Colours::white.withAlpha (0.32f));
    g.drawRoundedRectangle (r.expanded (2.0f).translated (0, 1.0f), corner + 2.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.expanded (2.0f), corner + 2.0f);

    const float sc = materials::scaleOf (g);
    const int w = (int) std::ceil (r.getWidth() * sc), h = (int) std::ceil (r.getHeight() * sc);
    const auto& base = materials::cached ("cursorpad:" + juce::String (w) + "x" + juce::String (h), [=] {
        materials::Shading sh { 0.42f, 0.64f, 0.12f, 12.0f, 0.03f, 0.08f, 1.4f };
        return materials::pillow (w, h, corner * sc, juce::jmin (w, h) * 0.08f, 1.5f * sc, juce::Colour (0xffe4dfcf), sh, 51);
    });
    materials::drawAt (g, base, r);

    // grooves between the four keys
    const auto c = r.getCentre();
    const float in = juce::jmin (r.getWidth(), r.getHeight()) * 0.16f;
    juce::Rectangle<float> centre (c.x - in, c.y - in, in * 2, in * 2);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawLine (r.getX() + 3, r.getY() + 3, centre.getX(), centre.getY(), 1.6f);
    g.drawLine (r.getRight() - 3, r.getY() + 3, centre.getRight(), centre.getY(), 1.6f);
    g.drawLine (r.getX() + 3, r.getBottom() - 3, centre.getX(), centre.getBottom(), 1.6f);
    g.drawLine (r.getRight() - 3, r.getBottom() - 3, centre.getRight(), centre.getBottom(), 1.6f);
    g.drawRect (centre, 1.4f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRect (centre.translated (0.8f, 0.8f), 0.8f);

    // printed arrows; the pressed key sinks a little (darker)
    for (int z = 0; z < 4; ++z)
    {
        if (z == pressed)
        {
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.fillPath (zonePath (r, z));
        }
        const float s = in * 0.55f;
        juce::Point<float> tip;
        juce::Path arrow;
        const float off = (z % 2 == 0 ? r.getHeight() : r.getWidth()) * 0.33f;
        switch (z)
        {
            case 0: tip = { c.x, c.y - off - s * 0.5f }; arrow.addTriangle (tip.x, tip.y, tip.x - s, tip.y + s, tip.x + s, tip.y + s); break;
            case 1: tip = { c.x + off + s * 0.5f, c.y }; arrow.addTriangle (tip.x, tip.y, tip.x - s, tip.y - s, tip.x - s, tip.y + s); break;
            case 2: tip = { c.x, c.y + off + s * 0.5f }; arrow.addTriangle (tip.x, tip.y, tip.x - s, tip.y - s, tip.x + s, tip.y - s); break;
            default: tip = { c.x - off - s * 0.5f, c.y }; arrow.addTriangle (tip.x, tip.y, tip.x + s, tip.y - s, tip.x + s, tip.y + s); break;
        }
        g.setColour (colours::silk.withAlpha (0.8f));
        g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));
    }
}

void CursorPad::fire()
{
    static constexpr int dx[] = { 0, 1, 0, -1 }, dy[] = { -1, 0, 1, 0 };
    if (pressed >= 0 && onMove)
        onMove (dx[pressed], dy[pressed]);
}

void CursorPad::mouseDown (const juce::MouseEvent& e)
{
    pressed = zoneAt (e.position);
    repeating = false;
    repaint();
    fire();
    startTimer (420);
}

void CursorPad::mouseUp (const juce::MouseEvent&)
{
    pressed = -1;
    stopTimer();
    repaint();
}

void CursorPad::timerCallback()
{
    if (! repeating)
    {
        repeating = true;
        startTimer (90); // held: auto-repeat like the hardware
    }
    fire();
}

// ------------------------------------------------------------------ soft key labels

void SoftKeyTabs::paint (juce::Graphics& g)
{
    using namespace theme::lcd;
    auto r = getLocalBounds();
    g.setFont (theme::screenFont (14.0f));
    const int n = labels.size();
    for (int i = 0; i < n; ++i)
    {
        const int cx = i < (int) centres.size() ? centres[(size_t) i] : (int) ((i + 0.5f) * r.getWidth() / juce::jmax (1, n));
        const int w = juce::jmax (40, r.getWidth() / juce::jmax (1, n) - 8);
        auto cell = juce::Rectangle<int> (cx - w / 2, r.getY() + 2, w, r.getHeight() - 3);
        if (i == active)
        {
            g.setColour (ink);
            g.fillRect (cell);
            g.setColour (backlight);
        }
        else
        {
            g.setColour (ink);
            g.drawRect (cell);
        }
        g.drawText (labels[i], cell, juce::Justification::centred, true);
    }
}

// ------------------------------------------------------------------ floppy drive

void FloppyDrive::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    // the bay cut into the panel and the drive's dark front bezel
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (r.translated (0, 1.0f), 4.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 4.0f);
    auto face = r.reduced (2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a38), 0, face.getY(), juce::Colour (0xff232322), 0, face.getBottom(), false));
    g.fillRoundedRectangle (face, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (face.reduced (0.5f), 3.0f, 1.0f);

    // disk slot with its spring flap
    auto slot = juce::Rectangle<float> (face.getX() + face.getWidth() * 0.08f, face.getY() + face.getHeight() * 0.30f,
                                        face.getWidth() * 0.84f, juce::jmax (5.0f, face.getHeight() * 0.16f));
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (slot, 2.0f);
    g.setColour (juce::Colour (0xff4a4a47));
    g.fillRect (slot.reduced (3.0f, 1.5f).withTrimmedBottom (slot.getHeight() * 0.35f));
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawLine (slot.getX() + 2, slot.getBottom() + 1.0f, slot.getRight() - 2, slot.getBottom() + 1.0f, 1.0f);

    // activity LED and the eject button
    drawLed (g, { face.getX() + 10.0f, face.getBottom() - 12.0f, 5.0f, 5.0f }, colours::ledGreen, busy ? 1.0f : 0.0f);
    auto eject = juce::Rectangle<float> (face.getRight() - 34.0f, face.getBottom() - 15.0f, 24.0f, 9.0f);
    drawFlatKey (g, eject, juce::Colour (0xff4e4e4b), false, 0.0f, 61);
    g.setFont (labelFont (8.0f));
    g.setColour (colours::silkLight.withAlpha (0.6f));
    g.drawText ("DISK", juce::Rectangle<float> (face.getX() + 20.0f, face.getBottom() - 15.0f, 40.0f, 10.0f), juce::Justification::centredLeft);
}

// ------------------------------------------------------------------ bank LEDs

juce::Rectangle<float> BankLeds::cell (int i) const
{
    const float w = getWidth() * 0.5f, h = getHeight() / 4.0f;
    return { (i / 4) * w, (i % 4) * h, w, h };
}

void BankLeds::paint (juce::Graphics& g)
{
    for (int i = 0; i < kNumBanks; ++i)
    {
        auto c = cell (i).reduced (2.0f);
        const float d = 5.5f;
        drawLed (g, { c.getX() + 3.0f, c.getCentreY() - d * 0.5f, d, d }, colours::ledRed, i == bank ? 1.0f : 0.0f);
        g.setFont (labelFont (11.0f));
        g.setColour (colours::silk.withAlpha (0.9f));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + i)), c.withTrimmedLeft (d + 8.0f), juce::Justification::centredLeft);
    }
}

void BankLeds::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kNumBanks; ++i)
        if (cell (i).contains (e.position) && onSelect)
            onSelect (i);
}

// ------------------------------------------------------------------ vents

void drawVentGrille (juce::Graphics& g, juce::Rectangle<float> area, int groups, int slots)
{
    const float gapX = area.getWidth() * 0.06f;
    const float gw = (area.getWidth() - gapX * (groups - 1)) / (float) groups;
    const float pitch = area.getHeight() / (float) slots;
    const float slotH = juce::jmax (3.0f, pitch * 0.42f);
    for (int gi = 0; gi < groups; ++gi)
        for (int s = 0; s < slots; ++s)
        {
            auto slot = juce::Rectangle<float> (area.getX() + gi * (gw + gapX), area.getY() + s * pitch + (pitch - slotH) * 0.5f, gw, slotH);
            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.fillRoundedRectangle (slot.translated (0, 1.2f), slotH * 0.5f);
            g.setColour (juce::Colour (0xff121212));
            g.fillRoundedRectangle (slot, slotH * 0.5f);
            // a glimpse of the dark interior lit from above
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRect (slot.reduced (slotH * 0.5f, 0).withTrimmedTop (slotH * 0.65f));
        }
}

} // namespace padlab
