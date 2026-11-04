#include "ui/DataWheel.h"
#include "ui/Materials.h"
#include <cmath>

namespace padlab
{
namespace
{
    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
    constexpr float kDetent = kTwoPi / (float) DataWheel::kDetents;
    constexpr int kPhaseSteps = 4;    // knurl phase images per tooth (90 teeth -> 1 degree)
    constexpr int kAngleSteps = 360;  // dimple positions per revolution

    float wrap (float a)
    {
        a = std::fmod (a, kTwoPi);
        return a < 0 ? a + kTwoPi : a;
    }
} // namespace

DataWheel::DataWheel()
{
    setRepaintsOnMouseActivity (false);
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

juce::Rectangle<float> DataWheel::wheelBounds() const
{
    const float d = std::floor (juce::jmin (getWidth(), getHeight()) * 0.84f);
    return juce::Rectangle<float> (d, d).withCentre (getLocalBounds().toFloat().getCentre());
}

void DataWheel::paint (juce::Graphics& g)
{
    const auto wb = wheelBounds();
    const auto c = wb.getCentre();
    const float R = wb.getWidth() * 0.5f;

    // the black moulded collar the wheel sits in
    auto collar = wb.expanded (R * 0.12f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawEllipse (collar.translated (0, 1.2f).expanded (0.6f), 1.2f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2c2b), c.x, collar.getY(), juce::Colour (0xff0d0d0d), c.x,
                                             collar.getBottom(), false));
    g.fillEllipse (collar);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), collar.getX(), collar.getY(),
                                             juce::Colours::transparentWhite, c.x, c.y, false));
    g.drawEllipse (collar.reduced (1.0f), 1.5f);
    // gap between collar and wheel, deepest at the top-left (away from the light)
    g.setColour (juce::Colours::black);
    g.fillEllipse (wb.expanded (2.0f));
    // short cast shadow from the wheel onto the collar floor
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (wb.translated (1.5f, 3.0f).expanded (1.0f));

    const float sc = materials::scaleOf (g);
    const int d = (int) std::ceil (wb.getWidth() * sc);
    if (d != cachedDiameter)
    {
        bases.clear();
        dimples.clear();
        cachedDiameter = d;
    }

    // the knurl repeats every tooth (90 per turn): only a handful of base images are needed
    const float toothPeriod = kTwoPi / 90.0f;
    const int phaseIdx = (int) std::floor (wrap (angle) / toothPeriod * kPhaseSteps) % kPhaseSteps;
    auto& base = bases[phaseIdx];
    if (base.isNull())
        base = materials::wheel (d, phaseIdx * toothPeriod / kPhaseSteps);

    const auto toLogical = juce::AffineTransform::scale (1.0f / sc).translated (wb.getX(), wb.getY());
    g.setOpacity (1.0f); // setColour() above left a translucent opacity behind
    g.drawImageTransformed (base, toLogical);

    const int angleIdx = (int) std::lround (wrap (angle) / kTwoPi * kAngleSteps) % kAngleSteps;
    auto& dim = dimples[angleIdx];
    if (dim.second.isNull())
        dim.second = materials::wheelDimple (d, angleIdx * kTwoPi / kAngleSteps, 0.0f, dim.first);
    g.drawImageTransformed (dim.second, juce::AffineTransform::translation ((float) dim.first.x, (float) dim.first.y)
                                            .followedBy (toLogical));
}

void DataWheel::turn (float radians)
{
    angle = wrap (angle + radians);
    pending += radians;
    int steps = 0;
    while (pending >= kDetent) { pending -= kDetent; ++steps; }
    while (pending <= -kDetent) { pending += kDetent; --steps; }
    repaint();
    if (steps != 0 && onStep)
        onStep (steps);
}

void DataWheel::nudge (int steps)
{
    angle = wrap (angle + steps * kDetent);
    repaint();
    if (steps != 0 && onStep)
        onStep (steps);
}

void DataWheel::mouseDown (const juce::MouseEvent& e)
{
    const auto c = wheelBounds().getCentre();
    lastPointer = std::atan2 (e.position.x - c.x, -(e.position.y - c.y));
    lastPos = e.position;
}

void DataWheel::mouseDrag (const juce::MouseEvent& e)
{
    const auto wb = wheelBounds();
    const auto c = wb.getCentre();
    const float r = e.position.getDistanceFrom (c);
    const float a = std::atan2 (e.position.x - c.x, -(e.position.y - c.y));
    float delta;
    if (r > wb.getWidth() * 0.12f)
    {
        // grab it and turn it: follow the pointer around the centre
        delta = a - lastPointer;
        if (delta > juce::MathConstants<float>::pi) delta -= kTwoPi;
        if (delta < -juce::MathConstants<float>::pi) delta += kTwoPi;
    }
    else
    {
        delta = (lastPos.y - e.position.y) * 0.02f; // too close to the hub to track an angle: drag up/down
    }
    lastPointer = a;
    lastPos = e.position;
    turn (delta);
}

void DataWheel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float d = (w.deltaY != 0.0f ? w.deltaY : -w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    if (w.isSmooth)
        turn (d * kTwoPi * 0.35f); // trackpad: continuous
    else if (d != 0.0f)
        turn (d > 0 ? kDetent : -kDetent); // mouse wheel: one detent per notch
}

} // namespace padlab
