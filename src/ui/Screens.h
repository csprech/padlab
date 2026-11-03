#pragma once

#include "ui/AppContext.h"
#include "ui/Theme.h"

namespace padlab
{
// ------------------------------------------------------------------ shared widgets

/** Compact screen parameter: a bar slider that shows its value. */
void setupParam (juce::Slider& s, double min, double max, double step, const juce::String& suffix = {},
                 double skewMid = 0.0);
void setupCombo (juce::ComboBox& c, const juce::StringArray& items, int firstId = 1);

class Caption : public juce::Label
{
public:
    explicit Caption (const juce::String& text = {});
};

/** Draws a sample with slice markers, a highlighted region and an optional playhead. */
class Waveform : public juce::Component
{
public:
    void setSample (SamplePtr s);
    SamplePtr getSample() const { return sample; }
    void setMarkers (std::vector<int> m) { markers = std::move (m); repaint(); }
    void setRegion (int start, int end) { regionStart = start; regionEnd = end; repaint(); }
    void setSelectedSlice (int i) { selectedSlice = i; repaint(); }
    void setPlayhead (int frame) { if (frame != playhead) { playhead = frame; repaint(); } }
    int frameAt (int x) const;
    int xFor (int frame) const;

    void setEmptyText (const juce::String& t) { emptyText = t; repaint(); }

    std::function<void (int frame, const juce::ModifierKeys&)> onClick;

    void paint (juce::Graphics&) override;
    void resized() override { rebuild(); }
    void mouseDown (const juce::MouseEvent& e) override;

private:
    void rebuild();
    SamplePtr sample;
    std::vector<int> markers;
    std::vector<std::pair<float, float>> peaks;
    int regionStart = -1, regionEnd = -1, selectedSlice = -1, playhead = -1;
    juce::String emptyText { "No sample on this pad - drop a file on a pad or record one in SAMPLER" };
};

/** Display name for what a pad plays: sample name, plus the slice start for chopped pads. */
juce::String describeLayer (const Layer& l);

// ------------------------------------------------------------------ screens

class Screen : public juce::Component, public juce::ChangeListener
{
public:
    explicit Screen (AppContext& c) : ctx (c) { ctx.addChangeListener (this); }
    ~Screen() override { ctx.removeChangeListener (this); }

    virtual juce::String getTitle() const = 0;
    virtual void refresh() {}          // ~20 Hz while visible
    virtual void contextChanged() {}   // selected pad / track / project changed
    /** Fast-changing LCD elements (playhead, meters), drawn on a light overlay above the screen
        so they don't force the whole LCD to re-render. Rectangles are in this screen's coordinates. */
    struct LiveOverlay
    {
        struct Meter
        {
            juce::Rectangle<int> area;
            float level = 0.0f; // 0..1
            bool vertical = false;
            bool operator== (const Meter& o) const { return area == o.area && juce::approximatelyEqual (level, o.level) && vertical == o.vertical; }
        };
        juce::Rectangle<int> playhead;
        std::vector<Meter> meters;
    };
    virtual void getLiveOverlay (double /*ticks*/, bool /*playing*/, LiveOverlay&) {}
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (isShowing())
            contextChanged();
    }
    void visibilityChanged() override
    {
        if (isVisible())
            contextChanged();
    }

protected:
    AppContext& ctx;
};

std::unique_ptr<Screen> createMainScreen (AppContext&);
std::unique_ptr<Screen> createProgramScreen (AppContext&);
std::unique_ptr<Screen> createSampleScreen (AppContext&);
std::unique_ptr<Screen> createSamplerScreen (AppContext&);
std::unique_ptr<Screen> createMixerScreen (AppContext&);

} // namespace padlab
