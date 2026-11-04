#include "ui/Theme.h"
#include "BinaryData.h"
#include "ui/Materials.h"
#include <map>

namespace padlab::theme
{
// ---------------------------------------------------------------- fonts

juce::Font screenFont (float height, bool)
{
    static juce::Typeface::Ptr vt = juce::Typeface::createSystemTypefaceFor (BinaryData::VT323Regular_ttf,
                                                                            (size_t) BinaryData::VT323Regular_ttfSize);
    return juce::Font (juce::FontOptions (vt).withHeight (height * 1.32f));
}

juce::Font labelFont (float height, bool)
{
    static const bool hasDin = juce::Font::findAllTypefaceNames().contains ("DIN Condensed");
    if (hasDin)
        return juce::Font (juce::FontOptions ("DIN Condensed", height * 1.18f, juce::Font::bold));
    return juce::Font (juce::FontOptions ("Avenir Next Condensed", height, juce::Font::bold));
}

// ---------------------------------------------------------------- textures

const juce::Image& speckle (int which)
{
    static juce::Image images[2];
    auto& img = images[juce::jlimit (0, 1, which)];
    if (img.isNull())
    {
        constexpr int size = 192;
        img = juce::Image (juce::Image::ARGB, size, size, true);
        juce::Random rng (which == 0 ? 1979 : 1988);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                const float n = rng.nextFloat();
                if (which == 0)
                {
                    // chassis paint: fine grain plus the odd darker fleck
                    const bool fleck = n > 0.985f;
                    const auto c = fleck ? juce::Colours::black.withAlpha (0.16f)
                                         : (n > 0.5f ? juce::Colours::white.withAlpha ((n - 0.5f) * 0.09f)
                                                     : juce::Colours::black.withAlpha ((0.5f - n) * 0.08f));
                    bd.setPixelColour (x, y, c);
                }
                else
                {
                    // rubber: softer, matte
                    bd.setPixelColour (x, y, n > 0.5f ? juce::Colours::white.withAlpha ((n - 0.5f) * 0.07f)
                                                      : juce::Colours::black.withAlpha ((0.5f - n) * 0.12f));
                }
            }
    }
    return img;
}

juce::Colour padGlow (float v) { return colours::ledAmber.withMultipliedBrightness (0.6f + 0.4f * juce::jlimit (0.0f, 1.0f, v)); }

void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    const float sc = materials::scaleOf (g);
    const int d = (int) std::ceil (r * 2 * sc);
    const auto key = "screw:" + juce::String (d) + ":" + juce::String ((int) (angle * 100));
    auto dest = juce::Rectangle<float> (r * 2, r * 2).withCentre (c);
    // countersunk hole
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (dest.expanded (1.5f).translated (0, -0.5f));
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawEllipse (dest.expanded (1.5f).translated (0, 0.8f), 0.8f);
    materials::drawAt (g, materials::cached (key, [=] { return materials::screw (d, angle); }), dest);
}

