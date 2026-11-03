#include "ui/Screens.h"

namespace padlab
{
namespace
{
class Strip : public juce::Component
{
public:
    Strip()
    {
        fader.setSliderStyle (juce::Slider::LinearVertical);
        fader.setRange (-60.0, 6.0, 0.1);
        fader.setSkewFactorFromMidPoint (-12.0);
        fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
        fader.setTextValueSuffix (" dB");
        fader.setDoubleClickReturnValue (true, 0.0);
        pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        pan.setRange (-1.0, 1.0, 0.01);
        pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        pan.setDoubleClickReturnValue (true, 0.0);
        pan.setColour (juce::Slider::trackColourId, theme::colours::accent2);
        name.setFont (theme::screenFont (11.0f, true));
        name.setJustificationType (juce::Justification::centred);
        mute.setClickingTogglesState (true);
        solo.setClickingTogglesState (true);
        solo.setColour (juce::TextButton::buttonOnColourId, theme::colours::accent2);
        for (auto* c : std::initializer_list<juce::Component*> { &fader, &pan, &name, &mute, &solo })
            addAndMakeVisible (c);
    }
    void setShowSolo (bool b) { solo.setVisible (b); resized(); }
    void setLevel (float peak) { meter = juce::jmax (peak, meter * 0.82f); repaint(); }

    void paint (juce::Graphics& g) override
    {
        using namespace theme::colours;
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (screen.brighter (0.03f));
        g.fillRoundedRectangle (r, 4.0f);
        if (showMeter)
        {
            auto m = fader.getBounds().toFloat().withTrimmedBottom (20.0f).removeFromRight (5.0f).translated (4.0f, 0.0f);
            g.setColour (screen.darker (0.4f));
            g.fillRect (m);
            const float db = juce::Decibels::gainToDecibels (meter, -60.0f);
            const float frac = juce::jmap (db, -60.0f, 6.0f, 0.0f, 1.0f);
            g.setColour (db > -3.0f ? record : play);
            g.fillRect (m.withTop (m.getBottom() - m.getHeight() * frac));
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        name.setBounds (r.removeFromTop (18));
        pan.setBounds (r.removeFromTop (40).withSizeKeepingCentre (40, 40));
        auto buttons = r.removeFromBottom (22);
        if (solo.isVisible())
        {
            mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1, 0));
            solo.setBounds (buttons.reduced (1, 0));
        }
        else
            mute.setBounds (buttons);
        r.removeFromBottom (4);
        fader.setBounds (r.withTrimmedRight (showMeter ? 8 : 0));
    }

    juce::Slider fader, pan;
    juce::Label name;
    juce::TextButton mute { "M" }, solo { "S" };
    bool showMeter = false;

private:
    float meter = 0.0f;
};

class MixerScreen : public Screen
{
public:
    explicit MixerScreen (AppContext& c) : Screen (c)
    {
        padsTab.setClickingTogglesState (true);
        tracksTab.setClickingTogglesState (true);
        padsTab.setRadioGroupId (77);
        tracksTab.setRadioGroupId (77);
        tracksTab.setToggleState (true, juce::dontSendNotification);
        padsTab.onClick = tracksTab.onClick = [this] { contextChanged(); resized(); };
        addAndMakeVisible (padsTab);
        addAndMakeVisible (tracksTab);
        addAndMakeVisible (title);
        title.setFont (theme::screenFont (12.0f));
        title.setColour (juce::Label::textColourId, theme::colours::textDim);

        for (int i = 0; i < kPadsPerBank; ++i)
        {
            auto& s = strips[(size_t) i];
            addChildComponent (s);
            s.fader.onValueChange = [this, i] { stripChanged (i); };
            s.pan.onValueChange = [this, i] { stripChanged (i); };
            s.mute.onClick = [this, i] { stripChanged (i); };
            s.solo.onClick = [this, i] { stripChanged (i); };
        }
        addAndMakeVisible (master);
        master.name.setText ("MASTER", juce::dontSendNotification);
        master.pan.setVisible (false);
        master.mute.setVisible (false);
        master.solo.setVisible (false);
        master.showMeter = true;
        master.fader.onValueChange = [this] {
            if (! loading)
                ctx.engine.withProject ([v = (float) master.fader.getValue()] (Project& p) { p.masterLevelDb = v; });
        };
    }

    juce::String getTitle() const override { return "MIXER"; }

