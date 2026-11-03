#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace padlab::theme
{
namespace colours
{
    const juce::Colour body       { 0xff1b1d21 };
    const juce::Colour bodyLight  { 0xff26292e };
    const juce::Colour bodyEdge   { 0xff0e0f11 };
    const juce::Colour screen     { 0xff0b0f14 };
    const juce::Colour screenLine { 0xff1f2a36 };
    const juce::Colour grid       { 0xff16202b };
    const juce::Colour accent     { 0xffff7a1a }; // amber
    const juce::Colour accent2    { 0xff2ec4b6 }; // teal
    const juce::Colour record     { 0xffe5383b };
    const juce::Colour play       { 0xff3ddc84 };
    const juce::Colour text       { 0xffe8e6e3 };
    const juce::Colour textDim    { 0xff8a9099 };
    const juce::Colour pad        { 0xff34373d };
    const juce::Colour padEdge    { 0xff4a4e55 };
}

juce::Font screenFont (float height, bool bold = false);
juce::Font labelFont (float height, bool bold = false);

/** Velocity -> pad glow colour (amber -> hot red). */
juce::Colour padGlow (float velocity01);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
};

/** A hardware-style button with an LED: set "ledColour" via setColour(TextButton::buttonOnColourId). */
class HardButton : public juce::TextButton
{
public:
    explicit HardButton (const juce::String& text, juce::Colour led = colours::accent);
    void setLit (bool lit);
    bool isLit() const { return lit; }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void setBlink (bool b) { blink = b; repaint(); }

private:
    juce::Colour ledColour;
    bool lit = false, blink = false;
};

} // namespace padlab::theme
