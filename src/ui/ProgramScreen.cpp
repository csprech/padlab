#include "ui/Screens.h"
#include "core/SampleOps.h"

namespace padlab
{
namespace
{
class ProgramScreen : public Screen
{
public:
    explicit ProgramScreen (AppContext& c) : Screen (c)
    {
        addAndMakeVisible (title);
        title.setFont (theme::screenFont (18.0f, true));
        title.setColour (juce::Label::textColourId, theme::colours::accent);

        programName.setFont (theme::screenFont (14.0f));
        programName.setEditable (true);
        programName.setColour (juce::Label::backgroundColourId, theme::colours::screen.brighter (0.04f));
        programName.onTextChange = [this] {
            const auto n = programName.getText();
            edit ([n] (Program& p, Pad&) { p.name = n; });
            ctx.changed();
        };
        addAndMakeVisible (programName);
        addAndMakeVisible (capProgram);

        // ---- layers
        for (auto* cap : { &capLayerSample, &capVel, &capLTune, &capLLevel, &capLPan })
            addAndMakeVisible (cap);
        for (int i = 0; i < kNumLayers; ++i)
        {
            auto& L = layers[(size_t) i];
            L.index.setText (juce::String (i + 1), juce::dontSendNotification);
            L.index.setFont (theme::screenFont (14.0f, true));
            L.index.setColour (juce::Label::textColourId, theme::colours::textDim);
            addAndMakeVisible (L.index);

            L.sample.onChange = [this, i] { layerSampleChanged (i); };
            addAndMakeVisible (L.sample);
            L.load.onClick = [this, i] { loadIntoLayer (i); };
            addAndMakeVisible (L.load);

            setupParam (L.velStart, 0, 127, 1);
            setupParam (L.velEnd, 0, 127, 1);
            setupParam (L.tune, -36, 36, 0.01, " st");
            setupParam (L.level, -48, 12, 0.1, " dB");
            setupParam (L.pan, -1, 1, 0.01);
            L.velStart.onValueChange = [this, i] { edit ([i, v = (int) layers[(size_t) i].velStart.getValue()] (Program&, Pad& p) { p.layers[(size_t) i].velStart = v; }); };
            L.velEnd.onValueChange = [this, i] { edit ([i, v = (int) layers[(size_t) i].velEnd.getValue()] (Program&, Pad& p) { p.layers[(size_t) i].velEnd = v; }); };
            L.tune.onValueChange = [this, i] { edit ([i, v = (float) layers[(size_t) i].tune.getValue()] (Program&, Pad& p) { p.layers[(size_t) i].tuneSemis = v; }); };
            L.level.onValueChange = [this, i] { edit ([i, v = (float) layers[(size_t) i].level.getValue()] (Program&, Pad& p) { p.layers[(size_t) i].levelDb = v; }); };
            L.pan.onValueChange = [this, i] { edit ([i, v = (float) layers[(size_t) i].pan.getValue()] (Program&, Pad& p) { p.layers[(size_t) i].pan = v; }); };
            for (auto* s : { &L.velStart, &L.velEnd, &L.tune, &L.level, &L.pan })
                addAndMakeVisible (s);
        }

        // ---- pad params
        auto param = [this] (juce::Slider& s, Caption& cap, double mn, double mx, double step, const juce::String& suf,
                             double skew, std::function<void (Pad&, double)> apply) {
            setupParam (s, mn, mx, step, suf, skew);
            s.onValueChange = [this, &s, apply] { edit ([&s, apply] (Program&, Pad& p) { apply (p, s.getValue()); }); };
            addAndMakeVisible (s);
            addAndMakeVisible (cap);
        };
        param (level, capLevel, -48, 12, 0.1, " dB", 0, [] (Pad& p, double v) { p.levelDb = (float) v; });
        param (pan, capPan, -1, 1, 0.01, "", 0, [] (Pad& p, double v) { p.pan = (float) v; });
        param (tune, capTune, -36, 36, 0.01, " st", 0, [] (Pad& p, double v) { p.tuneSemis = (float) v; });
        param (attack, capAttack, 0, 2000, 0.1, " ms", 100, [] (Pad& p, double v) { p.attackMs = (float) v; });
        param (decay, capDecay, 1, 10000, 1, " ms", 400, [] (Pad& p, double v) { p.decayMs = (float) v; });
        param (sustain, capSustain, 0, 100, 1, " %", 0, [] (Pad& p, double v) { p.sustain = (float) v / 100.0f; });
        param (release, capRelease, 1, 5000, 1, " ms", 200, [] (Pad& p, double v) { p.releaseMs = (float) v; });
        param (cutoff, capCutoff, 20, 20000, 1, " Hz", 1000, [] (Pad& p, double v) { p.cutoffHz = (float) v; });
        param (reso, capReso, 0, 100, 1, " %", 0, [] (Pad& p, double v) { p.resonance = (float) v / 100.0f; });

        setupCombo (filter, { "Off", "Low pass", "High pass", "Band pass" });
        filter.onChange = [this] { edit ([v = filter.getSelectedId() - 1] (Program&, Pad& p) { p.filter = (FilterType) v; }); };
        juce::StringArray groups { "Off" };
        for (int i = 1; i <= 32; ++i)
            groups.add (juce::String (i));
        setupCombo (muteGroup, groups);
        muteGroup.onChange = [this] { edit ([v = muteGroup.getSelectedId() - 1] (Program&, Pad& p) { p.muteGroup = v; }); };
        setupCombo (trigger, { "One shot", "Note on" });
        trigger.onChange = [this] { edit ([v = trigger.getSelectedId() - 1] (Program&, Pad& p) { p.trigger = (TriggerMode) v; }); };
        setupCombo (poly, { "Poly", "Mono" });
        poly.onChange = [this] { edit ([v = poly.getSelectedId() - 1] (Program&, Pad& p) { p.poly = (PolyMode) v; }); };
        for (auto* c2 : { &filter, &muteGroup, &trigger, &poly })
            addAndMakeVisible (c2);
        for (auto* cap : { &capFilter, &capMuteGroup, &capTrigger, &capPoly })
            addAndMakeVisible (cap);

        audition.onClick = [this] { ctx.engine.audition (ctx.currentTrack(), ctx.selectedPad); };
        clearPad.onClick = [this] {
            SamplePtr olds[kNumLayers];
            edit ([&] (Program&, Pad& p) {
                for (int i = 0; i < kNumLayers; ++i)
                    olds[i] = p.layers[(size_t) i].sample;
                p = Pad();
            });
            for (auto& o : olds)
                ctx.engine.retire (o);
            ctx.changed();
        };
        addAndMakeVisible (audition);
        addAndMakeVisible (clearPad);
    }

