#include "ui/Theme.h"

namespace padlab::theme
{
juce::Font screenFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font labelFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Colour padGlow (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    return juce::Colour (0xffffb347).interpolatedWith (juce::Colour (0xffff2d2d), v * v);
}

LookAndFeel::LookAndFeel()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, body);
    setColour (juce::TextButton::buttonColourId, bodyLight);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, screen);
    setColour (juce::ComboBox::outlineColourId, screenLine);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, accent);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff15191f));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, screen);
    setColour (juce::Slider::textBoxOutlineColourId, screenLine);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::backgroundColourId, juce::Colour (0xff1c2530));
    setColour (juce::Slider::thumbColourId, text);
    setColour (juce::TextEditor::backgroundColourId, screen);
    setColour (juce::TextEditor::outlineColourId, screenLine);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::AlertWindow::backgroundColourId, bodyLight);
    setColour (juce::ListBox::backgroundColourId, screen);
    setColour (juce::ScrollBar::thumbColourId, screenLine.brighter (0.4f));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : bg;
    if (down)
        c = c.darker (0.25f);
    else if (over)
        c = c.brighter (0.08f);
    g.setColour (colours::bodyEdge);
    g.fillRoundedRectangle (r.translated (0, 1.5f), 4.0f);
    g.setGradientFill (juce::ColourGradient (c.brighter (0.06f), r.getX(), r.getY(), c.darker (0.12f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                     .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 2);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return labelFont (juce::jlimit (10.0f, 14.0f, h * 0.42f), true);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& c) { return screenFont (juce::jmin (14.0f, c.getHeight() * 0.6f)); }
juce::Font LookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end,
                                    juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = start + pos * (end - start);

    // arc track
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, end, true);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? start + (float) s.valueToProportionOfLength (0.0) * (end - start) : start;
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (juce::Colour (0xff11151a));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (s.findColour (juce::Slider::trackColourId));
    g.strokePath (value, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob body
    const float kr = radius * 0.74f;
    g.setColour (colours::bodyEdge);
    g.fillEllipse (centre.x - kr, centre.y - kr + 2.0f, kr * 2, kr * 2);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4b5058), centre.x, centre.y - kr,
                                             juce::Colour (0xff23262b), centre.x, centre.y + kr, false));
    g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2, kr * 2);
    // pointer
    juce::Path p;
    p.addRoundedRectangle (-1.5f, -kr + 3.0f, 3.0f, kr * 0.45f, 1.5f);
    g.setColour (colours::text);
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (centre));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float min, float max,
                                    juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
    {
        auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
        g.setColour (s.findColour (juce::Slider::backgroundColourId));
        g.fillRoundedRectangle (r, 3.0f);
        auto fill = style == juce::Slider::LinearBar ? r.withRight (pos) : r.withTop (pos);
        g.setColour (s.findColour (juce::Slider::trackColourId).withAlpha (0.55f));
        g.fillRoundedRectangle (fill, 3.0f);
        return;
    }
    if (style == juce::Slider::LinearVertical)
    {
        auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
        auto slot = r.withSizeKeepingCentre (6.0f, r.getHeight());
        g.setColour (juce::Colour (0xff0d1014));
        g.fillRoundedRectangle (slot, 3.0f);
        g.setColour (s.findColour (juce::Slider::trackColourId).withAlpha (0.7f));
        g.fillRoundedRectangle (slot.withTop (pos), 3.0f);
        auto cap = juce::Rectangle<float> (r.getWidth() * 0.8f, 14.0f).withCentre ({ r.getCentreX(), pos });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5f68), cap.getX(), cap.getY(),
                                                 juce::Colour (0xff2a2d33), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 2.5f);
        g.setColour (colours::text);
        g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 6.0f, 1.5f));
        return;
    }
    LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, min, max, style, s);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 0.9f : 0.3f));
    g.fillPath (arrow);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.22f);
    g.setColour (colours::screen);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (over ? colours::accent : colours::screenLine.brighter (0.3f));
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    if (b.getToggleState())
    {
        g.setColour (colours::accent);
        g.fillRoundedRectangle (box.reduced (3.0f), 2.0f);
    }
    g.setColour (colours::text.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.setFont (labelFont (13.0f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (4), juce::Justification::centredLeft, 1);
}

// ---------------------------------------------------------------- HardButton

HardButton::HardButton (const juce::String& text, juce::Colour led) : juce::TextButton (text), ledColour (led) {}

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
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    auto base = juce::Colour (0xff2c2f35);
    if (down)
        base = base.darker (0.3f);
    else if (over)
        base = base.brighter (0.07f);

    g.setColour (colours::bodyEdge);
    g.fillRoundedRectangle (r.translated (0, 2.0f), 5.0f);
    g.setGradientFill (juce::ColourGradient (base.brighter (0.1f), r.getX(), r.getY(), base.darker (0.15f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    // LED strip
    const bool on = lit && ! (blink && (juce::Time::getMillisecondCounter() / 250) % 2 == 0);
    auto led = r.removeFromTop (6.0f).reduced (r.getWidth() * 0.28f, 1.5f);
    if (on)
    {
        g.setColour (ledColour.withAlpha (0.35f));
        g.fillRoundedRectangle (led.expanded (3.0f, 2.0f), 3.0f);
    }
    g.setColour (on ? ledColour : juce::Colour (0xff3b3f46));
    g.fillRoundedRectangle (led, 1.5f);

    g.setColour (colours::text.withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f));
    g.setFont (labelFont (juce::jlimit (9.5f, 13.0f, getHeight() * 0.3f), true));
    g.drawFittedText (getButtonText(), r.toNearestInt().reduced (3, 0), juce::Justification::centred, 2, 0.8f);
}

} // namespace padlab::theme
