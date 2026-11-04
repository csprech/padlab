#include "ui/Screens.h"
#include "core/Timing.h"

namespace padlab
{
namespace
{
juce::StringArray divisionNames()
{
    juce::StringArray a;
    for (auto& d : timing::divisions())
        a.add (d.name);
    return a;
}

// ------------------------------------------------------------------ event grid

class EventGrid : public juce::Component
{
public:
    explicit EventGrid (AppContext& c) : ctx (c) { setBufferedToImage (true); }

    void refreshData()
    {
        const int seqIdx = ctx.engine.getCurrentSequence();
        const auto s = ctx.engine.getSettings();
        track = s.currentTrack;
        bank = ctx.engine.getCurrentBank();
        ctx.engine.withProject ([&] (Project& p) {
            const auto& seq = p.sequences[(size_t) seqIdx];
            length = seq.getLengthTicks();
            beatsPerBar = seq.beatsPerBar;
            events = seq.tracks[(size_t) track].events;
            const auto& prog = p.programs[(size_t) seq.tracks[(size_t) track].programIndex];
            for (int i = 0; i < kPadsPerBank; ++i)
            {
                juce::String n;
                for (auto& l : prog.pads[(size_t) (bank * kPadsPerBank + i)].layers)
                    if (l.sample != nullptr) { n = describeLayer (l); break; }
                rowNames[(size_t) i] = n;
            }
        });
        const auto sig = signature();
        if (sig != lastSignature)
        {
            lastSignature = sig;
            repaint();
        }
    }

    /** Playhead x for a tick position, or -1. */
    juce::Rectangle<int> playheadRect (double ticks) const
    {
        if (length <= 0)
            return {};
        return { (int) xFor ((int64_t) ticks), headerH, 2, getHeight() - headerH };
    }

    void paint (juce::Graphics& g) override
    {
        using namespace theme::lcd;
        auto r = getLocalBounds();
        g.setColour (screen);
        g.fillRect (r);
        auto gutter = r.removeFromLeft (gutterW);
        auto header = r.removeFromTop (headerH);
        gutter.removeFromTop (headerH);
        const float rowH = r.getHeight() / (float) kPadsPerBank;

        // rows
        for (int i = 0; i < kPadsPerBank; ++i)
        {
            const int pad = bank * kPadsPerBank + i;
            auto row = rowRect (i);
            if (pad == ctx.selectedPad)
            {
                g.setColour (accent.withAlpha (0.08f));
                g.fillRect (row.withX (0).withWidth (getWidth()));
            }
            else if (i % 2 == 0)
            {
                g.setColour (text.withAlpha (0.035f));
                g.fillRect (row);
            }
            g.setColour (pad == ctx.selectedPad ? accent : textDim);
            g.setFont (theme::screenFont (juce::jmin (12.0f, rowH * 0.7f), pad == ctx.selectedPad));
            g.drawText (AppContext::padName (pad) + " " + rowNames[(size_t) i],
                        juce::Rectangle<float> (4.0f, row.getY(), (float) gutterW - 6.0f, rowH).toNearestInt(),
                        juce::Justification::centredLeft, true);
        }

        // vertical grid
        const int64_t barTicks = (int64_t) beatsPerBar * kPPQN;
        for (int64_t t = 0; t <= length; t += 240)
        {
            const float x = xFor (t);
            const bool bar = (t % barTicks) == 0, beat = (t % kPPQN) == 0;
            g.setColour (bar ? text.withAlpha (0.6f) : beat ? text.withAlpha (0.3f) : grid);
            g.drawVerticalLine ((int) x, (float) header.getBottom(), (float) getHeight());
            if (bar && t < length)
            {
                g.setColour (textDim);
                g.setFont (theme::screenFont (11.0f));
                g.drawText (juce::String (t / barTicks + 1), (int) x + 3, header.getY(), 30, header.getHeight(),
                            juce::Justification::centredLeft);
            }
        }
        g.setColour (screenLine);
        g.drawHorizontalLine (header.getBottom(), 0.0f, (float) getWidth());

        // events
        for (auto& e : events)
        {
            const int i = e.pad - bank * kPadsPerBank;
            if (i < 0 || i >= kPadsPerBank)
                continue;
            auto row = rowRect (i);
            const float x = xFor (e.tick);
            const float w = juce::jmax (5.0f, xFor (e.tick + juce::jmin<int64_t> (e.length, 240)) - x - 1.0f);
            auto rect = juce::Rectangle<float> (x + 1.0f, row.getY() + 2.0f, w, row.getHeight() - 4.0f);
            const float v = e.velocity / 127.0f;
            g.setColour (accent.withAlpha (0.35f + 0.65f * v));
            g.fillRect (rect);
            // velocity bar
            g.setColour (screen.withAlpha (0.55f));
            g.fillRect (rect.reduced (1.5f).withHeight ((rect.getHeight() - 3.0f) * (1.0f - v)));
        }

    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < gutterW)
        {
            const int i = rowAt (e.y);
            if (i >= 0)
            {
                const int pad = bank * kPadsPerBank + i;
                ctx.setSelectedPad (pad);
                ctx.engine.audition (track, pad);
            }
            return;
        }
        const int i = rowAt (e.y);
        if (i < 0)
            return;
        const int pad = bank * kPadsPerBank + i;
        const auto s = ctx.engine.getSettings();
        const auto& div = timing::divisions()[(size_t) s.timingCorrect];
        const int64_t raw = tickAt (e.x);
        const int gridTicks = div.ticks > 1 ? div.ticks : 240;
        int64_t tick = div.ticks > 1 ? timing::quantize (raw, div.ticks, div.swingable, s.swing, 1.0, s.legacy96)
                                     : raw;
        tick = juce::jlimit<int64_t> (0, length - 1, tick);

