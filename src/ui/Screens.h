#pragma once

#include "AppContext.h"
#include "Theme.h"

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

/** Draws a sample with slice markers, a highlighted region and an optional playhead.
    Zoom: the scroll wheel (or a trackpad pinch) zooms around the pointer, shift-wheel or a sideways
    swipe pans, and the strip along the bottom shows where you are (drag it to move). */
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

    // ---- zoom
    /** Shows frames start .. start + length (clamped); length <= 0 or the whole sample = not zoomed. */
    void setView (int start, int length);
    void showAll() { setView (0, 0); }
    bool isZoomed() const { return viewLength > 0; }
    int getViewStart() const { return viewLength > 0 ? viewStart : 0; }
    int getViewLength() const;
    /** Zooms by `factor` (> 1 = in) keeping the frame under x where it is. */
    void zoomAround (int x, double factor);
    /** If zoomed and `frame` is off screen, scrolls so it's in view. */
    void reveal (int frame);

    void setEmptyText (const juce::String& t) { emptyText = t; repaint(); }

    std::function<void (int frame, const juce::ModifierKeys&)> onClick;
    std::function<void (int frame)> onDoubleClick;
    /** A marker line was dragged (index >= 1; the first marker stays at 0). */
    std::function<void (int markerIndex, int frame)> onMarkerDragged;
    std::function<void()> onMarkerDragEnd;

    void paint (juce::Graphics&) override;
    void resized() override { rebuild(); }
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify (const juce::MouseEvent& e, float scaleFactor) override;

private:
    void rebuild();
    juce::Rectangle<int> overviewBounds() const; // the position strip along the bottom, while zoomed
    void panOverviewTo (int x);
    SamplePtr sample;
    std::vector<int> markers;
    std::vector<std::pair<float, float>> peaks; // per pixel min / max (zoomed out)
    std::vector<float> points;                  // per frame (zoomed in past one frame per pixel)
    int markerNear (int x) const; // index of a draggable marker within a few pixels, or -1
    int regionStart = -1, regionEnd = -1, selectedSlice = -1, playhead = -1, dragging = -1;
    int viewStart = 0, viewLength = 0; // viewLength 0 = the whole sample
    bool draggingOverview = false;
    int overviewGrab = 0; // where in the visible window the overview strip was grabbed (frames)
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
    /** Screen-specific hotkeys, offered before the global ones (never pad keys). */
    virtual bool handleKey (const juce::KeyPress&) { return false; }
    /** Which HELP topic to open when HELP is pressed on this screen. */
    virtual juce::String getHelpTopic() const { return "Quick start"; }
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
std::unique_ptr<Screen> createSongScreen (AppContext&);
std::unique_ptr<Screen> createStepEditScreen (AppContext&);
std::unique_ptr<Screen> createOtherScreen (AppContext&);
std::unique_ptr<Screen> createHelpScreen (AppContext&);
std::unique_ptr<Screen> createEditScreen (AppContext&);
void helpOpenTopic (Screen& help, const juce::String& topic);
/** Panel STEP < > while STEP EDIT is showing. */
void stepEditStep (Screen&, int direction);

} // namespace padlab
