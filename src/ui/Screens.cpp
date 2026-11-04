#include "ui/Screens.h"

namespace padlab
{
void setupParam (juce::Slider& s, double min, double max, double step, const juce::String& suffix, double skewMid)
{
    s.setSliderStyle (juce::Slider::LinearBar);
    s.setRange (min, max, step);
    if (skewMid > min && skewMid < max)
        s.setSkewFactorFromMidPoint (skewMid);
    s.setTextValueSuffix (suffix);
    s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 80, 20);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setVelocityBasedMode (false);
    s.setMouseDragSensitivity (220);
}

void setupCombo (juce::ComboBox& c, const juce::StringArray& items, int firstId)
{
    c.clear (juce::dontSendNotification);
    c.addItemList (items, firstId);
}

Caption::Caption (const juce::String& text) : juce::Label ({}, text.toUpperCase())
{
    setFont (theme::screenFont (10.5f, true));
    setColour (juce::Label::textColourId, theme::lcd::textDim);
    setJustificationType (juce::Justification::bottomLeft);
    setInterceptsMouseClicks (false, false);
}

juce::String describeLayer (const Layer& l)
{
    if (l.sample == nullptr)
        return {};
    if (l.sliceStart > 0 || l.sliceEnd >= 0)
        return l.sample->name + " @" + juce::String (l.sliceStart / l.sample->sampleRate, 2) + "s";
    return l.sample->name;
}

// ---------------------------------------------------------------- Waveform

void Waveform::setSample (SamplePtr s)
{
    if (s == sample)
        return;
    sample = std::move (s);
    playhead = -1;
    viewStart = viewLength = 0; // a new sample starts zoomed out
    rebuild();
}

int Waveform::getViewLength() const
{
    if (sample == nullptr)
        return 0;
    return viewLength > 0 ? viewLength : sample->getNumFrames();
}

int Waveform::frameAt (int x) const
{
    if (sample == nullptr || getWidth() <= 0)
        return 0;
    const double f = getViewStart() + (double) x / getWidth() * getViewLength();
    return juce::jlimit (0, sample->getNumFrames(), (int) std::lround (f));
}

int Waveform::xFor (int frame) const
{
    if (sample == nullptr || getViewLength() <= 0)
        return 0;
    return (int) std::lround ((double) (frame - getViewStart()) / getViewLength() * getWidth());
}

void Waveform::setView (int start, int length)
{
    if (sample == nullptr)
        return;
    const int n = sample->getNumFrames();
    // closest zoom: about 16 px per frame, so a chop can go on an exact sample
    const int minLength = juce::jmin (n, juce::jmax (8, getWidth() / 16));
    int newLength = length <= 0 || length >= n ? 0 : juce::jmax (minLength, length);
    int newStart = newLength == 0 ? 0 : juce::jlimit (0, n - newLength, start);
    if (newLength == viewLength && newStart == viewStart)
        return;
    viewLength = newLength;
    viewStart = newStart;
    rebuild();
}

void Waveform::zoomAround (int x, double factor)
{
    if (sample == nullptr || getWidth() <= 0 || factor <= 0.0)
        return;
    const double t = juce::jlimit (0.0, 1.0, (double) x / getWidth());
    const double anchor = getViewStart() + t * getViewLength(); // this frame stays under the pointer
    const double newLength = getViewLength() / factor;
    setView ((int) std::lround (anchor - t * newLength), (int) std::lround (newLength));
}

void Waveform::reveal (int frame)
{
    if (! isZoomed() || (frame >= viewStart && frame < viewStart + viewLength))
        return;
    setView (frame - viewLength / 4, viewLength);
}

void Waveform::rebuild()
{
    peaks.clear();
    points.clear();
    if (sample == nullptr || getWidth() <= 0)
    {
        repaint();
        return;
    }
    const int w = getWidth();
    const int n = sample->getNumFrames();
    const int start = getViewStart(), len = getViewLength();
    const int channels = sample->buffer.getNumChannels();
    if (len < w)
    {
        // zoomed right in: draw the samples themselves (one more each side, so the line reaches the edges)
        const int a = juce::jmax (0, start - 1), b = juce::jmin (n, start + len + 1);
        points.resize ((size_t) (b - a));
        for (int i = a; i < b; ++i)
        {
            float v = 0.0f;
            for (int c = 0; c < channels; ++c)
                v += sample->buffer.getSample (c, i);
            points[(size_t) (i - a)] = v / (float) juce::jmax (1, channels);
        }
        repaint();
        return;
    }
    peaks.resize ((size_t) w);
    for (int x = 0; x < w; ++x)
    {
        const int a = start + (int) ((int64_t) len * x / w);
        // (one frame past this pixel too, so neighbouring columns join up)
        const int b = juce::jmin (n, juce::jmax (a + 1, start + (int) ((int64_t) len * (x + 1) / w)) + 1);
        float lo = std::numeric_limits<float>::max(), hi = std::numeric_limits<float>::lowest();
        for (int c = 0; c < channels; ++c)
        {
            auto range = juce::FloatVectorOperations::findMinAndMax (sample->buffer.getReadPointer (c, a), b - a);
            lo = std::min (lo, range.getStart());
            hi = std::max (hi, range.getEnd());
        }
        peaks[(size_t) x] = { lo, hi };
    }
    repaint();
}

