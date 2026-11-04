#include "ui/Screens.h"

namespace padlab
{
namespace
{
class LevelMeter : public juce::Component
{
public:
    void setLevels (float l, float r)
    {
        levels[0] = juce::jmax (l, levels[0] * 0.85f);
        levels[1] = juce::jmax (r, levels[1] * 0.85f);
        const int sig = (int) (levels[0] * 60.0f) * 100 + (int) (levels[1] * 60.0f);
        if (sig != lastSig)
        {
            lastSig = sig;
            repaint();
        }
    }
    void setThreshold (float gain) { threshold = gain; }
    void paint (juce::Graphics& g) override
    {
        using namespace theme::lcd;
        auto r = getLocalBounds().toFloat();
        for (int c = 0; c < 2; ++c)
        {
            auto bar = r.removeFromTop (r.getHeight() / (2 - c)).reduced (0, 2);
            g.setColour (text.withAlpha (0.08f));
            g.fillRect (bar);
            const float db = juce::Decibels::gainToDecibels (levels[(size_t) c], -60.0f);
            const float frac = juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f);
            g.setColour (db > -3.0f ? record : db > -12.0f ? accent : play);
            g.fillRect (bar.withWidth (bar.getWidth() * frac));
            if (threshold > 0.0f)
            {
                const float tf = juce::jmap (juce::Decibels::gainToDecibels (threshold, -60.0f), -60.0f, 0.0f, 0.0f, 1.0f);
                g.setColour (text);
                g.drawVerticalLine ((int) (bar.getX() + bar.getWidth() * tf), bar.getY(), bar.getBottom());
            }
        }
    }

private:
    std::array<float, 2> levels {};
    float threshold = 0.0f;
    int lastSig = -1;
};

class SamplerScreen : public Screen
{
public:
    explicit SamplerScreen (AppContext& c) : Screen (c)
    {
        addAndMakeVisible (status);
        status.setFont (theme::screenFont (22.0f, true));
        status.setColour (juce::Label::textColourId, theme::lcd::text);

        setupCombo (source, { "Audio input 1/2", "Resample (master out)" });
        source.setSelectedId (1, juce::dontSendNotification);
        source.onChange = [this] { updateInputButton(); };
        setupParam (threshold, -61, 0, 1);
        threshold.setValue (-30, juce::dontSendNotification);
        threshold.textFromValueFunction = [] (double v) { return v <= -61 ? juce::String ("Off") : juce::String ((int) v) + " dB"; };
        threshold.updateText();
        setupParam (maxLen, 1, 120, 1, " s");
        maxLen.setValue (20, juce::dontSendNotification);

        inputBtn.onClick = [this] {
            ctx.engine.setInputEnabled (! ctx.engine.isInputEnabled());
            updateInputButton();
        };
        armBtn.onClick = [this] {
            const auto st = ctx.engine.getSamplingState();
            if (st == Engine::RecState::Armed || st == Engine::RecState::Recording)
                ctx.engine.stopSampling();
            else
            {
                if (source.getSelectedId() == 1 && ! ctx.engine.isInputEnabled())
                    ctx.engine.setInputEnabled (true);
                ctx.engine.armSampling (source.getSelectedId() == 2, (float) threshold.getValue(), maxLen.getValue());
            }
        };
        keepBtn.onClick = [this] { keep (false); };
        keepChopBtn.onClick = [this] { keep (true); };
        discardBtn.onClick = [this] {
            pending.reset();
            wave.setSample (nullptr);
            refreshButtons();
        };
        name.setText ("Sample", juce::dontSendNotification);
        wave.setEmptyText ("Nothing recorded yet - press ARM and make some noise");

        for (auto* comp : std::initializer_list<juce::Component*> { &source, &threshold, &maxLen, &inputBtn, &armBtn, &meter,
                                                                    &wave, &keepBtn, &keepChopBtn, &discardBtn, &name,
                                                                    &capSource, &capThreshold, &capMax, &capName, &capMeter,
                                                                    &help })
            addAndMakeVisible (comp);
        help.setFont (theme::screenFont (12.0f));
        help.setColour (juce::Label::textColourId, theme::lcd::textDim);
        help.setText ("ARM waits for the signal to cross the threshold, then records until you press STOP or the max length. "
                      "Resample records Padlab's own output - play pads or the sequence while armed.",
                      juce::dontSendNotification);
        help.setJustificationType (juce::Justification::topLeft);
        updateInputButton();
        refreshButtons();
    }

    juce::String getHelpTopic() const override { return "Sampling"; }
    juce::String getTitle() const override { return "SAMPLER"; }

    void contextChanged() override
    {
        keepBtn.setButtonText ("Keep -> pad " + AppContext::padName (ctx.selectedPad));
    }