void drawLed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float b)
{
    b = juce::jlimit (0.0f, 1.0f, b);
    const float sc = materials::scaleOf (g);
    auto dest = r.expanded (r.getWidth() * 0.35f); // lens + bezel ring
    const int d = (int) std::ceil (dest.getWidth() * sc);
    const auto base = "led:" + juce::String (d) + ":" + c.toString();
    const auto& off = materials::cached (base + ":0", [=] { return materials::led (d, c, false); });
    materials::drawAt (g, off, dest);
    if (b > 0.01f)
    {
        const auto& lit = materials::cached (base + ":1", [=] { return materials::led (d, c, true); });
        materials::drawAt (g, lit, dest, b);
        // bloom
        const auto centre = r.getCentre();
        juce::ColourGradient halo (c.withAlpha (0.55f * b), centre.x, centre.y, c.withAlpha (0.0f),
                                   centre.x + r.getWidth() * 2.4f, centre.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (r.expanded (r.getWidth() * 2.4f));
    }
}

void drawSilk (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, juce::Justification j, float h, bool light)
{
    g.setFont (labelFont (h));
    g.setColour (light ? colours::silkLight.withAlpha (0.92f) : colours::silk.withAlpha (0.9f));
    g.drawFittedText (text, r, j, 2, 0.9f);
}

// ---------------------------------------------------------------- hardware look and feel

HardwareLookAndFeel::HardwareLookAndFeel()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, chassis);
    setColour (juce::TextButton::buttonColourId, keyLight);
    setColour (juce::TextButton::textColourOffId, silk);
    setColour (juce::TextButton::textColourOnId, silk);
    setColour (juce::Label::textColourId, silk);
    setColour (juce::Slider::textBoxTextColourId, silk);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, ledAmber);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xfff4f1e6));
    setColour (juce::TooltipWindow::textColourId, silk);
    setColour (juce::TooltipWindow::outlineColourId, seam);
    // dialogs (audio settings, alerts) stay neutral and readable
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xffd9d6ce));
    setColour (juce::AlertWindow::textColourId, silk);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xffefede7));
    setColour (juce::ComboBox::textColourId, silk);
    setColour (juce::ComboBox::outlineColourId, seam);
    setColour (juce::ComboBox::arrowColourId, silk);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xffefede7));
    setColour (juce::PopupMenu::textColourId, silk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, keyDark);
    setColour (juce::PopupMenu::highlightedTextColourId, silkLight);
    setColour (juce::ListBox::backgroundColourId, juce::Colour (0xffefede7));
    setColour (juce::ListBox::textColourId, silk);
    setColour (juce::ToggleButton::textColourId, silk);
    setColour (juce::ToggleButton::tickColourId, silk);
    setColour (juce::ToggleButton::tickDisabledColourId, seam);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xffefede7));
    setColour (juce::TextEditor::textColourId, silk);
    setColour (juce::TextEditor::outlineColourId, seam);
    setColour (juce::Slider::thumbColourId, keyDark);
    setColour (juce::Slider::trackColourId, keyDark);
    setColour (juce::Slider::backgroundColourId, seam);
    setColour (juce::ScrollBar::thumbColourId, seam);
}

void HardwareLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (3.0f, 3.0f);
    drawFlatKey (g, r, colours::keyLight, down, 0.0f, 3);
    if (over && ! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (r, 2.5f);
    }
}

void HardwareLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (colours::silk.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (4, 2).translated (0, down ? 1 : 0),
                      juce::Justification::centred, 2);
}

juce::Font HardwareLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return labelFont (juce::jlimit (10.0f, 13.0f, h * 0.42f)); }
juce::Font HardwareLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

void HardwareLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start,
                                            float end, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);

    // printed scale ticks
    g.setColour (colours::silk.withAlpha (0.75f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.0f;
        const float r0 = radius * (i % 5 == 0 ? 0.80f : 0.86f), r1 = radius * 0.98f;
        g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0, c.x + std::sin (a) * r1, c.y - std::cos (a) * r1,
                    i % 5 == 0 ? 1.6f : 1.0f);
    }

    // knurled black knob (rendered per angle so the key light stays put while it turns)
    const float kr = radius * 0.66f;
    auto kb = juce::Rectangle<float> (kr * 2, kr * 2).withCentre (c);
    materials::roundShadow (g, c, kr * 0.95f, kr * 0.5f);
    const float sc = materials::scaleOf (g);
    const int d = (int) std::ceil (kb.getWidth() * sc);
    // one render per knob, refreshed only when its angle or size changes
    struct LastRender { float angle = -100.0f; int size = 0; juce::Image image; };
    static std::map<const juce::Slider*, LastRender> renders;
    auto& last = renders[&s];
    const auto capProp = s.getProperties()["capColour"];
    const juce::Colour cap = capProp.isVoid() ? juce::Colour (0xff1a1a1a) : juce::Colour ((juce::uint32) (juce::int64) capProp);
    if (std::abs (last.angle - angle) > 0.002f || last.size != d)
        last = { angle, d, materials::knob (d, angle, cap) };
    materials::drawAt (g, last.image, kb);
}

void HardwareLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float min, float max,
                                            juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
        return juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, min, max, style, s);

    // a panel fader: a slot cut into the panel, printed scale, and a black ribbed cap
    auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float capH = juce::jmin (30.0f, r.getHeight() * 0.16f);
    auto travel = r.reduced (0.0f, capH * 0.5f);
    const float cx = r.getCentreX();

    g.setColour (colours::silk.withAlpha (0.7f));
    for (int i = 0; i <= 10; ++i)
    {
        const float ty = travel.getY() + travel.getHeight() * (float) i / 10.0f;
        const float len = i % 5 == 0 ? 10.0f : 6.0f;
        g.drawLine (cx - 12.0f - len, ty, cx - 12.0f, ty, i % 5 == 0 ? 1.4f : 1.0f);
        g.drawLine (cx + 12.0f, ty, cx + 12.0f + len, ty, i % 5 == 0 ? 1.4f : 1.0f);
    }

    auto slot = juce::Rectangle<float> (cx - 3.0f, travel.getY() - 4.0f, 6.0f, travel.getHeight() + 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.fillRoundedRectangle (slot.translated (0.0f, 1.2f).expanded (0.8f), 3.0f);
    g.setColour (juce::Colour (0xff141414));
    g.fillRoundedRectangle (slot, 3.0f);

    const float capW = juce::jmin (r.getWidth() - 6.0f, 40.0f);
    auto cap = juce::Rectangle<float> (capW, capH).withCentre ({ cx, pos });
    juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 6, { 1, 4 }).drawForRectangle (g, cap.toNearestInt().reduced (2));
    const float sc = materials::scaleOf (g);
    const int cw = (int) std::ceil (cap.getWidth() * sc), ch = (int) std::ceil (cap.getHeight() * sc);
    const auto& img = materials::cached ("fadercap:" + juce::String (cw) + "x" + juce::String (ch), [=] {
        auto sh = materials::plastic();
        sh.specular = 0.35f;
        return materials::pillow (cw, ch, 2.5f * sc, juce::jmin (cw, ch) * 0.22f, 1.0f * sc, juce::Colour (0xff1e1e1e), sh, 41);
    });
    materials::drawAt (g, img, cap);
    // ribs and the white centre line
    for (int i = -2; i <= 2; ++i)
    {
        if (i == 0)
            continue;
        const float ly = cap.getCentreY() + i * cap.getHeight() * 0.16f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawLine (cap.getX() + 4.0f, ly, cap.getRight() - 4.0f, ly, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawLine (cap.getX() + 4.0f, ly + 1.0f, cap.getRight() - 4.0f, ly + 1.0f, 1.0f);
    }
    g.setColour (juce::Colour (0xffeeeae0));
    g.fillRect (juce::Rectangle<float> (cap.getX() + 3.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 6.0f, 2.0f));
}

// ---------------------------------------------------------------- LCD look and feel

LcdLookAndFeel::LcdLookAndFeel()
{
    using namespace lcd;
    setColour (juce::Label::textColourId, ink);
    setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::outlineColourId, ink);
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::arrowColourId, ink);
    setColour (juce::PopupMenu::backgroundColourId, backlight);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ink);
    setColour (juce::PopupMenu::highlightedTextColourId, backlight);
    setColour (juce::Slider::textBoxTextColourId, ink);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, ink.withAlpha (0.3f));
    setColour (juce::Slider::trackColourId, ink);
    setColour (juce::Slider::backgroundColourId, ink.withAlpha (0.12f));
    setColour (juce::Slider::thumbColourId, ink);
    setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonOnColourId, ink);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, backlight);
    setColour (juce::ToggleButton::textColourId, ink);
    setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::textColourId, ink);
    setColour (juce::TextEditor::highlightColourId, ink.withAlpha (0.3f));
    setColour (juce::TextEditor::highlightedTextColourId, ink);
    setColour (juce::TextEditor::outlineColourId, ink);
    setColour (juce::TextEditor::focusedOutlineColourId, ink);
    setColour (juce::CaretComponent::caretColourId, ink);
    setColour (juce::ScrollBar::thumbColourId, ink.withAlpha (0.5f));
}

