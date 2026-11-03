#include "ui/Screens.h"
#include "core/Chop.h"
#include "core/SampleOps.h"

namespace padlab
{
namespace
{
class SampleScreen : public Screen
{
public:
    explicit SampleScreen (AppContext& c) : Screen (c)
    {
        addAndMakeVisible (title);
        title.setFont (theme::screenFont (16.0f, true));
        title.setColour (juce::Label::textColourId, theme::colours::accent);
        addAndMakeVisible (info);
        info.setFont (theme::screenFont (12.0f));
        info.setColour (juce::Label::textColourId, theme::colours::textDim);
        info.setJustificationType (juce::Justification::centredRight);

        addAndMakeVisible (wave);
        wave.onClick = [this] (int frame, const juce::ModifierKeys& mods) { waveClicked (frame, mods); };

        setupCombo (mode, { "Threshold", "Regions", "BPM", "Manual" });
        mode.setSelectedId (1, juce::dontSendNotification);
        mode.onChange = [this] { updateVisibility(); };
        setupParam (sensitivity, 0, 100, 1, " %");
        sensitivity.setValue (55, juce::dontSendNotification);
        setupParam (minTime, 10, 1000, 1, " ms", 120);
        minTime.setValue (60, juce::dontSendNotification);
        setupParam (regions, 2, 64, 1);
        regions.setValue (16, juce::dontSendNotification);
        setupParam (bpm, 40, 240, 0.1, " BPM");
        bpm.setValue (90, juce::dontSendNotification);
        setupCombo (beats, { "1/4 beat", "1/2 beat", "1 beat", "2 beats", "1 bar" });
        beats.setSelectedId (3, juce::dontSendNotification);
        for (auto* comp : std::initializer_list<juce::Component*> { &mode, &sensitivity, &minTime, &regions, &bpm, &beats,
                                                                    &capMode, &capSens, &capMin, &capRegions, &capBpm, &capBeats })
            addAndMakeVisible (comp);

        chopBtn.onClick = [this] { doChop(); };
        clearBtn.onClick = [this] { markers = { 0 }; selected = -1; syncWave(); };
        previewBtn.onClick = [this] {
            if (auto s = wave.getSample())
                ctx.engine.previewSample (s, 0, -1);
        };
        stopBtn.onClick = [this] { ctx.engine.stopPreview(); };
        normalizeBtn.onClick = [this] { destructive ("normalized", [] (Sample& s) { sampleops::normalize (s); }); };
        reverseBtn.onClick = [this] { destructive ("reversed", [] (Sample& s) { sampleops::reverse (s); }); };
        trimBtn.onClick = [this] { trimToSlice(); };
        setSliceBtn.onClick = [this] { applySliceToPad(); };

        juce::StringArray progs;
        for (int i = 0; i < kNumTracks; ++i)
            progs.add ("Program " + juce::String (i + 1));
        setupCombo (target, progs);
        target.setSelectedId (2, juce::dontSendNotification);
        makeSeq.setButtonText ("Create sequence");
        makeSeq.setToggleState (true, juce::dontSendNotification);
        fitTempo.setButtonText ("Set tempo from loop");
        fitTempo.setToggleState (true, juce::dontSendNotification);
        setupCombo (loopBars, { "1 bar", "2 bars", "4 bars", "8 bars" });
        loopBars.setSelectedId (2, juce::dontSendNotification);
        convertBtn.onClick = [this] { convert(); };

        for (auto* comp : std::initializer_list<juce::Component*> { &chopBtn, &clearBtn, &previewBtn, &stopBtn, &normalizeBtn,
                                                                    &reverseBtn, &trimBtn, &setSliceBtn, &target, &makeSeq,
                                                                    &fitTempo, &loopBars, &convertBtn, &capTarget, &capLoop,
                                                                    &hint })
            addAndMakeVisible (comp);
        hint.setFont (theme::labelFont (11.5f));
        hint.setColour (juce::Label::textColourId, theme::colours::textDim);
        hint.setText ("Click a slice to hear it.  Manual mode: click adds a marker, Cmd-click removes one.",
                      juce::dontSendNotification);
        updateVisibility();
    }

    juce::String getTitle() const override { return "SAMPLE EDIT"; }