    void refresh() override
    {
        meter.setLevels (ctx.engine.getInputPeak (0), ctx.engine.getInputPeak (1));
        meter.setThreshold (threshold.getValue() <= -61 ? 0.0f : juce::Decibels::decibelsToGain ((float) threshold.getValue()));
        const auto st = ctx.engine.getSamplingState();
        juce::String text;
        switch (st)
        {
            case Engine::RecState::Idle: text = pending ? "RECORDED " + juce::String (pending->getLengthSeconds(), 2) + " s" : "READY"; break;
            case Engine::RecState::Armed: text = "ARMED - waiting for signal"; break;
            case Engine::RecState::Recording: text = "RECORDING  " + juce::String (ctx.engine.getSampledSeconds(), 1) + " s"; break;
            case Engine::RecState::Done: break;
        }
        if (st == Engine::RecState::Done)
        {
            pending = ctx.engine.takeSampledAudio (name.getText());
            wave.setSample (pending);
            text = "RECORDED " + juce::String (pending != nullptr ? pending->getLengthSeconds() : 0.0, 2) + " s";
        }
        status.setText (text, juce::dontSendNotification);
        status.setColour (juce::Label::textColourId, st == Engine::RecState::Recording ? theme::lcd::record
                                                     : st == Engine::RecState::Armed ? theme::lcd::accent
                                                                                      : theme::lcd::text);
        if (st != lastState)
            refreshButtons();
        lastState = st;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        status.setBounds (r.removeFromTop (36));
        r.removeFromTop (6);
        auto row = r.removeFromTop (44);
        auto place = [] (juce::Rectangle<int>& rr, Caption& cap, juce::Component& comp, int w) {
            auto cell = rr.removeFromLeft (w);
            rr.removeFromLeft (10);
            cap.setBounds (cell.removeFromTop (16));
            comp.setBounds (cell.withHeight (26));
        };
        place (row, capSource, source, 190);
        place (row, capThreshold, threshold, 120);
        place (row, capMax, maxLen, 90);
        row.removeFromTop (16);
        inputBtn.setBounds (row.removeFromLeft (120).withHeight (26));
        row.removeFromLeft (10);
        armBtn.setBounds (row.removeFromLeft (110).withHeight (26));

        r.removeFromTop (8);
        capMeter.setBounds (r.removeFromTop (16));
        meter.setBounds (r.removeFromTop (30));
        r.removeFromTop (10);

        auto bottom = r.removeFromBottom (86);
        help.setBounds (bottom.removeFromBottom (36));
        auto row2 = bottom.removeFromTop (44);
        place (row2, capName, name, 180);
        row2.removeFromTop (16);
        keepBtn.setBounds (row2.removeFromLeft (150).withHeight (26));
        row2.removeFromLeft (8);
        keepChopBtn.setBounds (row2.removeFromLeft (140).withHeight (26));
        row2.removeFromLeft (8);
        discardBtn.setBounds (row2.removeFromLeft (90).withHeight (26));
        wave.setBounds (r);
    }

private:
    void updateInputButton()
    {
        inputBtn.setButtonText (ctx.engine.isInputEnabled() ? "Input: ON" : "Input: OFF");
        inputBtn.setToggleState (ctx.engine.isInputEnabled(), juce::dontSendNotification);
    }

    void refreshButtons()
    {
        const auto st = ctx.engine.getSamplingState();
        armBtn.setButtonText (st == Engine::RecState::Armed || st == Engine::RecState::Recording ? "STOP" : "ARM");
        armBtn.setToggleState (st != Engine::RecState::Idle, juce::dontSendNotification);
        keepBtn.setEnabled (pending != nullptr);
        keepChopBtn.setEnabled (pending != nullptr);
        discardBtn.setEnabled (pending != nullptr);
    }

    void keep (bool thenChop)
    {
        if (pending == nullptr)
            return;
        const int prog = ctx.currentProgram();
        const int pad = ctx.selectedPad;
        auto s = pending;
        ctx.engine.withProject ([&] (Project& p) {
            s->name = p.makeUniqueSampleName (name.getText().trim());
            p.addSample (s);
            auto& l = p.programs[(size_t) prog].pads[(size_t) pad].layers[0];
            l.sample = s;
            l.sliceStart = 0;
            l.sliceEnd = -1;
        });
        pending.reset();
        wave.setSample (nullptr);
        refreshButtons();
        ctx.changed();
        if (ctx.flashMessage)
            ctx.flashMessage (s->name + " -> " + AppContext::padName (pad));
        if (thenChop && ctx.showScreen)
            ctx.showScreen (ScreenMode::SampleEdit);
    }

    juce::Label status, help;
    juce::ComboBox source;
    juce::Slider threshold, maxLen;
    juce::TextButton inputBtn, armBtn { "ARM" }, keepBtn { "Keep" }, keepChopBtn { "Keep + chop" }, discardBtn { "Discard" };
    juce::TextEditor name;
    Caption capSource { "Source" }, capThreshold { "Threshold" }, capMax { "Max length" }, capName { "Name" },
        capMeter { "Input level" };
    LevelMeter meter;
    Waveform wave;
    SamplePtr pending;
    Engine::RecState lastState = Engine::RecState::Idle;
};
} // namespace

std::unique_ptr<Screen> createSamplerScreen (AppContext& c) { return std::make_unique<SamplerScreen> (c); }

} // namespace padlab