juce::Font LcdLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return screenFont (juce::jlimit (11.0f, 14.0f, h * 0.52f)); }
juce::Font LcdLookAndFeel::getComboBoxFont (juce::ComboBox& c) { return screenFont (juce::jmin (14.0f, c.getHeight() * 0.62f)); }
juce::Font LcdLookAndFeel::getPopupMenuFont() { return screenFont (15.0f); }
juce::Font LcdLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }
juce::Font LcdLookAndFeel::getSliderPopupFont (juce::Slider&) { return screenFont (14.0f); }

juce::Label* LcdLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (screenFont (juce::jlimit (11.0f, 14.0f, s.getHeight() * 0.6f)));
    l->setColour (juce::Label::textColourId, lcd::ink);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

void LcdLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool inverse = b.getToggleState() || down;
    g.setColour (lcd::ink.withAlpha (b.isEnabled() ? 1.0f : 0.35f));
    if (inverse)
        g.fillRect (r);
    else
    {
        g.drawRect (r, 1.5f);
        if (over)
            g.fillRect (r.reduced (3.0f).removeFromBottom (1.5f));
    }
}

void LcdLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    const bool inverse = b.getToggleState() || down;
    g.setColour ((inverse ? lcd::backlight : lcd::ink).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.45f));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 2);
}

void LcdLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (lcd::ink.withAlpha (box.isEnabled() ? 1.0f : 0.35f));
    g.drawRect (r, 1.2f);
    if (down)
        g.fillRect (r.removeFromRight (16.0f));
    juce::Path arrow;
    const float ax = (float) w - 9.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (down ? lcd::backlight : lcd::ink);
    g.fillPath (arrow);
}

void LcdLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float min, float max,
                                       juce::Slider::SliderStyle style, juce::Slider& s)
{
    using namespace lcd;
    auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
    if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
    {
        g.setColour (ink.withAlpha (s.isEnabled() ? 1.0f : 0.35f));
        g.drawRect (r.reduced (0.5f), 1.2f);
        // dot-matrix fill: evenly spaced ink columns
        auto fill = (style == juce::Slider::LinearBar ? r.withRight (pos) : r.withTop (pos)).reduced (3.0f);
        g.setColour (ink.withAlpha (s.isEnabled() ? 0.22f : 0.08f));
        for (float xx = fill.getX(); xx < fill.getRight(); xx += 3.0f)
            g.fillRect (xx, fill.getY(), 2.0f, fill.getHeight());
        return;
    }
    if (style == juce::Slider::LinearVertical)
    {
        auto slot = r.withSizeKeepingCentre (4.0f, r.getHeight());
        g.setColour (ink.withAlpha (0.35f));
        g.fillRect (slot);
        for (int i = 0; i <= 10; ++i)
        {
            const float yy = r.getY() + r.getHeight() * (float) i / 10.0f;
            g.fillRect (r.getX() + 2.0f, yy, 6.0f, 1.0f);
        }
        auto cap = juce::Rectangle<float> (r.getWidth() * 0.7f, 10.0f).withCentre ({ r.getCentreX(), pos });
        g.setColour (ink);
        g.fillRect (cap);
        g.setColour (backlight);
        g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 6.0f, 1.5f));
        return;
    }
    LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, min, max, style, s);
}

void LcdLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                       juce::Slider&)
{
    auto b = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) / 2.0f;
    const auto c = b.getCentre();
    const float angle = start + pos * (end - start);
    g.setColour (lcd::ink);
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.5f);
    g.drawLine (c.x, c.y, c.x + std::sin (angle) * r * 0.85f, c.y - std::cos (angle) * r * 0.85f, 2.0f);
}

void LcdLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.24f);
    g.setColour (lcd::ink.withAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawRect (box, over ? 2.0f : 1.4f);
    if (b.getToggleState())
        g.fillRect (box.reduced (3.0f));
    g.setFont (screenFont (13.0f));
    g.drawFittedText (b.getButtonText().toUpperCase(), r.toNearestInt().withTrimmedLeft (4), juce::Justification::centredLeft, 1);
}

void LcdLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (lcd::backlight);
    g.setColour (lcd::ink);
    g.drawRect (0, 0, w, h, 2);
}

void LcdLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                        bool isHighlighted, bool isTicked, bool, const juce::String& text,
                                        const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour (lcd::ink.withAlpha (0.4f));
        g.fillRect (area.withSizeKeepingCentre (area.getWidth() - 8, 1));
        return;
    }
    auto r = area.reduced (2, 0);
    if (isHighlighted && isActive)
    {
        g.setColour (lcd::ink);
        g.fillRect (r);
    }
    g.setColour ((isHighlighted && isActive ? lcd::backlight : lcd::ink).withMultipliedAlpha (isActive ? 1.0f : 0.4f));
    g.setFont (getPopupMenuFont());
    g.drawFittedText ((isTicked ? "> " : "  ") + text, r.reduced (6, 0), juce::Justification::centredLeft, 1);
}

void LcdLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (lcd::ink.withAlpha (0.06f));
    g.fillRect (0, 0, w, h);
}

void LcdLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (lcd::ink);
    g.drawRect (0, 0, w, h, e.hasKeyboardFocus (true) ? 2 : 1);
}

// ---------------------------------------------------------------- rubber key caps

HardButton::HardButton (const juce::String& text, juce::Colour led, Cap c)
    : juce::TextButton (text), ledColour (led), cap_ (c), seed ((int) (juce::DefaultHashFunctions::generateHash (text, 1 << 16))) {}

void HardButton::setLit (bool l)
{
    if (l != lit)
    {
        lit = l;
        repaint();
    }
}

namespace
{
    /** Low, flat-faced moulded key: a small round-over at the edge and an almost flat top. */
    materials::Shading keyShading() { return { 0.42f, 0.64f, 0.10f, 12.0f, 0.03f, 0.08f, 1.4f }; }

    void drawCutout (juce::Graphics& g, juce::Rectangle<float> cap)
    {
        // the slot in the panel: a thin dark gap with a lit lower lip
        auto slot = cap.expanded (1.6f);
        g.setColour (juce::Colours::white.withAlpha (0.32f));
        g.drawRoundedRectangle (slot.translated (0, 1.0f), 3.5f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.62f));
        g.fillRoundedRectangle (slot, 3.5f);
    }
} // namespace

void drawFlatKey (juce::Graphics& g, juce::Rectangle<float> cap, juce::Colour albedo, bool down, float wear, uint32_t seed)
{
    drawCutout (g, cap);
    if (! down)
        juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 3, { 1, 2 }).drawForRectangle (g, cap.toNearestInt().reduced (1));
    cap = cap.translated (0, down ? 1.0f : 0.0f);
    const float sc = materials::scaleOf (g);
    const int w = (int) std::ceil (cap.getWidth() * sc), h = (int) std::ceil (cap.getHeight() * sc);
    const auto key = "flatkey:" + juce::String (w) + "x" + juce::String (h) + ":" + albedo.toString() + ":"
                     + juce::String ((int) (wear * 100)) + ":" + juce::String ((int) seed);
    const auto& img = materials::cached (key, [=] {
        return materials::pillow (w, h, 2.5f * sc, juce::jmin (w, h) * 0.12f, 0.4f * sc, albedo, keyShading(), seed, wear);
    });
    materials::drawAt (g, img, cap);
    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.08f));
        g.fillRoundedRectangle (cap, 2.5f);
    }
}

void HardButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat();
    const auto lines = juce::StringArray::fromLines (getButtonText());
    const float lineH = 11.0f;
    const bool above = ! legendOnCap && getButtonText().isNotEmpty();
    const float legendH = above ? lines.size() * lineH : 0.0f;
    // every key in a row has the same cap, whatever its legend: size and place it for the row's legend height
    const float slotH = legendOnCap ? 0.0f : (float) legendLines * lineH, gap = slotH > 0.0f ? 4.0f : 0.0f;
    const float subH = subLabel.isEmpty() ? 0.0f : 13.0f;
    const float capH = juce::jlimit (14.0f, 30.0f, b.getHeight() - slotH - gap - subH - 4.0f);
    const float capW = juce::jmin (b.getWidth() - 8.0f, capH * capAspect);
    const float capY = b.getY() + std::round ((b.getHeight() - (slotH + gap + capH + subH)) * 0.5f) + slotH + gap;
    const float top = capY - gap - legendH; // legend sits just above the cap

    // legend silkscreened on the panel above the key, with the LED dot beside its first line
    g.setFont (labelFont (10.5f));
    for (int i = 0; above && i < lines.size(); ++i)
    {
        const juce::Rectangle<float> row (b.getX(), top + i * lineH, b.getWidth(), lineH);
        g.setColour (colours::silk.withMultipliedAlpha (isEnabled() ? 0.92f : 0.4f));
        g.drawText (lines[i], row, juce::Justification::centred, false);
        if (i == 0 && hasLed)
        {
            const bool on = lit && ! (blink && (juce::Time::getMillisecondCounter() / 250) % 2 == 0);
            const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), lines[i]);
            const float d = 5.0f;
            drawLed (g, juce::Rectangle<float> (d, d).withCentre ({ row.getCentreX() - tw * 0.5f - 7.0f, row.getCentreY() - 0.5f }),
                     ledColour, on ? 1.0f : 0.0f);
        }
    }

    const juce::Rectangle<float> cap (std::round (b.getCentreX() - capW * 0.5f), capY, capW, capH);
    const juce::Colour albedo = cap_ == Cap::Dark   ? colours::keyDark
                              : cap_ == Cap::Red    ? colours::keyRed
                              : cap_ == Cap::White  ? colours::keyWhite
                              : cap_ == Cap::Blue   ? colours::keyBlue
                                                    : colours::keyLight;
    drawFlatKey (g, cap, albedo, down, wear, 17u + (uint32_t) seed);
    if (over && ! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (cap, 2.5f);
    }
    if (legendOnCap)
    {
        const bool darkCap = cap_ == Cap::Dark || cap_ == Cap::Red;
        g.setFont (labelFont (juce::jlimit (10.0f, 14.0f, capH * 0.55f)));
        g.setColour ((darkCap ? colours::silkLight : colours::silk).withMultipliedAlpha (isEnabled() ? 0.9f : 0.4f));
        g.drawFittedText (getButtonText(), cap.toNearestInt().translated (0, down ? 1 : 0), juce::Justification::centred, 2, 0.85f);
        if (hasLed)
        {
            const bool on = lit && ! (blink && (juce::Time::getMillisecondCounter() / 250) % 2 == 0);
            drawLed (g, { cap.getX() + 5.0f, cap.getY() + 4.0f, 4.5f, 4.5f }, ledColour, on ? 1.0f : 0.0f);
        }
    }
    if (subLabel.isNotEmpty())
    {
        // the shifted function, printed in a small dark box under the key
        g.setFont (labelFont (8.5f));
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), subLabel) + 8.0f;
        auto box = juce::Rectangle<float> (juce::jmin (b.getWidth() - 2.0f, juce::jmax (capW, tw)), 10.0f)
                       .withCentre ({ b.getCentreX(), cap.getBottom() + 3.0f + 5.0f });
        g.setColour (colours::keyDark.withAlpha (0.85f));
        g.fillRect (box);
        g.setColour (colours::silkLight.withAlpha (0.92f));
        g.drawFittedText (subLabel, box.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }
}

// ---------------------------------------------------------------- LCD ink shadow

void LcdInkShadow::applyEffect (juce::Image& src, juce::Graphics& g, float scale, float alpha)
{
    g.setColour (lcd::ink.withAlpha (0.10f * alpha));
    g.drawImageAt (src, (int) std::round (1.0f * scale), (int) std::round (1.0f * scale), true);
    g.setColour (lcd::ink.withAlpha (0.08f * alpha));
    g.drawImageAt (src, (int) std::round (2.0f * scale), (int) std::round (2.0f * scale), true);
    g.setOpacity (alpha);
    g.drawImageAt (src, 0, 0);
}