    void contextChanged() override
    {
        const int prog = ctx.currentProgram();
        const int pad = ctx.selectedPad;
        SamplePtr s;
        int a = 0, b = -1;
        ctx.engine.withProject ([&] (Project& p) {
            const auto& l = p.programs[(size_t) prog].pads[(size_t) pad].layers[0];
            s = l.sample;
            a = l.sliceStart;
            b = l.sliceEnd;
        });
        if (s != wave.getSample())
        {
            markers = { 0 };
            selected = -1;
            if (s != nullptr)
            {
                const double est = chop::estimateLoopBpm (*s, loopBars.getSelectedId() == 1 ? 1 : loopBars.getSelectedId() == 2 ? 2 : loopBars.getSelectedId() == 3 ? 4 : 8);
                if (est > 0)
                    bpm.setValue (est, juce::dontSendNotification);
            }
        }
        wave.setSample (s);
        wave.setRegion (s != nullptr && (a > 0 || b >= 0) ? a : -1, b);
        title.setText ("PAD " + AppContext::padName (pad) + (s != nullptr ? "  " + s->name : juce::String()),
                       juce::dontSendNotification);
        if (s != nullptr)
            info.setText (juce::String (s->getLengthSeconds(), 3) + " s   " + juce::String (s->sampleRate / 1000.0, 1)
                              + " kHz   " + (s->buffer.getNumChannels() > 1 ? "stereo" : "mono") + "   "
                              + juce::String ((int) markers.size()) + " slices",
                          juce::dontSendNotification);
        else
            info.setText ({}, juce::dontSendNotification);
        syncWave();
    }

    void refresh() override { wave.setPlayhead (ctx.engine.getPreviewPosition()); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto top = r.removeFromTop (26);
        title.setBounds (top.removeFromLeft (top.getWidth() / 2));
        info.setBounds (top);
        r.removeFromTop (4);
        hint.setBounds (r.removeFromBottom (18));
        r.removeFromBottom (4);

        auto controls = r.removeFromBottom (150);
        wave.setBounds (r.reduced (0, 4));

        auto row1 = controls.removeFromTop (44);
        auto place = [] (juce::Rectangle<int>& row, Caption& cap, juce::Component& comp, int w) {
            auto cell = row.removeFromLeft (w);
            row.removeFromLeft (8);
            cap.setBounds (cell.removeFromTop (16));
            comp.setBounds (cell.withHeight (24));
        };
        place (row1, capMode, mode, 110);
        place (row1, capSens, sensitivity, 110);
        place (row1, capMin, minTime, 100);
        place (row1, capRegions, regions, 80);
        place (row1, capBpm, bpm, 100);
        place (row1, capBeats, beats, 90);
        row1.removeFromTop (16);
        chopBtn.setBounds (row1.removeFromLeft (70).withHeight (24));
        row1.removeFromLeft (6);
        clearBtn.setBounds (row1.removeFromLeft (60).withHeight (24));

        controls.removeFromTop (8);
        auto row2 = controls.removeFromTop (28);
        for (auto* b : { &previewBtn, &stopBtn, &normalizeBtn, &reverseBtn, &trimBtn, &setSliceBtn })
        {
            b->setBounds (row2.removeFromLeft (b == &setSliceBtn ? 130 : 92));
            row2.removeFromLeft (6);
        }

        controls.removeFromTop (8);
        auto row3 = controls.removeFromTop (44);
        place (row3, capTarget, target, 120);
        place (row3, capLoop, loopBars, 90);
        row3.removeFromTop (16);
        makeSeq.setBounds (row3.removeFromLeft (140).withHeight (24));
        fitTempo.setBounds (row3.removeFromLeft (160).withHeight (24));
        convertBtn.setBounds (row3.removeFromLeft (180).withHeight (26));
    }

private:
    void updateVisibility()
    {
        const int m = mode.getSelectedId();
        sensitivity.setEnabled (m == 1);
        minTime.setEnabled (m == 1);
        regions.setEnabled (m == 2);
        bpm.setEnabled (m == 3);
        beats.setEnabled (m == 3);
        chopBtn.setEnabled (m != 4);
    }