        // hit an existing note? delete it (or start a velocity drag with shift)
        const int seqIdx = ctx.engine.getCurrentSequence();
        dragEvent = -1;
        for (size_t k = 0; k < events.size(); ++k)
        {
            const auto& ev = events[k];
            if (ev.pad == pad && raw >= ev.tick - 20 && raw <= ev.tick + std::max<int64_t> (gridTicks / 2, std::min<int64_t> (ev.length, 240)))
            {
                if (e.mods.isShiftDown())
                {
                    ctx.engine.pushUndo ("velocity");
                    dragEvent = (int) k;
                    dragTick = ev.tick;
                    dragPad = ev.pad;
                    dragStartVel = ev.velocity;
                    return;
                }
                ctx.engine.pushUndo ("delete note");
                ctx.engine.withProject ([&] (Project& p) {
                    auto& evs = p.sequences[(size_t) seqIdx].tracks[(size_t) track].events;
                    for (auto it = evs.begin(); it != evs.end(); ++it)
                        if (it->tick == ev.tick && it->pad == ev.pad)
                        {
                            evs.erase (it);
                            break;
                        }
                });
                refreshData();
                return;
            }
        }

        ctx.engine.pushUndo ("add note");
        ctx.engine.withProject ([&] (Project& p) {
            NoteEvent ne;
            ne.tick = tick;
            ne.pad = (int16_t) pad;
            ne.velocity = (uint8_t) (s.fullLevel ? 127 : 100);
            ne.length = gridTicks;
            p.sequences[(size_t) seqIdx].tracks[(size_t) track].insertSorted (ne);
        });
        ctx.setSelectedPad (pad);
        if (ctx.engine.getTransportState() == Transport::State::Stopped)
            ctx.engine.audition (track, pad, s.fullLevel ? 127 : 100);
        refreshData();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragEvent < 0)
            return;
        const int vel = juce::jlimit (1, 127, dragStartVel - e.getDistanceFromDragStartY());
        const int seqIdx = ctx.engine.getCurrentSequence();
        ctx.engine.withProject ([&] (Project& p) {
            for (auto& ev : p.sequences[(size_t) seqIdx].tracks[(size_t) track].events)
                if (ev.tick == dragTick && ev.pad == dragPad)
                    ev.velocity = (uint8_t) vel;
        });
        refreshData();
    }

    void mouseUp (const juce::MouseEvent&) override { dragEvent = -1; }

private:
    float xFor (int64_t tick) const
    {
        const float w = (float) (getWidth() - gutterW);
        return gutterW + (length > 0 ? w * (float) tick / (float) length : 0.0f);
    }
    int64_t tickAt (int x) const
    {
        const double w = getWidth() - gutterW;
        return (int64_t) ((x - gutterW) / w * (double) length);
    }
    juce::Rectangle<float> rowRect (int i) const
    {
        const float rowH = (getHeight() - headerH) / (float) kPadsPerBank;
        return { (float) gutterW, headerH + (kPadsPerBank - 1 - i) * rowH, (float) (getWidth() - gutterW), rowH };
    }
    int rowAt (int y) const
    {
        if (y < headerH)
            return -1;
        const float rowH = (getHeight() - headerH) / (float) kPadsPerBank;
        return juce::jlimit (0, kPadsPerBank - 1, kPadsPerBank - 1 - (int) ((y - headerH) / rowH));
    }

