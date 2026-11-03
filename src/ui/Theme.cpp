#include "ui/Theme.h"
#include "BinaryData.h"

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
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (c.x - r - 1.0f, c.y - r, r * 2 + 2.0f, r * 2 + 2.5f);
    juce::ColourGradient grad (juce::Colour (0xffe2e0da), c.x - r * 0.5f, c.y - r * 0.6f,
                               juce::Colour (0xff6e6c67), c.x + r, c.y + r, true);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    juce::Path slot;
    slot.addRectangle (-r * 0.75f, -r * 0.13f, r * 1.5f, r * 0.26f);
    g.setColour (juce::Colour (0xff3a3936));
    g.fillPath (slot, juce::AffineTransform::rotation (angle).translated (c));
}

void drawLed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float b)
{
    b = juce::jlimit (0.0f, 1.0f, b);
    const auto centre = r.getCentre();
    if (b > 0.02f)
    {
        juce::ColourGradient halo (c.withAlpha (0.45f * b), centre.x, centre.y, c.withAlpha (0.0f),
                                   centre.x + r.getWidth() * 1.6f, centre.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (r.expanded (r.getWidth() * 1.4f));
    }
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (r.expanded (1.0f));
    const auto lensOff = colours::ledOff.interpolatedWith (c, 0.18f);
    const auto lens = lensOff.interpolatedWith (c, b);
    juce::ColourGradient g2 (lens.brighter (0.6f * b + 0.15f), centre.x - r.getWidth() * 0.2f, r.getY(),
                             lens.darker (0.3f), centre.x, r.getBottom(), false);
    g.setGradientFill (g2);
    g.fillEllipse (r);
    g.setColour (juce::Colours::white.withAlpha (0.35f + 0.3f * b));
    g.fillEllipse (r.getX() + r.getWidth() * 0.25f, r.getY() + r.getHeight() * 0.15f, r.getWidth() * 0.3f, r.getHeight() * 0.25f);
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
    // small rubber key
    auto r = b.getLocalBounds().toFloat().reduced (2.0f, 2.0f);
    const float press = down ? 1.5f : 0.0f;
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillRoundedRectangle (r.translated (0, 2.5f - press * 0.6f).expanded (0.5f), 4.0f);
    r = r.translated (0, press);
    auto c = colours::keyLight;
    if (over && ! down)
        c = c.brighter (0.04f);
    g.setGradientFill (juce::ColourGradient (c.brighter (0.12f), r.getX(), r.getY(), c.darker (0.12f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setTiledImageFill (speckle (1), 0, 0, 0.8f);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 4.0f, r.getRight() - 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawRoundedRectangle (r, 4.0f, 0.8f);
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
    // value arc (bipolar controls start from the centre)
    {
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bipolar ? start + (float) s.valueToProportionOfLength (0.0) * (end - start) : start;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, radius * 0.74f, radius * 0.74f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (colours::ledAmber.withAlpha (0.85f));
        g.strokePath (arc, juce::PathStrokeType (2.0f));
    }

    // knurled black knob
    const float kr = radius * 0.62f;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - kr - 1.0f, c.y - kr + 2.0f, kr * 2 + 2.0f, kr * 2 + 2.0f);
    juce::Path knurl;
    const int teeth = 28;
    for (int i = 0; i < teeth * 2; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) i / (teeth * 2);
        const float rr = (i % 2 == 0) ? kr : kr * 0.94f;
        const auto p = juce::Point<float> (c.x + std::sin (a) * rr, c.y - std::cos (a) * rr);
        if (i == 0) knurl.startNewSubPath (p); else knurl.lineTo (p);
    }
    knurl.closeSubPath();
    g.setColour (juce::Colour (0xff1b1b1a));
    g.fillPath (knurl);
    const float cap = kr * 0.8f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a47), c.x - cap * 0.4f, c.y - cap,
                                             juce::Colour (0xff161615), c.x + cap * 0.3f, c.y + cap, false));
    g.fillEllipse (c.x - cap, c.y - cap, cap * 2, cap * 2);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (c.x - cap + 1, c.y - cap + 1, cap * 2 - 2, cap * 2 - 2, 1.0f);
    // indicator line
    juce::Path p;
    p.addRoundedRectangle (-1.4f, -cap + 2.0f, 2.8f, cap * 0.62f, 1.2f);
    g.setColour (colours::silkLight);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
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
    : juce::TextButton (text), ledColour (led), cap (c) {}

void HardButton::setLit (bool l)
{
    if (l != lit)
    {
        lit = l;
        repaint();
    }
}

void HardButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f, 3.0f);
    const float press = down ? 2.0f : 0.0f;

    // recess the key sits in
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRoundedRectangle (r.expanded (2.5f).translated (0, 0.5f), 7.0f);
    // drop shadow (shrinks when pressed)
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0, 3.0f - press), 6.0f);

    r = r.translated (0, press);
    juce::Colour base = cap == Cap::Dark ? colours::keyDark : cap == Cap::Red ? colours::keyRed : colours::keyLight;
    if (over && ! down)
        base = base.brighter (0.04f);
    if (down)
        base = base.darker (0.08f);

    g.setGradientFill (juce::ColourGradient (base.brighter (0.16f), r.getX(), r.getY(), base.darker (0.18f), r.getX(),
                                             r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);
    g.setTiledImageFill (speckle (1), 0, 0, 1.0f);
    g.fillRoundedRectangle (r, 6.0f);
    // soft top highlight + rim
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (cap == Cap::Light ? 0.38f : 0.16f), r.getX(),
                                             r.getY(), juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.45f, false));
    g.fillRoundedRectangle (r.reduced (1.5f), 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    // LED
    auto textArea = r.toNearestInt().reduced (4, 2);
    if (hasLed)
    {
        const bool on = lit && ! (blink && (juce::Time::getMillisecondCounter() / 250) % 2 == 0);
        const float d = juce::jlimit (5.0f, 8.0f, r.getHeight() * 0.16f);
        drawLed (g, { r.getX() + 7.0f, r.getY() + 6.0f, d, d }, ledColour, on ? 1.0f : 0.0f);
    }

    const bool lightText = cap != Cap::Light;
    g.setColour ((lightText ? colours::silkLight : colours::silk).withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f));
    g.setFont (labelFont (juce::jlimit (10.0f, 14.0f, getHeight() * 0.27f)));
    g.drawFittedText (getButtonText(), textArea.withTrimmedTop (hasLed ? 6 : 0), juce::Justification::centred, 2, 0.85f);
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
}

} // namespace padlab::theme