    juce::String getTitle() const override { return "PROGRAM EDIT"; }

    void contextChanged() override
    {
        const juce::ScopedValueSetter<bool> svs (loading, true);
        const int prog = ctx.currentProgram();
        const int padIdx = ctx.selectedPad;
        juce::StringArray pool;
        ctx.engine.withProject ([&] (Project& p) {
            for (auto& s : p.samplePool)
                pool.add (s->name);
            const auto& pr = p.programs[(size_t) prog];
            const auto& pad = pr.pads[(size_t) padIdx];
            programName.setText (pr.name, juce::dontSendNotification);
            title.setText ("PAD " + AppContext::padName (padIdx), juce::dontSendNotification);
            for (int i = 0; i < kNumLayers; ++i)
            {
                auto& L = layers[(size_t) i];
                const auto& l = pad.layers[(size_t) i];
                setupCombo (L.sample, juce::StringArray ("(empty)"), 1);
                L.sample.addItemList (pool, 2);
                L.sample.setSelectedId (l.sample != nullptr ? pool.indexOf (l.sample->name) + 2 : 1, juce::dontSendNotification);
                L.velStart.setValue (l.velStart, juce::dontSendNotification);
                L.velEnd.setValue (l.velEnd, juce::dontSendNotification);
                L.tune.setValue (l.tuneSemis, juce::dontSendNotification);
                L.level.setValue (l.levelDb, juce::dontSendNotification);
                L.pan.setValue (l.pan, juce::dontSendNotification);
            }
            level.setValue (pad.levelDb, juce::dontSendNotification);
            pan.setValue (pad.pan, juce::dontSendNotification);
            tune.setValue (pad.tuneSemis, juce::dontSendNotification);
            attack.setValue (pad.attackMs, juce::dontSendNotification);
            decay.setValue (pad.decayMs, juce::dontSendNotification);
            sustain.setValue (pad.sustain * 100.0, juce::dontSendNotification);
            release.setValue (pad.releaseMs, juce::dontSendNotification);
            cutoff.setValue (pad.cutoffHz, juce::dontSendNotification);
            reso.setValue (pad.resonance * 100.0, juce::dontSendNotification);
            filter.setSelectedId ((int) pad.filter + 1, juce::dontSendNotification);
            muteGroup.setSelectedId (pad.muteGroup + 1, juce::dontSendNotification);
            trigger.setSelectedId ((int) pad.trigger + 1, juce::dontSendNotification);
            poly.setSelectedId ((int) pad.poly + 1, juce::dontSendNotification);
        });
        poolNames = pool;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto top = r.removeFromTop (34);
        title.setBounds (top.removeFromLeft (140));
        clearPad.setBounds (top.removeFromRight (90).reduced (0, 4));
        top.removeFromRight (6);
        audition.setBounds (top.removeFromRight (90).reduced (0, 4));
        top.removeFromRight (16);
        auto pn = top.removeFromRight (240);
        capProgram.setBounds (pn.removeFromLeft (70));
        programName.setBounds (pn.reduced (0, 5));

        r.removeFromTop (8);
        // layers table
        auto hdr = r.removeFromTop (16);
        const int wIdx = 24, wLoad = 54, wVel = 60, wTune = 80, wLvl = 80, wPan = 64;
        const int wSample = hdr.getWidth() - wIdx - wLoad - wVel * 2 - wTune - wLvl - wPan - 7 * 6;
        hdr.removeFromLeft (wIdx + 6);
        capLayerSample.setBounds (hdr.removeFromLeft (wSample + wLoad + 6));
        hdr.removeFromLeft (6);
        capVel.setBounds (hdr.removeFromLeft (wVel * 2 + 6));
        hdr.removeFromLeft (6);
        capLTune.setBounds (hdr.removeFromLeft (wTune));
        hdr.removeFromLeft (6);
        capLLevel.setBounds (hdr.removeFromLeft (wLvl));
        hdr.removeFromLeft (6);
        capLPan.setBounds (hdr.removeFromLeft (wPan));

        for (auto& L : layers)
        {
            auto row = r.removeFromTop (28).reduced (0, 2);
            auto take = [&row] (int w) { auto c = row.removeFromLeft (w); row.removeFromLeft (6); return c; };
            L.index.setBounds (take (wIdx));
            L.sample.setBounds (take (wSample));
            L.load.setBounds (take (wLoad));
            L.velStart.setBounds (take (wVel));
            L.velEnd.setBounds (take (wVel));
            L.tune.setBounds (take (wTune));
            L.level.setBounds (take (wLvl));
            L.pan.setBounds (take (wPan));
        }

        r.removeFromTop (14);
        // params in a grid: 4 columns
        const int colW = (r.getWidth() - 3 * 12) / 4;
        auto cell = [&] (int col, int row, Caption& cap, juce::Component& comp) {
            auto c = juce::Rectangle<int> (r.getX() + col * (colW + 12), r.getY() + row * 46, colW, 42);
            cap.setBounds (c.removeFromTop (16));
            comp.setBounds (c.withHeight (24));
        };
        cell (0, 0, capLevel, level);
        cell (1, 0, capPan, pan);
        cell (2, 0, capTune, tune);
        cell (3, 0, capPoly, poly);
        cell (0, 1, capAttack, attack);
        cell (1, 1, capDecay, decay);
        cell (2, 1, capSustain, sustain);
        cell (3, 1, capRelease, release);
        cell (0, 2, capFilter, filter);
        cell (1, 2, capCutoff, cutoff);
        cell (2, 2, capReso, reso);
        cell (3, 2, capTrigger, trigger);
        cell (0, 3, capMuteGroup, muteGroup);
    }

private:
    template <typename Fn>
    void edit (Fn&& fn)
    {
        if (loading)
            return;
        const int prog = ctx.currentProgram();
        const int pad = ctx.selectedPad;
        ctx.engine.withProject ([&] (Project& p) {
            auto& pr = p.programs[(size_t) prog];
            fn (pr, pr.pads[(size_t) pad]);
        });
    }

