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
    // small rubber key
    auto r = b.getLocalBounds().toFloat().reduced (3.0f, 3.0f);
    const float sc = materials::scaleOf (g);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (down ? 0.35f : 0.55f), down ? 3 : 6, { 0, down ? 1 : 3 })
        .drawForRectangle (g, r.toNearestInt().reduced (2));
    r = r.translated (0, down ? 1.5f : 0.0f);
    const int w = (int) std::ceil (r.getWidth() * sc), h = (int) std::ceil (r.getHeight() * sc);
    const auto key = "smallkey:" + juce::String (w) + "x" + juce::String (h);
    const auto& img = materials::cached (key, [=] {
        return materials::pillow (w, h, 5.0f * sc, juce::jmin (w, h) * 0.26f, 1.2f * sc, colours::keyLight, materials::plastic(), 3);
    });
    materials::drawAt (g, img, r);
    if (over && ! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (r, 5.0f);
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
    materials::contactShadow (g, kb.reduced (kr * 0.12f), kr * 0.35f, kr * 0.22f, 0.55f);
    const float sc = materials::scaleOf (g);
    const int d = (int) std::ceil (kb.getWidth() * sc);
    // one render per knob, refreshed only when its angle or size changes
    struct LastRender { float angle = -100.0f; int size = 0; juce::Image image; };
    static std::map<const juce::Slider*, LastRender> renders;
    auto& last = renders[&s];
    if (std::abs (last.angle - angle) > 0.002f || last.size != d)
        last = { angle, d, materials::knob (d, angle) };
    materials::drawAt (g, last.image, kb);
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
    auto r = getLocalBounds().toFloat().reduced (4.0f, 4.0f);
    const float press = down ? 2.0f : 0.0f;
    const float sc = materials::scaleOf (g);

    // the hole in the panel the key sits in: dark, with a lit lower lip
    auto hole = r.expanded (2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.drawRoundedRectangle (hole.translated (0, 1.0f), 8.0f, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0, hole.getY(),
                                             juce::Colours::black.withAlpha (0.30f), 0, hole.getBottom(), false));
    g.fillRoundedRectangle (hole, 8.0f);

    // soft shadow cast onto the panel, shorter when the key is pressed
    juce::DropShadow (juce::Colours::black.withAlpha (down ? 0.4f : 0.6f), down ? 3 : 7, { 1, down ? 1 : 4 })
        .drawForRectangle (g, r.toNearestInt().reduced (3));

    r = r.translated (0, press);
    const int w = (int) std::ceil (r.getWidth() * sc), h = (int) std::ceil (r.getHeight() * sc);
    const juce::Colour albedo = cap == Cap::Dark ? colours::keyDark : cap == Cap::Red ? colours::keyRed : colours::keyLight;
    const auto key = "key:" + juce::String (w) + "x" + juce::String (h) + ":" + albedo.toString();
    const auto& img = materials::cached (key, [=] {
        auto sh = materials::plastic();
        if (cap != Cap::Light)
        {
            sh.specular = 0.22f; // darker rubber reads softer
            sh.shininess = 18.0f;
            sh.bump = 0.25f;
        }
        return materials::pillow (w, h, 7.0f * sc, juce::jmin (w, h) * 0.24f, 2.0f * sc, albedo, sh, 17);
    });
    materials::drawAt (g, img, r);
    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillRoundedRectangle (r, 7.0f);
    }
    else if (over)
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (r, 7.0f);
    }

    // LED: centred above the legend on tall keys, top-left on small ones
    auto textArea = r.toNearestInt().reduced (5, 3);
    const bool tall = r.getHeight() >= 50.0f;
    if (hasLed)
    {
        const bool on = lit && ! (blink && (juce::Time::getMillisecondCounter() / 250) % 2 == 0);
        const float d = juce::jlimit (4.5f, 6.5f, r.getHeight() * 0.12f);
        auto ledR = tall ? juce::Rectangle<float> (d, d).withCentre ({ r.getCentreX(), r.getY() + 9.0f })
                         : juce::Rectangle<float> (r.getX() + 8.0f, r.getY() + 7.0f, d, d);
        drawLed (g, ledR, ledColour, on ? 1.0f : 0.0f);
        if (tall)
            textArea.removeFromTop (12);
    }

    // pad-printed legend, very slightly embossed by the light
    const bool lightText = cap != Cap::Light;
    const auto ink = (lightText ? colours::silkLight : colours::silk).withMultipliedAlpha (isEnabled() ? 0.92f : 0.4f);
    g.setFont (labelFont (juce::jlimit (10.0f, 14.0f, getHeight() * 0.27f)));
    auto ta = textArea.withTrimmedTop (hasLed && ! tall ? 4 : 0);
    g.setColour ((lightText ? juce::Colours::black : juce::Colours::white).withAlpha (0.22f));
    g.drawFittedText (getButtonText(), ta.translated (0, 1), juce::Justification::centred, 2, 0.85f);
    g.setColour (ink);
    g.drawFittedText (getButtonText(), ta, juce::Justification::centred, 2, 0.85f);
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
}

} // namespace padlab::theme
