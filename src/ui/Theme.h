#pragma once

// Visual language: a classic 90s sampling workstation.
//   * warm grey speckled chassis, darker pad deck, printed (silkscreen) labels
//   * dark grey rubber pads and rubber key caps with LEDs, knurled black knobs
//   * a backlit green LCD with dark dot-matrix ink
//
// Two LookAndFeels: HardwareLookAndFeel (panel) and LcdLookAndFeel (everything on the screen).

#include <juce_gui_basics/juce_gui_basics.h>

namespace padlab::theme
{
/** Hardware (panel) palette. */
namespace colours
{
    const juce::Colour chassis     { 0xffaaa79f };
    const juce::Colour chassisLow  { 0xff99968e };
    const juce::Colour deck        { 0xff8c8981 };
    const juce::Colour seam        { 0xff6b6862 };
    const juce::Colour bezel       { 0xff2a2a28 };
    const juce::Colour silk        { 0xff272624 }; // printed text on the light chassis
    const juce::Colour silkLight   { 0xfff2f0ea }; // printed text on dark parts
    const juce::Colour rubber      { 0xff4c4b48 }; // pads
    const juce::Colour keyLight    { 0xffd2cec4 };
    const juce::Colour keyDark     { 0xff5f5d59 };
    const juce::Colour keyRed      { 0xffa8453b };
    const juce::Colour ledRed      { 0xffff3a2c };
    const juce::Colour ledGreen    { 0xff63ff5a };
    const juce::Colour ledAmber    { 0xffffb11f };
    const juce::Colour ledOff      { 0xff3b3532 };

    // names shared with older code
    const juce::Colour body     = chassis;
    const juce::Colour bodyEdge = bezel;
    const juce::Colour text     = silk;
    const juce::Colour textDim  = juce::Colour (0xff5d5a54);
    const juce::Colour accent   = ledAmber;
    const juce::Colour accent2  = ledGreen;
    const juce::Colour record   = ledRed;
    const juce::Colour play     = ledGreen;
}

/** Backlit LCD palette: a single ink colour on a green backlight (same names as the old screen palette). */
namespace lcd
{
    const juce::Colour backlight   { 0xffaec65a };
    const juce::Colour backlightLo { 0xff91aa42 };
    const juce::Colour ink         { 0xff1c2713 };

    const juce::Colour screen     = backlight;
    const juce::Colour text       = ink;
    const juce::Colour accent     = ink;
    const juce::Colour accent2    = ink;
    const juce::Colour play       = ink;
    const juce::Colour record     = ink;
    const juce::Colour textDim    = ink.withAlpha (0.58f);
    const juce::Colour screenLine = ink.withAlpha (0.38f);
    const juce::Colour grid       = ink.withAlpha (0.13f);
    const juce::Colour bodyEdge   = ink;
}

/** Dot-matrix LCD font (VT323, SIL OFL). */
juce::Font screenFont (float height, bool bold = false);
/** Condensed industrial panel font for printed labels and key caps. */
juce::Font labelFont (float height, bool bold = true);

/** Tileable speckle texture (cached). */
const juce::Image& speckle (int which = 0);

/** Old name kept for the pad glow; now drives LED brightness. */
juce::Colour padGlow (float velocity01);

void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius, float angle);
void drawLed (juce::Graphics&, juce::Rectangle<float> r, juce::Colour colour, float brightness01);
/** Printed panel label (silkscreen). */
void drawSilk (juce::Graphics&, const juce::String& text, juce::Rectangle<int> r,
               juce::Justification j = juce::Justification::centred, float height = 11.0f, bool light = false);

class HardwareLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HardwareLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
};

class LcdLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LcdLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    int getPopupMenuBorderSize() override { return 2; }
};

/** A rubber key cap with an optional LED. */
class HardButton : public juce::TextButton
{
public:
    enum class Cap { Light, Dark, Red };

    explicit HardButton (const juce::String& text, juce::Colour led = colours::ledRed, Cap cap = Cap::Light);
    void setLit (bool lit);
    bool isLit() const { return lit; }
    void setBlink (bool b) { blink = b; repaint(); }
    void setHasLed (bool b) { hasLed = b; repaint(); }
    void setCap (Cap c) { cap = c; repaint(); }
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Colour ledColour;
    Cap cap;
    bool lit = false, blink = false, hasLed = true;
};

/** Paints the LCD glass on top of the screen contents: pixel grid, vignette, reflection. */
class LcdGlass : public juce::Component
{
public:
    LcdGlass() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
};

} // namespace padlab::theme