    void layerSampleChanged (int i)
    {
        if (loading)
            return;
        const int id = layers[(size_t) i].sample.getSelectedId();
        const auto name = id >= 2 ? poolNames[id - 2] : juce::String();
        const int prog = ctx.currentProgram();
        const int pad = ctx.selectedPad;
        ctx.engine.withProject ([&] (Project& p) {
            auto& l = p.programs[(size_t) prog].pads[(size_t) pad].layers[(size_t) i];
            l.sample = name.isNotEmpty() ? p.findSample (name) : nullptr; // pool keeps the old one alive
            l.sliceStart = 0;
            l.sliceEnd = -1;
        });
        ctx.changed();
    }

    void loadIntoLayer (int i)
    {
        chooser = std::make_unique<juce::FileChooser> ("Load sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, i] (const juce::FileChooser& fc) {
                                  auto f = fc.getResult();
                                  if (! f.existsAsFile())
                                      return;
                                  auto s = sampleops::loadFile (f);
                                  if (s == nullptr)
                                      return;
                                  ctx.engine.withProject ([&] (Project& p) {
                                      s->name = p.makeUniqueSampleName (s->name);
                                      p.addSample (s);
                                  });
                                  edit ([&] (Program&, Pad& p) {
                                      p.layers[(size_t) i].sample = s;
                                      p.layers[(size_t) i].sliceStart = 0;
                                      p.layers[(size_t) i].sliceEnd = -1;
                                  });
                                  ctx.changed();
                              });
    }