juce::Rectangle<int> Waveform::overviewBounds() const
{
    return getLocalBounds().removeFromBottom (8);
}

void Waveform::panOverviewTo (int x)
{
    if (sample == nullptr || ! isZoomed() || getWidth() <= 0)
        return;
    const int frame = (int) ((double) x / getWidth() * sample->getNumFrames());
    setView (frame - overviewGrab, viewLength);
}

void Waveform::paint (juce::Graphics& g)
{
    using namespace theme::lcd;
    auto r = getLocalBounds();
    g.setColour (grid);
    g.drawHorizontalLine (r.getCentreY(), 0.0f, (float) r.getWidth());

    if (sample == nullptr)
    {
        g.setColour (textDim);
        g.setFont (theme::screenFont (14.0f));
        g.drawText (emptyText, r, juce::Justification::centred);
        return;
    }
    const int w = r.getWidth();
    auto clampX = [w] (int x) { return juce::jlimit (-1, w + 1, x); };

    // region
    if (regionStart >= 0)
    {
        const int x0 = clampX (xFor (regionStart)), x1 = clampX (xFor (regionEnd < 0 ? sample->getNumFrames() : regionEnd));
        g.setColour (accent2.withAlpha (0.12f));
        g.fillRect (x0, 0, std::max (1, x1 - x0), r.getHeight());
    }
    // selected slice
    if (selectedSlice >= 0 && selectedSlice < (int) markers.size())
    {
        const int x0 = clampX (xFor (markers[(size_t) selectedSlice]));
        const int x1 = selectedSlice + 1 < (int) markers.size() ? clampX (xFor (markers[(size_t) selectedSlice + 1]))
                                                                  : clampX (xFor (sample->getNumFrames()));
        g.setColour (accent.withAlpha (0.16f));
        g.fillRect (x0, 0, std::max (1, x1 - x0), r.getHeight());
    }

    const float mid = r.getHeight() * 0.5f, scale = r.getHeight() * 0.46f;
    g.setColour (accent.withAlpha (0.9f));
    if (! points.empty())
    {
        // zoomed in past one frame per pixel: a line through the samples, with a dot on each once they're apart
        const int first = juce::jmax (0, getViewStart() - 1);
        const double pxPerFrame = (double) w / getViewLength();
        juce::Path line;
        for (size_t i = 0; i < points.size(); ++i)
        {
            const float x = (float) ((first + (int) i - getViewStart()) * pxPerFrame), y = mid - points[i] * scale;
            if (i == 0)
                line.startNewSubPath (x, y);
            else
                line.lineTo (x, y);
        }
        g.strokePath (line, juce::PathStrokeType (1.2f));
        if (pxPerFrame >= 5.0)
            for (size_t i = 0; i < points.size(); ++i)
                g.fillEllipse ((float) ((first + (int) i - getViewStart()) * pxPerFrame) - 1.5f, mid - points[i] * scale - 1.5f, 3.0f, 3.0f);
    }
    else
        for (int x = 0; x < (int) peaks.size(); ++x)
            g.drawVerticalLine (x, mid - peaks[(size_t) x].second * scale, mid - peaks[(size_t) x].first * scale + 1.0f);

    g.setFont (theme::screenFont (11.0f));
    for (size_t i = 0; i < markers.size(); ++i)
    {
        const int x = xFor (markers[i]);
        if (x < -22 || x > w)
            continue; // off screen while zoomed
        g.setColour (text.withAlpha (0.8f));
        g.drawVerticalLine (x, 0.0f, (float) r.getHeight());
        g.setColour (text);
        g.fillRect (x, 0, 22, 14);
        g.setColour (screen);
        g.drawText (juce::String ((int) i + 1), x + 2, 0, 20, 14, juce::Justification::centredLeft);
    }
    if (playhead >= 0)
        if (const int x = xFor (playhead); x >= 0 && x <= w)
        {
            g.setColour (play);
            g.drawVerticalLine (x, 0.0f, (float) r.getHeight());
        }

    // while zoomed: how far in, and where (the strip along the bottom: the whole sample, the bright part is on screen)
    if (isZoomed())
    {
        const auto strip = overviewBounds();
        const int n = sample->getNumFrames();
        g.setColour (screen.withAlpha (0.85f));
        g.fillRect (strip);
        g.setColour (grid);
        g.drawRect (strip, 1);
        for (int m : markers)
            g.drawVerticalLine ((int) ((double) m / n * w), (float) strip.getY(), (float) strip.getBottom());
        const int x0 = (int) ((double) viewStart / n * w), x1 = (int) std::ceil ((double) (viewStart + viewLength) / n * w);
        g.setColour (accent.withAlpha (0.75f));
        g.fillRect (juce::Rectangle<int> (x0, strip.getY() + 1, juce::jmax (3, x1 - x0), strip.getHeight() - 2));
        const double zoom = (double) n / viewLength;
        g.setColour (textDim);
        g.setFont (theme::screenFont (10.5f));
        g.drawText ("ZOOM x" + (zoom < 10.0 ? juce::String (zoom, 1) : juce::String (juce::roundToInt (zoom))) + "   0 = show all",
                    r.withTrimmedBottom (strip.getHeight() + 2).removeFromBottom (14).withTrimmedRight (4), juce::Justification::centredRight);
    }
}