    void syncWave()
    {
        wave.setMarkers (markers);
        wave.setSelectedSlice (selected);
    }

    void doChop()
    {
        auto s = wave.getSample();
        if (s == nullptr)
            return;
        switch (mode.getSelectedId())
        {
            case 1: markers = chop::detectTransients (*s, sensitivity.getValue(), minTime.getValue()); break;
            case 2: markers = chop::equalRegions (*s, (int) regions.getValue()); break;
            case 3:
            {
                static const double beatsPer[] = { 0.25, 0.5, 1.0, 2.0, 4.0 };
                markers = chop::byBpm (*s, bpm.getValue(), beatsPer[beats.getSelectedId() - 1]);
                break;
            }
            default: break;
        }
        if (markers.size() > (size_t) kNumPads)
            markers.resize ((size_t) kNumPads);
        selected = -1;
        syncWave();
        contextChanged();
        if (ctx.flashMessage)
            ctx.flashMessage (juce::String ((int) markers.size()) + " slices");
    }

    int sliceAt (int frame) const
    {
        for (int i = (int) markers.size() - 1; i >= 0; --i)
            if (frame >= markers[(size_t) i])
                return i;
        return 0;
    }
    int sliceEnd (int i) const
    {
        auto s = wave.getSample();
        return i + 1 < (int) markers.size() ? markers[(size_t) i + 1] : (s != nullptr ? s->getNumFrames() : 0);
    }

    void waveClicked (int frame, const juce::ModifierKeys& mods)
    {
        if (mode.getSelectedId() == 4 || mods.isAltDown())
        {
            if (mods.isCommandDown())
            {
                // remove nearest marker (never the first)
                int best = -1, bestDist = std::numeric_limits<int>::max();
                for (int i = 1; i < (int) markers.size(); ++i)
                    if (std::abs (markers[(size_t) i] - frame) < bestDist)
                    {
                        bestDist = std::abs (markers[(size_t) i] - frame);
                        best = i;
                    }
                if (best > 0)
                    markers.erase (markers.begin() + best);
            }
            else if (markers.size() < (size_t) kNumPads)
            {
                markers.insert (std::upper_bound (markers.begin(), markers.end(), frame), frame);
                markers.erase (std::unique (markers.begin(), markers.end()), markers.end());
            }
            selected = -1;
            syncWave();
            contextChanged();
            return;
        }
        selected = sliceAt (frame);
        syncWave();
        if (auto s = wave.getSample())
            ctx.engine.previewSample (s, markers[(size_t) selected], sliceEnd (selected));
    }

    template <typename Fn>
    void destructive (const juce::String& what, Fn&& fn)
    {
        auto src = wave.getSample();
        if (src == nullptr)
            return;
        auto copy = sampleops::extract (*src, 0, -1, src->name);
        fn (*copy);
        replaceOnPad (copy, src, what);
    }

    void trimToSlice()
    {
        auto src = wave.getSample();
        if (src == nullptr || selected < 0)
        {
            if (ctx.flashMessage)
                ctx.flashMessage ("Click a slice first");
            return;
        }
        auto copy = sampleops::extract (*src, markers[(size_t) selected], sliceEnd (selected), src->name);
        markers = { 0 };
        selected = -1;
        replaceOnPad (copy, src, "trimmed");
    }

    void replaceOnPad (SamplePtr copy, SamplePtr old, const juce::String& what)
    {
        const int prog = ctx.currentProgram();
        const int pad = ctx.selectedPad;
        ctx.engine.withProject ([&] (Project& p) {
            copy->name = p.makeUniqueSampleName (old->name + " " + what);
            p.addSample (copy);
            auto& l = p.programs[(size_t) prog].pads[(size_t) pad].layers[0];
            l.sample = copy;
            l.sliceStart = 0;
            l.sliceEnd = -1;
        });
        ctx.changed();
        if (ctx.flashMessage)
            ctx.flashMessage ("New sample: " + copy->name);
    }