    struct LayerRow
    {
        juce::Label index;
        juce::ComboBox sample;
        juce::TextButton load { "Load" };
        juce::Slider velStart, velEnd, tune, level, pan;
    };

    juce::Label title, programName;
    Caption capProgram { "Program" };
    std::array<LayerRow, kNumLayers> layers;
    Caption capLayerSample { "Layer sample" }, capVel { "Velocity range" }, capLTune { "Tune" }, capLLevel { "Level" },
        capLPan { "Pan" };
    juce::Slider level, pan, tune, attack, decay, sustain, release, cutoff, reso;
    Caption capLevel { "Pad level" }, capPan { "Pad pan" }, capTune { "Pad tune" }, capAttack { "Attack" },
        capDecay { "Decay" }, capSustain { "Sustain" }, capRelease { "Release" }, capCutoff { "Cutoff" },
        capReso { "Resonance" }, capFilter { "Filter" }, capMuteGroup { "Mute group (choke)" },
        capTrigger { "Trigger" }, capPoly { "Polyphony" };
    juce::ComboBox filter, muteGroup, trigger, poly;
    juce::TextButton audition { "Audition" }, clearPad { "Clear pad" };
    juce::StringArray poolNames;
    std::unique_ptr<juce::FileChooser> chooser;
    bool loading = false;
};
} // namespace

std::unique_ptr<Screen> createProgramScreen (AppContext& c) { return std::make_unique<ProgramScreen> (c); }

} // namespace padlab