    juce::int64 signature() const
    {
        juce::int64 h = (juce::int64) length * 31 + beatsPerBar * 7 + track * 131 + bank * 977 + ctx.selectedPad * 7919;
        for (auto& e : events)
            h = h * 1000003 + e.tick * 131 + e.pad * 17 + e.velocity * 3 + e.length;
        for (auto& n : rowNames)
            h = h * 31 + n.hashCode64();
        return h;
    }

    AppContext& ctx;
    static constexpr int gutterW = 118, headerH = 18;
    std::vector<NoteEvent> events;
    std::array<juce::String, kPadsPerBank> rowNames;
    int64_t length = 3840;
    int beatsPerBar = 4, track = 0, bank = 0;
    juce::int64 lastSignature = 0;
    int dragEvent = -1, dragPad = 0, dragStartVel = 100;
    int64_t dragTick = 0;
};

// ------------------------------------------------------------------ track strip

class TrackStrip : public juce::Component
{
public:
    explicit TrackStrip (AppContext& c) : ctx (c) {}

    void refreshData()
    {
        const int seqIdx = ctx.engine.getCurrentSequence();
        current = ctx.engine.getSettings().currentTrack;
        ctx.engine.withProject ([&] (Project& p) {
            const auto& seq = p.sequences[(size_t) seqIdx];
            for (int t = 0; t < kNumTracks; ++t)
            {
                const auto& tr = seq.tracks[(size_t) t];
                info[(size_t) t] = { tr.name, p.programs[(size_t) tr.programIndex].name, tr.mute, tr.solo,
                                     ! tr.events.empty() };
            }
        });
        juce::int64 sig = current;
        for (int t = 0; t < kNumTracks; ++t)
        {
            level[(size_t) t] = juce::jmax (ctx.engine.getTrackPeak (t), level[(size_t) t] * 0.8f);
            const auto& in = info[(size_t) t];
            sig = sig * 131 + in.name.hashCode64() + in.program.hashCode64() * 3 + (in.mute ? 5 : 0) + (in.solo ? 7 : 0)
                  + (in.hasEvents ? 11 : 0);
        }
        if (sig != lastSignature)
        {
            lastSignature = sig;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace theme::lcd;
        for (int t = 0; t < kNumTracks; ++t)
        {
            auto r = cell (t);
            const auto& in = info[(size_t) t];
            g.setColour (t == current ? accent.withAlpha (0.16f) : juce::Colours::transparentBlack);
            g.fillRect (r);
            g.setColour (t == current ? accent : screenLine);
            g.drawRect (r, 1.0f);

            auto inner = r.reduced (5.0f, 3.0f);
            auto top = inner.removeFromTop (inner.getHeight() * 0.5f);
            g.setFont (theme::screenFont (12.0f, true));
            g.setColour (in.hasEvents ? text : textDim);
            g.drawText (juce::String (t + 1) + " " + in.name, top.toNearestInt(), juce::Justification::centredLeft, true);
            g.setFont (theme::screenFont (10.5f));
            g.setColour (textDim);
            g.drawText (in.program, inner.withTrimmedRight (40).toNearestInt(), juce::Justification::centredLeft, true);

            // M / S
            auto ms = inner.removeFromRight (36.0f);
            auto m = ms.removeFromLeft (17.0f).reduced (1.0f), s = ms.reduced (1.0f);
            g.setColour (text);
            if (in.mute) g.fillRect (m); else g.drawRect (m, 1.0f);
            if (in.solo) g.fillRect (s); else g.drawRect (s, 1.0f);
            g.setFont (theme::screenFont (10.0f, true));
            g.setColour (in.mute ? screen : text);
            g.drawText ("M", m.toNearestInt(), juce::Justification::centred);
            g.setColour (in.solo ? screen : text);
            g.drawText ("S", s.toNearestInt(), juce::Justification::centred);

        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int t = 0; t < kNumTracks; ++t)
        {
            auto r = cell (t);
            if (! r.contains (e.position))
                continue;
            auto inner = r.reduced (5.0f, 3.0f);
            inner.removeFromTop (inner.getHeight() * 0.5f);
            auto ms = inner.removeFromRight (36.0f);
            auto m = ms.removeFromLeft (17.0f);
            const int seqIdx = ctx.engine.getCurrentSequence();
            if (m.contains (e.position) || ms.contains (e.position))
            {
                const bool isMute = m.contains (e.position);
                ctx.engine.withProject ([&] (Project& p) {
                    auto& tr = p.sequences[(size_t) seqIdx].tracks[(size_t) t];
                    if (isMute) tr.mute = ! tr.mute;
                    else tr.solo = ! tr.solo;
                });
            }
            else
            {
                ctx.engine.updateSettings ([t] (PerformanceSettings& s) { s.currentTrack = t; });
            }
            ctx.changed();
            refreshData();
            return;
        }
    }

    juce::Rectangle<int> meterRect (int t) const
    {
        auto r = cell (t);
        return r.removeFromBottom (3.0f).reduced (2.0f, 0.0f).toNearestInt();
    }
    float meterLevel (int t) const { return std::round (juce::jmin (1.0f, level[(size_t) t]) * 40.0f) / 40.0f; }

private:
    juce::Rectangle<float> cell (int t) const
    {
        const float w = getWidth() / 4.0f, h = getHeight() / 2.0f;
        return juce::Rectangle<float> ((t % 4) * w, (t / 4) * h, w, h).reduced (2.0f);
    }

    struct Info
    {
        juce::String name, program;
        bool mute = false, solo = false, hasEvents = false;
    };
    AppContext& ctx;
    std::array<Info, kNumTracks> info;
    std::array<float, kNumTracks> level {};
    int current = 0;
    juce::int64 lastSignature = -1;
};

// ------------------------------------------------------------------ main screen

class MainScreen : public Screen
{
public:
    explicit MainScreen (AppContext& c) : Screen (c), grid (c), tracks (c)
    {
        for (auto* cap : { &capSeq, &capBpm, &capBars, &capTs, &capProg, &capTc, &capSwing, &capRepeat })
            addAndMakeVisible (cap);

        juce::StringArray seqs;
        for (int i = 0; i < kNumSequences; ++i)
            seqs.add (juce::String (i + 1).paddedLeft ('0', 2));
        setupCombo (seqBox, seqs);
        seqBox.onChange = [this] {
            if (! loading)
                ctx.engine.queueSequence (seqBox.getSelectedId() - 1);
        };

        setupParam (bpm, 30.0, 300.0, 0.1, " BPM");
        bpm.onValueChange = [this] {
            if (! loading)
                editSequence ([v = bpm.getValue()] (Sequence& s) { s.bpm = v; });
        };

        setupCombo (bars, { "1", "2", "3", "4", "8", "16", "32", "64" });
        bars.onChange = [this] {
            if (loading)
                return;
            const int b = bars.getText().getIntValue();
            ctx.engine.pushUndo ("change length");
            editSequence ([b] (Sequence& s) {
                s.bars = b;
                const auto L = s.getLengthTicks();
                for (auto& t : s.tracks)
                    t.events.erase (std::remove_if (t.events.begin(), t.events.end(),
                                                    [L] (const NoteEvent& e) { return e.tick >= L; }),
                                    t.events.end());
            });
        };

        setupCombo (timeSig, { "2/4", "3/4", "4/4", "5/4", "6/4", "7/4" });
        timeSig.onChange = [this] {
            if (! loading)
                editSequence ([b = timeSig.getSelectedId() + 1] (Sequence& s) { s.beatsPerBar = b; });
        };

        programBox.onChange = [this] {
            if (loading)
                return;
            const int prog = programBox.getSelectedId() - 1, t = ctx.currentTrack();
            editSequence ([prog, t] (Sequence& s) { s.tracks[(size_t) t].programIndex = prog; });
            ctx.changed();
        };

        setupCombo (tc, divisionNames());
        tc.onChange = [this] {
            if (! loading)
                ctx.engine.updateSettings ([v = tc.getSelectedId() - 1] (PerformanceSettings& s) { s.timingCorrect = v; });
        };
        setupParam (swing, 50.0, 75.0, 1.0, "%");
        swing.onValueChange = [this] {
            if (! loading)
                ctx.engine.updateSettings ([v = swing.getValue()] (PerformanceSettings& s) { s.swing = v; });
        };
        setupCombo (repeatRate, divisionNames());
        repeatRate.setItemEnabled (timing::kNumDivisions, false); // "Off" isn't a repeat rate
        repeatRate.onChange = [this] {
            if (! loading)
                ctx.engine.updateSettings ([v = repeatRate.getSelectedId() - 1] (PerformanceSettings& s) { s.repeatDivision = v; });
        };

        legacy96.setButtonText ("96 PPQN grid");
        legacy96.setTooltip ("Snap swing to the coarse 96 PPQN grid of late-80s sequencers");
        legacy96.onClick = [this] {
            ctx.engine.updateSettings ([v = legacy96.getToggleState()] (PerformanceSettings& s) { s.legacy96 = v; });
        };
        metronome.setButtonText ("Click");
        metronome.onClick = [this] {
            ctx.engine.updateSettings ([v = metronome.getToggleState()] (PerformanceSettings& s) { s.metronome = v; });
        };
        countIn.setButtonText ("Count-in");
        countIn.onClick = [this] {
            ctx.engine.updateSettings ([v = countIn.getToggleState()] (PerformanceSettings& s) { s.countIn = v; });
        };

        clearBtn.onClick = [this] {
            ctx.engine.pushUndo ("clear track");
            const int t = ctx.currentTrack();
            editSequence ([t] (Sequence& s) { s.tracks[(size_t) t].events.clear(); });
            flash ("Track " + juce::String (t + 1) + " cleared");
        };
        quantizeBtn.onClick = [this] {
            const auto s = ctx.engine.getSettings();
            const auto& div = timing::divisions()[(size_t) s.timingCorrect];
            if (div.ticks <= 1)
                return flash ("Timing correct is Off");
            ctx.engine.pushUndo ("quantize");
            const int t = ctx.currentTrack();
            editSequence ([&] (Sequence& seq) {
                auto evs = seq.tracks[(size_t) t].events;
                const auto L = seq.getLengthTicks();
                for (auto& e : evs)
                    e.tick = ((timing::quantize (e.tick, div.ticks, div.swingable, s.swing, s.tcStrength, s.legacy96) % L) + L) % L;
                std::stable_sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.tick < b.tick; });
                seq.tracks[(size_t) t].events.assign (evs.begin(), evs.end());
            });
            flash ("Quantized to " + juce::String (div.name) + " @ " + juce::String ((int) s.swing) + "% swing");
        };
        doubleBtn.onClick = [this] {
            ctx.engine.pushUndo ("double");
            bool ok = true;
            editSequence ([&ok] (Sequence& s) {
                if (s.bars * 2 > 64) { ok = false; return; }
                const auto L = s.getLengthTicks();
                for (auto& t : s.tracks)
                {
                    const auto n = t.events.size();
                    for (size_t i = 0; i < n && ! t.isFull(); ++i)
                    {
                        auto e = t.events[i];
                        e.tick += L;
                        t.events.push_back (e);
                    }
                }
                s.bars *= 2;
            });
            flash (ok ? "Sequence doubled" : "Max 64 bars");
        };