void LcdInkCache::paint (juce::Graphics& g)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const auto bounds = owner.getLocalBounds();
    const auto imageBounds = bounds * scale;
    if (content.isNull() || content.getBounds() != imageBounds)
    {
        content = juce::Image (juce::Image::ARGB, juce::jmax (1, imageBounds.getWidth()), juce::jmax (1, imageBounds.getHeight()), true);
        composed = juce::Image (juce::Image::ARGB, content.getWidth(), content.getHeight(), true);
        validArea.clear();
    }

    if (! validArea.containsRectangle (bounds))
    {
        {
            juce::Graphics ig (content);
            ig.addTransform (juce::AffineTransform::scale (scale));
            for (auto& r : validArea)
                ig.excludeClipRegion (r);
            ig.getInternalContext().setFill (juce::Colours::transparentBlack);
            ig.getInternalContext().fillRect (bounds, true);
            owner.paintEntireComponent (ig, true);
        }
        validArea = bounds;
        composedValid = false;
    }

    if (! composedValid)
    {
        composed.clear (composed.getBounds());
        juce::Graphics cg (composed);
        cg.setColour (lcd::ink.withAlpha (0.10f));
        cg.drawImageAt (content, (int) std::round (scale), (int) std::round (scale), true);
        cg.setColour (lcd::ink.withAlpha (0.08f));
        cg.drawImageAt (content, (int) std::round (2.0f * scale), (int) std::round (2.0f * scale), true);
        cg.setOpacity (1.0f);
        cg.drawImageAt (content, 0, 0);
        composedValid = true;
    }

    g.setOpacity (owner.getAlpha());
    g.drawImageTransformed (composed, juce::AffineTransform::scale (1.0f / scale));
}

// ---------------------------------------------------------------- LCD glass

void LcdGlass::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // pixel grid: faint gaps between LCD dots
    g.setColour (lcd::backlightLo.withAlpha (0.16f));
    for (float y = 0; y < r.getHeight(); y += 3.0f)
        g.fillRect (0.0f, y, r.getWidth(), 1.0f);
    for (float x = 0; x < r.getWidth(); x += 3.0f)
        g.fillRect (x, 0.0f, 1.0f, r.getHeight());

    // uneven backlight: darker edges and corners
    juce::ColourGradient vignette (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                   juce::Colours::black.withAlpha (0.16f), r.getX(), r.getY(), true);
    g.setGradientFill (vignette);
    g.fillRect (r);

    // inner shadow where the bezel overhangs the glass
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.28f), 0, 0, juce::Colours::transparentBlack,
                                             0, 14.0f, false));
    g.fillRect (r.withHeight (14.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.18f), 0, 0, juce::Colours::transparentBlack,
                                             10.0f, 0, false));
    g.fillRect (r.withWidth (10.0f));

    // glass reflection
    juce::Path sheen;
    sheen.startNewSubPath (r.getX(), r.getY());
    sheen.lineTo (r.getX() + r.getWidth() * 0.55f, r.getY());
    sheen.lineTo (r.getX() + r.getWidth() * 0.30f, r.getBottom());
    sheen.lineTo (r.getX(), r.getBottom());
    sheen.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.09f), r.getX(), r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.4f,
                                             r.getBottom(), false));
    g.fillPath (sheen);

    // a couple of greasy fingerprint smears, only visible as a change in the glass's sheen
    const float sc = materials::scaleOf (g);
    struct Smear { float x, y, w, h, alpha; uint32_t seed; };
    for (auto s : { Smear { 0.70f, 0.58f, 0.16f, 0.22f, 0.10f, 3 }, Smear { 0.06f, 0.30f, 0.10f, 0.20f, 0.07f, 9 },
                    Smear { 0.44f, 0.84f, 0.12f, 0.12f, 0.06f, 21 } })
    {
        auto dest = juce::Rectangle<float> (r.getWidth() * s.w, r.getHeight() * s.h)
                        .withCentre ({ r.getWidth() * s.x, r.getHeight() * s.y });
        const int w = (int) std::ceil (dest.getWidth() * sc), h = (int) std::ceil (dest.getHeight() * sc);
        materials::drawAt (g, materials::cached ("smudge:" + juce::String (w) + "x" + juce::String (h) + ":" + juce::String ((int) s.seed),
                                                 [=] { return materials::smudge (w, h, s.seed); }),
                           dest, s.alpha);
    }
}

} // namespace padlab::theme
