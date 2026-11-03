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
    setFont (theme::labelFont (10.5f, true));
    setColour (juce::Label::textColourId, theme::colours::textDim);
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
    rebuild();
}

int Waveform::frameAt (int x) const
{
    if (sample == nullptr || getWidth() <= 0)
        return 0;
    return juce::jlimit (0, sample->getNumFrames(), (int) ((double) x / getWidth() * sample->getNumFrames()));
}

int Waveform::xFor (int frame) const
{
    if (sample == nullptr || sample->getNumFrames() == 0)
        return 0;
    return (int) ((double) frame / sample->getNumFrames() * getWidth());
}

void Waveform::rebuild()
{
    peaks.clear();
    if (sample == nullptr || getWidth() <= 0)
    {
        repaint();
        return;
    }
    const int w = getWidth();
    const int n = sample->getNumFrames();
    peaks.resize ((size_t) w);
    for (int x = 0; x < w; ++x)
    {
        const int a = (int) ((int64_t) n * x / w);
        const int b = std::max (a + 1, (int) ((int64_t) n * (x + 1) / w));
        float lo = 0.0f, hi = 0.0f;
        for (int c = 0; c < sample->buffer.getNumChannels(); ++c)
        {
            auto range = juce::FloatVectorOperations::findMinAndMax (sample->buffer.getReadPointer (c, a), std::min (b, n) - a);
            lo = std::min (lo, range.getStart());
            hi = std::max (hi, range.getEnd());
        }
        peaks[(size_t) x] = { lo, hi };
    }
    repaint();
}

void Waveform::paint (juce::Graphics& g)
{
    using namespace theme::colours;
    auto r = getLocalBounds();
    g.setColour (screen.darker (0.3f));
    g.fillRect (r);
    g.setColour (grid);
    g.drawHorizontalLine (r.getCentreY(), 0.0f, (float) r.getWidth());

    if (sample == nullptr)
    {
        g.setColour (textDim);
        g.setFont (theme::screenFont (14.0f));
        g.drawText (emptyText, r, juce::Justification::centred);
        return;
    }

    // region
    if (regionStart >= 0)
    {
        const int x0 = xFor (regionStart), x1 = xFor (regionEnd < 0 ? sample->getNumFrames() : regionEnd);
        g.setColour (accent2.withAlpha (0.12f));
        g.fillRect (x0, 0, std::max (1, x1 - x0), r.getHeight());
    }
    // selected slice
    if (selectedSlice >= 0 && selectedSlice < (int) markers.size())
    {
        const int x0 = xFor (markers[(size_t) selectedSlice]);
        const int x1 = selectedSlice + 1 < (int) markers.size() ? xFor (markers[(size_t) selectedSlice + 1]) : r.getWidth();
        g.setColour (accent.withAlpha (0.16f));
        g.fillRect (x0, 0, std::max (1, x1 - x0), r.getHeight());
    }

    const float mid = r.getHeight() * 0.5f, scale = r.getHeight() * 0.46f;
    g.setColour (accent.withAlpha (0.9f));
    for (int x = 0; x < (int) peaks.size(); ++x)
        g.drawVerticalLine (x, mid - peaks[(size_t) x].second * scale, mid - peaks[(size_t) x].first * scale + 1.0f);

    g.setFont (theme::screenFont (11.0f));
    for (size_t i = 0; i < markers.size(); ++i)
    {
        const int x = xFor (markers[i]);
        g.setColour (text.withAlpha (0.8f));
        g.drawVerticalLine (x, 0.0f, (float) r.getHeight());
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRect (x, 0, 22, 14);
        g.setColour (text);
        g.drawText (juce::String ((int) i + 1), x + 2, 0, 20, 14, juce::Justification::centredLeft);
    }
    if (playhead >= 0)
    {
        g.setColour (play);
        g.drawVerticalLine (xFor (playhead), 0.0f, (float) r.getHeight());
    }
}

void Waveform::mouseDown (const juce::MouseEvent& e)
{
    if (onClick && sample != nullptr)
        onClick (frameAt (e.x), e.mods);
}

} // namespace padlab