int Waveform::markerNear (int x) const
{
    int best = -1, bestDist = 6; // pixels
    for (int i = 1; i < (int) markers.size(); ++i)
        if (const int d = std::abs (xFor (markers[(size_t) i]) - x); d <= bestDist)
        {
            bestDist = d;
            best = i;
        }
    return best;
}

void Waveform::mouseMove (const juce::MouseEvent& e)
{
    if (isZoomed() && overviewBounds().contains (e.getPosition()))
        return setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setMouseCursor (markerNear (e.x) > 0 && ! e.mods.isAnyModifierKeyDown() ? juce::MouseCursor::LeftRightResizeCursor
                                                                            : juce::MouseCursor::NormalCursor);
}

void Waveform::mouseDown (const juce::MouseEvent& e)
{
    if (sample == nullptr)
        return;
    dragging = -1;
    draggingOverview = false;
    if (isZoomed() && overviewBounds().contains (e.getPosition()))
    {
        // grab the strip: inside the bright window it moves from where you grabbed it, elsewhere it jumps there
        draggingOverview = true;
        const int frame = (int) ((double) e.x / getWidth() * sample->getNumFrames());
        overviewGrab = frame >= viewStart && frame < viewStart + viewLength ? frame - viewStart : viewLength / 2;
        panOverviewTo (e.x);
        return;
    }
    // grab a marker line to move it (plain click right on a marker)
    if (! e.mods.isAnyModifierKeyDown() && e.getNumberOfClicks() == 1)
        if (const int m = markerNear (e.x); m > 0 && onMarkerDragged)
        {
            dragging = m;
            return;
        }
    if (onClick && e.getNumberOfClicks() == 1)
        onClick (frameAt (e.x), e.mods);
}

void Waveform::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingOverview)
        return panOverviewTo (e.x);
    if (dragging > 0 && onMarkerDragged && sample != nullptr)
    {
        // dragging a chop past the edge while zoomed scrolls the view along with it
        if (isZoomed() && (e.x < 0 || e.x > getWidth()))
            setView (viewStart + (e.x < 0 ? -viewLength / 20 : viewLength / 20), viewLength);
        onMarkerDragged (dragging, juce::jlimit (0, sample->getNumFrames() - 1, frameAt (juce::jlimit (0, getWidth(), e.x))));
    }
}

void Waveform::mouseUp (const juce::MouseEvent&)
{
    if (dragging > 0 && onMarkerDragEnd)
        onMarkerDragEnd();
    dragging = -1;
    draggingOverview = false;
}

void Waveform::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (isZoomed() && overviewBounds().contains (e.getPosition()))
        return;
    if (onDoubleClick && sample != nullptr)
        onDoubleClick (frameAt (e.x));
}

void Waveform::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (sample == nullptr)
        return juce::Component::mouseWheelMove (e, wheel);
    // the wheel turned away from you zooms in, whatever the system's scroll direction
    const float zoomDelta = wheel.isReversed ? -wheel.deltaY : wheel.deltaY;
    float panDelta = wheel.deltaX;
    if (e.mods.isShiftDown() && std::abs (panDelta) < 1.0e-6f)
        panDelta = wheel.deltaY; // shift + wheel pans on a plain mouse
    if (std::abs (panDelta) > std::abs (zoomDelta) || e.mods.isShiftDown())
    {
        if (isZoomed())
            setView (viewStart - (int) std::lround (panDelta * 3.0 * viewLength), viewLength);
        return;
    }
    zoomAround (e.x, std::pow (2.0, zoomDelta * 12.0)); // about 1.4x a notch
}

void Waveform::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    zoomAround (e.x, scaleFactor); // trackpad pinch
}

} // namespace padlab
