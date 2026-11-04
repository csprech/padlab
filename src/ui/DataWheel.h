#pragma once

// The big DATA wheel: turn it (drag around it, scroll, or Up/Down keys) to step whatever
// value was touched last, like the jog wheel on classic hardware.

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

namespace padlab
{
class DataWheel : public juce::Component
{
public:
    DataWheel();

    /** Called with +/- detent steps as the wheel turns. */
    std::function<void (int steps)> onStep;

    /** Turns the wheel by a number of detents (keyboard), firing onStep. */
    void nudge (int steps);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int kDetents = 32; // per revolution

private:
    void turn (float radians);
    juce::Rectangle<float> wheelBounds() const;

    float angle = 0.0f;     // visual angle (radians)
    float pending = 0.0f;   // rotation not yet converted into steps
    float lastPointer = 0.0f;
    juce::Point<float> lastPos;

    int cachedDiameter = 0;
    std::map<int, juce::Image> bases;                                    // by knurl phase step
    std::map<int, std::pair<juce::Point<int>, juce::Image>> dimples;     // by angle step
};
} // namespace padlab