        for (auto* comp : std::initializer_list<juce::Component*> { &seqBox, &bpm, &bars, &timeSig, &programBox, &tc, &swing,
                                                                    &repeatRate, &legacy96, &metronome, &countIn, &clearBtn,
                                                                    &quantizeBtn, &doubleBtn, &grid, &tracks })
            addAndMakeVisible (comp);
    }

    juce::String getTitle() const override { return "MAIN"; }
    juce::String getHelpTopic() const override { return "Recording a beat"; }

    void getLiveOverlay (double ticks, bool playing, LiveOverlay& o) override
    {
        if (playing)
            o.playhead = grid.playheadRect (ticks) + grid.getPosition();
        for (int t = 0; t < kNumTracks; ++t)
            o.meters.push_back ({ tracks.meterRect (t) + tracks.getPosition(), tracks.meterLevel (t), false });
    }

    void contextChanged() override
    {
        const juce::ScopedValueSetter<bool> svs (loading, true);
        const int seqIdx = ctx.engine.getCurrentSequence();
        const auto s = ctx.engine.getSettings();
        juce::StringArray progNames;
        int progIdx = 0;
        ctx.engine.withProject ([&] (Project& p) {
            const auto& seq = p.sequences[(size_t) seqIdx];
            bpm.setValue (seq.bpm, juce::dontSendNotification);
            bars.setText (juce::String (seq.bars), juce::dontSendNotification);
            timeSig.setSelectedId (seq.beatsPerBar - 1, juce::dontSendNotification);
            for (auto& pr : p.programs)
                progNames.add (pr.name);
            progIdx = seq.tracks[(size_t) s.currentTrack].programIndex;
            for (int i = 0; i < kNumSequences; ++i)
                seqBox.changeItemText (i + 1, juce::String (i + 1).paddedLeft ('0', 2) + "  " + p.sequences[(size_t) i].name);
        });
        seqBox.setSelectedId (seqIdx + 1, juce::dontSendNotification);
        setupCombo (programBox, progNames);
        programBox.setSelectedId (progIdx + 1, juce::dontSendNotification);
        tc.setSelectedId (s.timingCorrect + 1, juce::dontSendNotification);
        swing.setValue (s.swing, juce::dontSendNotification);
        repeatRate.setSelectedId (s.repeatDivision + 1, juce::dontSendNotification);
        legacy96.setToggleState (s.legacy96, juce::dontSendNotification);
        metronome.setToggleState (s.metronome, juce::dontSendNotification);
        countIn.setToggleState (s.countIn, juce::dontSendNotification);
        capProg.setText ("TRACK " + juce::String (s.currentTrack + 1) + " PROGRAM", juce::dontSendNotification);
        refresh();
    }

    void refresh() override
    {
        if (ctx.engine.getCurrentSequence() + 1 != seqBox.getSelectedId() && ctx.engine.getQueuedSequence() < 0)
            contextChanged();
        grid.refreshData();
        tracks.refreshData();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto row1 = r.removeFromTop (40);
        auto place = [] (juce::Rectangle<int>& row, Caption& cap, juce::Component& c, int w) {
            auto cell = row.removeFromLeft (w);
            row.removeFromLeft (6);
            cap.setBounds (cell.removeFromTop (14));
            c.setBounds (cell.withHeight (24));
        };
        place (row1, capSeq, seqBox, 150);
        place (row1, capBpm, bpm, 100);
        place (row1, capBars, bars, 58);
        place (row1, capTs, timeSig, 58);
        place (row1, capProg, programBox, 150);
        place (row1, capTc, tc, 70);
        place (row1, capSwing, swing, 70);
        place (row1, capRepeat, repeatRate, 70);

        r.removeFromTop (4);
        auto row2 = r.removeFromTop (24);
        legacy96.setBounds (row2.removeFromLeft (120));
        metronome.setBounds (row2.removeFromLeft (70));
        countIn.setBounds (row2.removeFromLeft (90));
        doubleBtn.setBounds (row2.removeFromRight (80));
        row2.removeFromRight (4);
        quantizeBtn.setBounds (row2.removeFromRight (90));
        row2.removeFromRight (4);
        clearBtn.setBounds (row2.removeFromRight (90));

        r.removeFromTop (6);
        tracks.setBounds (r.removeFromBottom (78));
        r.removeFromBottom (6);
        grid.setBounds (r);
    }

private:
    template <typename Fn>
    void editSequence (Fn&& fn)
    {
        const int seqIdx = ctx.engine.getCurrentSequence();
        ctx.engine.withProject ([&] (Project& p) { fn (p.sequences[(size_t) seqIdx]); });
        grid.refreshData();
    }
    void flash (const juce::String& m)
    {
        if (ctx.flashMessage)
            ctx.flashMessage (m);
    }

    EventGrid grid;
    TrackStrip tracks;
    Caption capSeq { "Sequence" }, capBpm { "Tempo" }, capBars { "Bars" }, capTs { "Time sig" }, capProg { "Program" },
        capTc { "Timing correct" }, capSwing { "Swing" }, capRepeat { "Note repeat" };
    juce::ComboBox seqBox, bars, timeSig, programBox, tc, repeatRate;
    juce::Slider bpm, swing;
    juce::ToggleButton legacy96, metronome, countIn;
    juce::TextButton clearBtn { "Clear track" }, quantizeBtn { "Quantize" }, doubleBtn { "Double" };
    bool loading = false;
};
} // namespace

std::unique_ptr<Screen> createMainScreen (AppContext& c) { return std::make_unique<MainScreen> (c); }

} // namespace padlab