    void contextChanged() override
    {
        const juce::ScopedValueSetter<bool> svs (loading, true);
        const bool pads = padsTab.getToggleState();
        const int seqIdx = ctx.engine.getCurrentSequence();
        const int prog = ctx.currentProgram();
        const int bank = ctx.engine.getCurrentBank();
        ctx.engine.withProject ([&] (Project& p) {
            const auto& seq = p.sequences[(size_t) seqIdx];
            master.fader.setValue (p.masterLevelDb, juce::dontSendNotification);
            for (int i = 0; i < kPadsPerBank; ++i)
            {
                auto& s = strips[(size_t) i];
                const bool vis = pads || i < kNumTracks;
                s.setVisible (vis);
                if (! vis)
                    continue;
                s.setShowSolo (! pads);
                s.showMeter = ! pads;
                if (pads)
                {
                    const auto& pd = p.programs[(size_t) prog].pads[(size_t) (bank * kPadsPerBank + i)];
                    juce::String n = AppContext::padName (bank * kPadsPerBank + i);
                    s.name.setText (n, juce::dontSendNotification);
                    s.name.setTooltip (pd.layers[0].sample != nullptr ? pd.layers[0].sample->name : juce::String());
                    s.fader.setValue (pd.levelDb, juce::dontSendNotification);
                    s.pan.setValue (pd.pan, juce::dontSendNotification);
                    s.mute.setToggleState (pd.mute, juce::dontSendNotification);
                    s.setAlpha (pd.hasSample() ? 1.0f : 0.45f);
                }
                else
                {
                    const auto& tr = seq.tracks[(size_t) i];
                    const auto& pr = p.programs[(size_t) tr.programIndex];
                    s.name.setText (juce::String (i + 1) + " " + tr.name, juce::dontSendNotification);
                    s.name.setTooltip (pr.name);
                    s.fader.setValue (pr.levelDb, juce::dontSendNotification);
                    s.pan.setValue (pr.pan, juce::dontSendNotification);
                    s.mute.setToggleState (tr.mute, juce::dontSendNotification);
                    s.solo.setToggleState (tr.solo, juce::dontSendNotification);
                    s.setAlpha (1.0f);
                }
            }
            title.setText (pads ? "Pad mixer - " + p.programs[(size_t) prog].name + "  bank " + juce::String::charToString ((juce::juce_wchar) ('A' + bank))
                                : "Track mixer - level/pan live on each track's program",
                           juce::dontSendNotification);
        });
        resized();
    }

    void refresh() override
    {
        if (! padsTab.getToggleState())
            for (int t = 0; t < kNumTracks; ++t)
                strips[(size_t) t].setLevel (ctx.engine.getTrackPeak (t));
        master.setLevel (juce::jmax (ctx.engine.getMasterPeak (0), ctx.engine.getMasterPeak (1)));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto top = r.removeFromTop (26);
        tracksTab.setBounds (top.removeFromLeft (90));
        top.removeFromLeft (4);
        padsTab.setBounds (top.removeFromLeft (90));
        top.removeFromLeft (12);
        title.setBounds (top);
        r.removeFromTop (6);
        master.setBounds (r.removeFromRight (78));
        r.removeFromRight (8);
        const int n = padsTab.getToggleState() ? kPadsPerBank : kNumTracks;
        const int w = r.getWidth() / n;
        for (int i = 0; i < n; ++i)
            strips[(size_t) i].setBounds (r.getX() + i * w, r.getY(), w, r.getHeight());
    }

private:
    void stripChanged (int i)
    {
        if (loading)
            return;
        auto& s = strips[(size_t) i];
        const bool pads = padsTab.getToggleState();
        const int seqIdx = ctx.engine.getCurrentSequence();
        const int prog = ctx.currentProgram();
        const int bank = ctx.engine.getCurrentBank();
        const float lvl = (float) s.fader.getValue(), pn = (float) s.pan.getValue();
        const bool m = s.mute.getToggleState(), so = s.solo.getToggleState();
        ctx.engine.withProject ([&] (Project& p) {
            if (pads)
            {
                auto& pd = p.programs[(size_t) prog].pads[(size_t) (bank * kPadsPerBank + i)];
                pd.levelDb = lvl;
                pd.pan = pn;
                pd.mute = m;
            }
            else
            {
                auto& tr = p.sequences[(size_t) seqIdx].tracks[(size_t) i];
                auto& pr = p.programs[(size_t) tr.programIndex];
                pr.levelDb = lvl;
                pr.pan = pn;
                tr.mute = m;
                tr.solo = so;
            }
        });
    }

    juce::TextButton padsTab { "PADS" }, tracksTab { "TRACKS" };
    juce::Label title;
    std::array<Strip, kPadsPerBank> strips;
    Strip master;
    bool loading = false;
};
} // namespace

std::unique_ptr<Screen> createMixerScreen (AppContext& c) { return std::make_unique<MixerScreen> (c); }

} // namespace padlab