    void applySliceToPad()
    {
        if (selected < 0 || wave.getSample() == nullptr)
        {
            if (ctx.flashMessage)
                ctx.flashMessage ("Click a slice first");
            return;
        }
        const int prog = ctx.currentProgram();
        const int pad = ctx.selectedPad;
        const int a = markers[(size_t) selected], b = sliceEnd (selected);
        ctx.engine.withProject ([&] (Project& p) {
            auto& l = p.programs[(size_t) prog].pads[(size_t) pad].layers[0];
            l.sliceStart = a;
            l.sliceEnd = b;
        });
        ctx.changed();
    }

    void convert()
    {
        auto s = wave.getSample();
        if (s == nullptr || markers.size() < 2)
        {
            if (ctx.flashMessage)
                ctx.flashMessage ("Chop the sample first");
            return;
        }
        const int targetProg = target.getSelectedId() - 1;
        const int seqIdx = ctx.engine.getCurrentSequence();
        const bool seqToo = makeSeq.getToggleState();
        const bool tempo = fitTempo.getToggleState();
        static const int barsFor[] = { 1, 2, 4, 8 };
        const int loopLen = barsFor[loopBars.getSelectedId() - 1];
        const auto slices = markers;

        ctx.engine.pushUndo ("convert chops");
        ctx.engine.withProject ([&] (Project& p) {
            auto& prog = p.programs[(size_t) targetProg];
            prog = Program();
            prog.name = s->name.substring (0, 18) + " chops";
            for (size_t i = 0; i < slices.size(); ++i)
            {
                auto& l = prog.pads[i].layers[0];
                l.sample = s;
                l.sliceStart = slices[i];
                l.sliceEnd = i + 1 < slices.size() ? slices[i + 1] : -1;
                prog.pads[i].poly = PolyMode::Mono;
                prog.pads[i].releaseMs = 10.0f;
            }

            if (! seqToo)
                return;
            auto& seq = p.sequences[(size_t) seqIdx];
            if (tempo)
            {
                const double est = chop::estimateLoopBpm (*s, loopLen);
                if (est > 0.0)
                    seq.bpm = est;
                seq.bars = std::max (seq.bars, loopLen);
            }
            auto& track = seq.tracks[(size_t) targetProg];
            track.programIndex = targetProg;
            track.name = "Chops";
            track.events.clear();
            const double ticksPerFrame = seq.bpm / 60.0 * kPPQN / s->sampleRate;
            for (size_t i = 0; i < slices.size(); ++i)
            {
                NoteEvent e;
                e.tick = (int64_t) std::llround (slices[i] * ticksPerFrame);
                if (e.tick >= seq.getLengthTicks())
                    break;
                e.pad = (int16_t) i;
                e.velocity = 110;
                track.insertSorted (e);
            }
        });

        ctx.engine.updateSettings ([targetProg, seqToo] (PerformanceSettings& st) {
            if (seqToo)
                st.currentTrack = targetProg;
        });
        ctx.engine.setCurrentBank (0);
        ctx.setSelectedPad (0);
        ctx.changed();
        if (ctx.flashMessage)
            ctx.flashMessage (juce::String ((int) slices.size()) + " slices -> Program " + juce::String (targetProg + 1)
                              + (seqToo ? " + Track " + juce::String (targetProg + 1) : juce::String()));
    }

    juce::Label title, info, hint;
    Waveform wave;
    std::vector<int> markers { 0 };
    int selected = -1;

    juce::ComboBox mode, beats, target, loopBars;
    juce::Slider sensitivity, minTime, regions, bpm;
    Caption capMode { "Chop mode" }, capSens { "Sensitivity" }, capMin { "Min slice" }, capRegions { "Regions" },
        capBpm { "Loop BPM" }, capBeats { "Slice every" }, capTarget { "Convert to" }, capLoop { "Loop length" };
    juce::TextButton chopBtn { "Chop" }, clearBtn { "Clear" }, previewBtn { "Play all" }, stopBtn { "Stop" },
        normalizeBtn { "Normalize" }, reverseBtn { "Reverse" }, trimBtn { "Trim to slice" },
        setSliceBtn { "Pad plays this slice" }, convertBtn { "Convert slices to pads" };
    juce::ToggleButton makeSeq, fitTempo;
};
} // namespace

std::unique_ptr<Screen> createSampleScreen (AppContext& c) { return std::make_unique<SampleScreen> (c); }

} // namespace padlab
