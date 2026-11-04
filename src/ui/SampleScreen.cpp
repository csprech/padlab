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
        title.setColour (juce::Label::textColourId, theme::lcd::accent);
        addAndMakeVisible (info);
        info.setFont (theme::screenFont (12.0f));
        info.setColour (juce::Label::textColourId, theme::lcd::textDim);
        info.setJustificationType (juce::Justification::centredRight);

        addAndMakeVisible (wave);
        wave.onClick = [this] (int frame, const juce::ModifierKeys& mods) { waveClicked (frame, mods); };
        wave.onDoubleClick = [this] (int frame) { addMarker (frame); };
        wave.onMarkerDragged = [this] (int index, int frame) { moveMarker (index, frame); };
        wave.onMarkerDragEnd = [this] { auditionSelected(); };

        setupParam (sliceStart, 0, 1, 1);
        sliceStart.textFromValueFunction = [this] (double v) {
            auto smp = wave.getSample();
            // two decimals: about one frame at 44.1 kHz, so nudging with the DATA wheel always shows
            return smp != nullptr ? juce::String (v / smp->sampleRate * 1000.0, 2) + " ms" : juce::String ("-");
        };
        sliceStart.onValueChange = [this] {
            if (! loadingSlice && selected > 0)
                moveMarker (selected, (int) sliceStart.getValue());
        };
        addAndMakeVisible (sliceStart);
        addAndMakeVisible (capSlice);

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
        clearBtn.onClick = [this] { clearChops(); };
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
        for (int i = 0; i < kNumPrograms; ++i)
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
        setupCombo (voicing, { "Poly (overlap)", "Mono (one at a time)" });
        voicing.setSelectedId (1, juce::dontSendNotification);
        voicing.setTooltip ("How the chop pads play when you hit several: Poly lets them ring over each other, "
                            "Mono cuts the last one off (they share a mute group)");
        voicing.onChange = [this] { voicingChanged(); };
        target.onChange = [this] { showTargetVoicing(); };

        for (auto* comp : std::initializer_list<juce::Component*> { &chopBtn, &clearBtn, &previewBtn, &stopBtn, &normalizeBtn,
                                                                    &reverseBtn, &trimBtn, &setSliceBtn, &target, &makeSeq,
                                                                    &fitTempo, &loopBars, &convertBtn, &capTarget, &capLoop,
                                                                    &voicing, &capVoicing, &hint })
            addAndMakeVisible (comp);
        hint.setFont (theme::screenFont (11.5f));
        hint.setColour (juce::Label::textColourId, theme::lcd::textDim);
        hint.setText ("CLICK play  DBL/SHIFT-CLICK add  CMD/ALT-CLICK delete  DRAG move  WHEEL zoom  SHIFT-WHEEL pan   "
                      "KEYS: H chop  M mark  DEL delete  J/K prev/next  T trim  0 show all",
                      juce::dontSendNotification);
        updateVisibility();
    }

    juce::String getTitle() const override { return "SAMPLE EDIT"; }
    juce::String getHelpTopic() const override { return "Chopping a sample"; }

    bool handleKey (const juce::KeyPress& k) override
    {
        const auto mods = k.getModifiers();
        const int code = k.getKeyCode();
        const auto ch = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
        if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
        {
            if (mods.isCommandDown())
                clearChops();
            else
                deleteSelectedChop();
            return true;
        }
        if (mods.isCommandDown())
            return false;
        if (ch == 'h') { doChop(); return true; }
        if (ch == 'm') { markAtPlayhead(); return true; }
        if (ch == 'j') { selectSlice (selected - 1); return true; }
        if (ch == 'k') { selectSlice (selected + 1); return true; }
        if (ch == 't') { trimToSlice(); return true; }
        if (ch == 'p') { auditionSelected(); return true; }
        if (ch == '0') { wave.showAll(); return true; }
        return false;
    }

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
                sliceStart.setRange (0, juce::jmax (1, s->getNumFrames() - 1), 1);
            if (s != nullptr)
            {
                const double est = chop::estimateLoopBpm (*s, loopBars.getSelectedId() == 1 ? 1 : loopBars.getSelectedId() == 2 ? 2 : loopBars.getSelectedId() == 3 ? 4 : 8);
                if (est > 0)
                    bpm.setValue (est, juce::dontSendNotification);
            }
        }
        wave.setSample (s);
        showTargetVoicing();
        wave.setRegion (s != nullptr && (a > 0 || b >= 0) ? a : -1, b);
        title.setText ("PAD " + AppContext::padName (pad) + (s != nullptr ? "  " + s->name : juce::String()),
                       juce::dontSendNotification);
        if (s != nullptr)
            info.setText (juce::String (s->getLengthSeconds(), 3) + " s   " + juce::String (s->sampleRate / 1000.0, 1)
                              + " kHz   " + (s->buffer.getNumChannels() > 1 ? "stereo" : "mono") + "   "
                              + juce::String ((int) markers.size()) + (markers.size() == 1 ? " slice" : " slices"),
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

        const float k = juce::jmin (1.0f, controls.getWidth() / 770.0f); // shrink columns on a narrow LCD
        auto W = [k] (int w) { return (int) std::floor (w * k); };
        auto row1 = controls.removeFromTop (44);
        auto place = [] (juce::Rectangle<int>& row, Caption& cap, juce::Component& comp, int w) {
            auto cell = row.removeFromLeft (w);
            row.removeFromLeft (6);
            cap.setBounds (cell.removeFromTop (16));
            comp.setBounds (cell.withHeight (24));
        };
        place (row1, capMode, mode, W (100));
        place (row1, capSens, sensitivity, W (96));
        place (row1, capMin, minTime, W (86));
        place (row1, capRegions, regions, W (60));
        place (row1, capBpm, bpm, W (92));
        place (row1, capBeats, beats, W (82));
        place (row1, capSlice, sliceStart, W (96));
        row1.removeFromTop (16);
        const int bw = juce::jmax (40, (row1.getWidth() - 6) / 2);
        chopBtn.setBounds (row1.removeFromLeft (bw).withHeight (24));
        row1.removeFromLeft (6);
        clearBtn.setBounds (row1.removeFromLeft (bw).withHeight (24));

        controls.removeFromTop (8);
        auto row2 = controls.removeFromTop (28);
        const int unit = (row2.getWidth() - 5 * 6) / 7;
        for (auto* b : { &previewBtn, &stopBtn, &normalizeBtn, &reverseBtn, &trimBtn, &setSliceBtn })
        {
            b->setBounds (row2.removeFromLeft (b == &setSliceBtn ? row2.getWidth() : unit));
            row2.removeFromLeft (6);
        }

        controls.removeFromTop (8);
        auto row3 = controls.removeFromTop (44);
        place (row3, capTarget, target, W (106));
        place (row3, capVoicing, voicing, W (144));
        place (row3, capLoop, loopBars, W (76));
        row3.removeFromTop (16);
        makeSeq.setBounds (row3.removeFromLeft (W (132)).withHeight (24));
        fitTempo.setBounds (row3.removeFromLeft (W (156)).withHeight (24));
        row3.removeFromLeft (6);
        convertBtn.setBounds (row3.withHeight (26));
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
        const juce::ScopedValueSetter<bool> svs (loadingSlice, true);
        sliceStart.setEnabled (selected > 0);
        if (selected >= 0 && selected < (int) markers.size())
            sliceStart.setValue (markers[(size_t) selected], juce::dontSendNotification);
        sliceStart.updateText();
    }

    void doChop()
    {
        auto s = wave.getSample();
        if (s == nullptr)
            return flashMsg ("No sample on this pad - load one first (see HELP)");
        if (mode.getSelectedId() == 4)
            return flashMsg ("Manual mode: double-click the waveform (or press M while it plays) to add chops");
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

    // ---- chop editing: the same in every mode
    //   click = select + play a slice, double/shift-click = add a marker, cmd/alt-click = delete the nearest
    //   marker, drag a marker line = move it. Manual mode just means CHOP doesn't auto-detect.
    void waveClicked (int frame, const juce::ModifierKeys& mods)
    {
        if (mods.isCommandDown() || mods.isAltDown())
            return deleteMarkerNear (frame);
        if (mods.isShiftDown() || mode.getSelectedId() == 4)
            return addMarker (frame);
        selectSlice (sliceAt (frame));
    }

    void selectSlice (int i)
    {
        if (wave.getSample() == nullptr || markers.empty())
            return;
        selected = juce::jlimit (0, (int) markers.size() - 1, i);
        wave.reveal (markers[(size_t) selected]); // J / K while zoomed: follow the slice
        syncWave();
        auditionSelected();
    }

    void auditionSelected()
    {
        if (auto smp = wave.getSample())
        {
            if (selected >= 0 && selected < (int) markers.size())
                ctx.engine.previewSample (smp, markers[(size_t) selected], sliceEnd (selected));
            else
                ctx.engine.previewSample (smp, 0, -1);
        }
    }

    void addMarker (int frame)
    {
        auto smp = wave.getSample();
        if (smp == nullptr)
            return;
        if (markers.size() >= (size_t) kNumPads)
            return flashMsg ("Max 128 slices");
        frame = juce::jlimit (1, smp->getNumFrames() - 1, frame);
        markers.insert (std::upper_bound (markers.begin(), markers.end(), frame), frame);
        markers.erase (std::unique (markers.begin(), markers.end()), markers.end());
        selected = sliceAt (frame);
        syncWave();
        updateInfo();
    }

    void deleteMarkerNear (int frame)
    {
        int best = -1, bestDist = std::numeric_limits<int>::max();
        for (int i = 1; i < (int) markers.size(); ++i)
            if (std::abs (markers[(size_t) i] - frame) < bestDist)
            {
                bestDist = std::abs (markers[(size_t) i] - frame);
                best = i;
            }
        if (best <= 0)
            return flashMsg ("No chop to delete");
        markers.erase (markers.begin() + best);
        selected = juce::jmin (best - 1, (int) markers.size() - 1);
        syncWave();
        updateInfo();
    }

    /** DELETE: remove the selected slice's start (it merges into the slice before it). */
    void deleteSelectedChop()
    {
        if (markers.size() <= 1)
            return flashMsg ("No chops to delete");
        int idx = selected <= 0 ? 1 : selected;
        if (selected < 0)
            return flashMsg ("Click a slice first (DEL removes its start marker)");
        idx = juce::jlimit (1, (int) markers.size() - 1, idx);
        markers.erase (markers.begin() + idx);
        selected = idx - 1;
        syncWave();
        updateInfo();
        flashMsg ("Chop deleted - " + juce::String ((int) markers.size()) + " slices");
    }

    void clearChops()
    {
        markers = { 0 };
        selected = -1;
        syncWave();
        updateInfo();
        flashMsg ("Chops cleared");
    }

    /** M: tap a chop at the preview playhead while it plays (or split the selected slice in half). */
    void markAtPlayhead()
    {
        const int pos = ctx.engine.getPreviewPosition();
        if (pos > 0)
            return addMarker (pos);
        if (selected >= 0)
            return addMarker ((markers[(size_t) selected] + sliceEnd (selected)) / 2);
        flashMsg ("Play the sample (P / PLAY ALL) and press M to drop chops in time");
    }

    void moveMarker (int index, int frame)
    {
        if (index <= 0 || index >= (int) markers.size())
            return;
        const int lo = markers[(size_t) index - 1] + 1;
        const int hi = index + 1 < (int) markers.size() ? markers[(size_t) index + 1] - 1 : (wave.getSample() ? wave.getSample()->getNumFrames() - 1 : frame);
        markers[(size_t) index] = juce::jlimit (lo, juce::jmax (lo, hi), frame);
        selected = index;
        syncWave();
    }

    void updateInfo() { contextChanged(); }
    void flashMsg (const juce::String& m)
    {
        if (ctx.flashMessage)
            ctx.flashMessage (m);
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
        const auto chopVoicing = voicing.getSelectedId() == 2 ? chop::Voicing::Mono : chop::Voicing::Poly;

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
                prog.pads[i].releaseMs = 10.0f;
            }
            chop::setVoicing (prog, s.get(), chopVoicing);

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
                              + (seqToo ? " + Track " + juce::String (targetProg + 1) : juce::String())
                              + (chopVoicing == chop::Voicing::Mono ? ", mono" : ", poly"));
    }

    /** CHOPS PLAY: applies straight away to chops already converted into the target program,
        and to every conversion after. */
    void voicingChanged()
    {
        auto s = wave.getSample();
        const auto v = voicing.getSelectedId() == 2 ? chop::Voicing::Mono : chop::Voicing::Poly;
        const int targetProg = target.getSelectedId() - 1;
        int changed = 0;
        bool has = false;
        ctx.engine.withProject ([&] (Project& p) { has = ! chop::chopPads (p.programs[(size_t) targetProg], s.get()).empty(); });
        if (has)
        {
            ctx.engine.pushUndo ("chop voicing");
            ctx.engine.withProject ([&] (Project& p) { changed = chop::setVoicing (p.programs[(size_t) targetProg], s.get(), v); });
            ctx.changed();
        }
        const auto how = juce::String (v == chop::Voicing::Mono ? "mono (one at a time)" : "poly (they overlap)");
        if (changed > 0)
            flashMsg (juce::String (changed) + " chops in Program " + juce::String (targetProg + 1) + " now play " + how);
        else
            flashMsg ("Chops will play " + how + " when you convert");
    }

    /** Shows the voicing of the chops already in the target program (if this sample has some there). */
    void showTargetVoicing()
    {
        auto s = wave.getSample();
        const int targetProg = target.getSelectedId() - 1;
        bool has = false;
        auto v = chop::Voicing::Poly;
        ctx.engine.withProject ([&] (Project& p) {
            const auto& prog = p.programs[(size_t) targetProg];
            has = ! chop::chopPads (prog, s.get()).empty();
            v = chop::getVoicing (prog, s.get());
        });
        if (has)
            voicing.setSelectedId (v == chop::Voicing::Mono ? 2 : 1, juce::dontSendNotification);
    }

    juce::Label title, info, hint;
    juce::Slider sliceStart;
    Caption capSlice { "Slice start" };
    bool loadingSlice = false;
    Waveform wave;
    std::vector<int> markers { 0 };
    int selected = -1;

    juce::ComboBox mode, beats, target, loopBars, voicing;
    juce::Slider sensitivity, minTime, regions, bpm;
    Caption capMode { "Chop mode" }, capSens { "Sensitivity" }, capMin { "Min slice" }, capRegions { "Regions" },
        capBpm { "Loop BPM" }, capBeats { "Slice every" }, capTarget { "Convert to" }, capLoop { "Loop length" },
        capVoicing { "Chops play" };
    juce::TextButton chopBtn { "Chop" }, clearBtn { "Clear" }, previewBtn { "Play all" }, stopBtn { "Stop" },
        normalizeBtn { "Normalize" }, reverseBtn { "Reverse" }, trimBtn { "Trim to slice" },
        setSliceBtn { "Pad plays this slice" }, convertBtn { "Convert slices to pads" };
    juce::ToggleButton makeSeq, fitTempo;
};
} // namespace

std::unique_ptr<Screen> createSampleScreen (AppContext& c) { return std::make_unique<SampleScreen> (c); }

} // namespace padlab
