#pragma once

#include "ui/AppContext.h"
#include "ui/Theme.h"

namespace padlab
{
/** The 4x4 velocity/pressure pads (pad 1 bottom-left, like the hardware). */
class PadGrid : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit PadGrid (AppContext& ctx);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int x, int y) override;
    void fileDragMove (const juce::StringArray&, int x, int y) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    /** Keyboard-triggered pads light up too. */
    void flashPad (int index16, int velocity);

    /** Hit a pad (index 0..15 in the current bank) as if pressed. */
    void press (int index16, int velocity);
    void release (int index16);

private:
    void timerCallback() override;
    int padAt (juce::Point<int> p) const;
    juce::Rectangle<float> padBounds (int index16) const;
    int velocityFor (int index16, juce::Point<int> p) const;

public:
    static constexpr float kMargin = 14.0f, kGap = 12.0f, kLabelH = 20.0f;
    /** Height needed for square pads at a given width. */
    static int heightForWidth (int w)
    {
        const float size = (w - kMargin * 2 - kGap * 3) / 4.0f;
        return (int) std::ceil (kMargin * 2 + kGap * 3 + (size + kLabelH) * 4);
    }

private:
    AppContext& ctx;
    std::array<float, kPadsPerBank> glow {};
    std::array<uint32_t, kNumPads> lastHits {};
    std::array<juce::String, kPadsPerBank> names;
    std::array<bool, kPadsPerBank> hasSample {}, padMuted {}, trackMuted {};
    int mousePad = -1;
    std::array<int, kPadsPerBank> pressedAbs = [] { std::array<int, kPadsPerBank> a {}; a.fill (-1); return a; }();
    int dragHover = -1;
    std::array<int, kPadsPerBank> lastPainted = [] { std::array<int, kPadsPerBank> a {}; a.fill (-1); return a; }();
};

} // namespace padlab
