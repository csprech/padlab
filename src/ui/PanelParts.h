#pragma once

// Small hardware parts of the front panel: the CURSOR pad, the soft-key labels on the LCD,
// the floppy drive bay and the pad-bank LEDs.

#include "Theme.h"

namespace padlab
{
/** Four-way cursor key cluster (a cream rounded block split into up/down/left/right). Auto-repeats. */
class CursorPad : public juce::Component, private juce::Timer
{
public:
    std::function<void (int dx, int dy)> onMove;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    int zoneAt (juce::Point<float> p) const; // 0 up, 1 right, 2 down, 3 left
    void fire();
    int pressed = -1;
    bool repeating = false;
};

/** The F1..F6 soft-key labels along the bottom of the LCD; the active one is shown inverse. */
class SoftKeyTabs : public juce::Component
{
public:
    SoftKeyTabs() { setInterceptsMouseClicks (false, false); }
    void setLabels (const juce::StringArray& l) { labels = l; repaint(); }
    void setActive (int i) { if (i != active) { active = i; repaint(); } }
    /** x-centres of the soft keys in this component's coordinates, so labels line up with the keys. */
    void setKeyCentres (std::vector<int> c) { centres = std::move (c); repaint(); }
    void paint (juce::Graphics&) override;

private:
    juce::StringArray labels;
    std::vector<int> centres;
    int active = -1;
};

/** A 3.5" floppy drive bay on the top panel; click it for the DISK menu. */
class FloppyDrive : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;
    void setBusy (bool b) { if (b != busy) { busy = b; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (onClick && getLocalBounds().contains (e.getPosition()))
            onClick();
    }

private:
    bool busy = false;
};

/** Eight bank LEDs (A-H) in two columns; clicking a letter selects that bank. */
class BankLeds : public juce::Component
{
public:
    std::function<void (int bank)> onSelect;
    void setBank (int b) { if (b != bank) { bank = b; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> cell (int i) const;
    int bank = 0;
};

/** Ventilation slots: groups of horizontal slots punched in the panel (static; drawn into the background). */
void drawVentGrille (juce::Graphics&, juce::Rectangle<float> area, int groups, int slotsPerGroup);

} // namespace padlab
