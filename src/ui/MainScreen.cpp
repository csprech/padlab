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
            const auto& tr = seq.tracks[(size_t) track];
            events = tr.events;
            const auto& prog = p.programs[(size_t) juce::jlimit (0, kNumPrograms - 1, tr.programIndex)];
            for (int i = 0; i < kPadsPerBank; ++i)
            {
                const int pad = bank * kPadsPerBank + i;
                juce::String n;
                if (! tr.internalSound && tr.sendsMidi()) // MIDI track: rows are notes
                    n = juce::MidiMessage::getMidiNoteName (padToMidiNote (pad), true, true, 3) + " (" + juce::String (padToMidiNote (pad)) + ")";
                else
                    for (auto& l : prog.pads[(size_t) pad].layers)
                        if (l.sample != nullptr) { n = describeLayer (l); break; }
                rowNames[(size_t) i] = n;
            }
        });
        // long sequences show 8 bars at a time; the view pages along with the playhead
        const int64_t barTicks = (int64_t) beatsPerBar * kPPQN;
        viewLen = juce::jmin<int64_t> (length, 8 * barTicks);
        const auto pos = (int64_t) ctx.engine.getPositionTicks();
        viewStart = viewLen > 0 ? juce::jlimit<int64_t> (0, juce::jmax<int64_t> (0, length - viewLen), (pos / viewLen) * viewLen) : 0;
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
        if (length <= 0 || ticks < (double) viewStart || ticks >= (double) (viewStart + viewLen))
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
        for (int64_t t = viewStart; t <= viewStart + viewLen; t += 240)
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
            if (i < 0 || i >= kPadsPerBank || e.tick < viewStart || e.tick >= viewStart + viewLen)
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
        return gutterW + (viewLen > 0 ? w * (float) (tick - viewStart) / (float) viewLen : 0.0f);
    }
    int64_t tickAt (int x) const
    {
        const double w = getWidth() - gutterW;
        return viewStart + (int64_t) ((x - gutterW) / w * (double) viewLen);
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
        juce::int64 h = (juce::int64) length * 31 + beatsPerBar * 7 + track * 131 + bank * 977 + ctx.selectedPad * 7919 + viewStart * 13;
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
    int64_t length = 3840, viewStart = 0, viewLen = 3840;
    int beatsPerBar = 4, track = 0, bank = 0;
    juce::int64 lastSignature = 0;
    int dragEvent = -1, dragPad = 0, dragStartVel = 100;
    int64_t dragTick = 0;
};

// ------------------------------------------------------------------ track strip

class TrackStrip : public juce::Component
{
public:
    static constexpr int kPerPage = 8;
    static constexpr int kPages = kNumTracks / kPerPage;

    explicit TrackStrip (AppContext& c) : ctx (c) {}

    void refreshData()
    {
        const int seqIdx = ctx.engine.getCurrentSequence();
        const int cur = ctx.engine.getSettings().currentTrack;
        if (cur != current)
            page = cur / kPerPage; // follow the current track
        current = cur;
        ctx.engine.withProject ([&] (Project& p) {
            const auto& seq = p.sequences[(size_t) seqIdx];
            for (int k = 0; k < kPerPage; ++k)
            {
                const auto& tr = seq.tracks[(size_t) (page * kPerPage + k)];
                const juce::String out = tr.sendsMidi() ? (tr.internalSound ? "+MIDI " : "MIDI ") + juce::String (tr.midiChannel) : juce::String();
                info[(size_t) k] = { tr.name, (tr.internalSound ? p.programs[(size_t) juce::jlimit (0, kNumPrograms - 1, tr.programIndex)].name + " " : juce::String()) + out,
                                     tr.mute, tr.solo, ! tr.events.empty() };
            }
            for (int pg = 0; pg < kPages; ++pg)
            {
                bool any = false;
                for (int k = 0; k < kPerPage && ! any; ++k)
                    any = ! seq.tracks[(size_t) (pg * kPerPage + k)].events.empty();
                pageUsed[(size_t) pg] = any;
            }
        });
        juce::int64 sig = current * 7 + page * 1000003;
        for (int k = 0; k < kPerPage; ++k)
        {
            level[(size_t) k] = juce::jmax (ctx.engine.getTrackPeak (page * kPerPage + k), level[(size_t) k] * 0.8f);
            const auto& in = info[(size_t) k];
            sig = sig * 131 + in.name.hashCode64() + in.program.hashCode64() * 3 + (in.mute ? 5 : 0) + (in.solo ? 7 : 0)
                  + (in.hasEvents ? 11 : 0);
        }
        for (int pg = 0; pg < kPages; ++pg)
            sig = sig * 3 + (pageUsed[(size_t) pg] ? 1 : 0);
        if (sig != lastSignature)
        {
            lastSignature = sig;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace theme::lcd;
        // page selector: up / down arrows around the page's track range; a dot per page that has notes
        auto sel = pagerArea();
        g.setColour (screenLine);
        g.drawRect (sel, 1.0f);
        auto up = sel.removeFromTop (sel.getHeight() * 0.3f), down = sel.removeFromBottom (up.getHeight());
        auto arrow = [&g] (juce::Rectangle<float> r, bool pointUp, bool enabled) {
            const auto c = r.getCentre();
            juce::Path p;
            const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.28f;
            if (pointUp) p.addTriangle (c.x - s, c.y + s * 0.6f, c.x + s, c.y + s * 0.6f, c.x, c.y - s * 0.6f);
            else         p.addTriangle (c.x - s, c.y - s * 0.6f, c.x + s, c.y - s * 0.6f, c.x, c.y + s * 0.6f);
            g.setColour (enabled ? theme::lcd::ink : theme::lcd::textDim.withAlpha (0.4f));
            g.fillPath (p);
        };
        arrow (up, true, page > 0);
        arrow (down, false, page < kPages - 1);
        g.setColour (text);
        g.setFont (theme::screenFont (10.0f, true));
        auto mid = sel;
        g.drawText ("TR", mid.removeFromTop (mid.getHeight() * 0.3f).toNearestInt(), juce::Justification::centred);
        g.setFont (theme::screenFont (10.0f));
        g.drawText (juce::String (page * kPerPage + 1) + "-" + juce::String (page * kPerPage + kPerPage),
                    mid.removeFromTop (mid.getHeight() * 0.55f).toNearestInt(), juce::Justification::centred);
        const float dot = juce::jmin (3.0f, mid.getWidth() / (kPages * 1.6f));
        const float x0 = mid.getCentreX() - (kPages - 1) * dot * 0.8f;
        for (int pg = 0; pg < kPages; ++pg)
        {
            auto d = juce::Rectangle<float> (dot, dot).withCentre ({ x0 + pg * dot * 1.6f, mid.getCentreY() });
            g.setColour (pg == page ? ink : (pageUsed[(size_t) pg] ? text : textDim.withAlpha (0.35f)));
            if (pg == page || pageUsed[(size_t) pg])
                g.fillRect (d);
            else
                g.drawRect (d, 0.6f);
        }

        for (int k = 0; k < kPerPage; ++k)
        {
            const int t = page * kPerPage + k;
            auto r = cell (k);
            const auto& in = info[(size_t) k];
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
        auto sel = pagerArea();
        if (sel.contains (e.position))
        {
            // top half pages up, bottom half pages down
            page = juce::jlimit (0, kPages - 1, page + (e.position.y < sel.getCentreY() ? -1 : 1));
            lastSignature = -1;
            refreshData();
            return;
        }
        for (int k = 0; k < kPerPage; ++k)
        {
            const int t = page * kPerPage + k;
            auto r = cell (k);
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
                if (ctx.engine.getRecordMode() != RecordMode::None)
                    ctx.engine.ensureRecordable (seqIdx, t);
            }
            ctx.changed();
            refreshData();
            return;
        }
    }

    /** Meters for the 8 tracks on the current page. */
    juce::Rectangle<int> meterRect (int k) const
    {
        auto r = cell (k);
        return r.removeFromBottom (3.0f).reduced (2.0f, 0.0f).toNearestInt();
    }
    float meterLevel (int k) const { return std::round (juce::jmin (1.0f, level[(size_t) k]) * 40.0f) / 40.0f; }

private:
    juce::Rectangle<float> pagerArea() const { return getLocalBounds().toFloat().removeFromRight (34.0f).reduced (1.0f, 2.0f); }
    juce::Rectangle<float> cell (int k) const
    {
        auto area = getLocalBounds().toFloat().withTrimmedRight (36.0f);
        const float w = area.getWidth() / 4.0f, h = area.getHeight() / 2.0f;
        return juce::Rectangle<float> ((k % 4) * w, (k / 4) * h, w, h).reduced (2.0f);
    }

    struct Info
    {
        juce::String name, program;
        bool mute = false, solo = false, hasEvents = false;
    };
    AppContext& ctx;
    std::array<Info, kPerPage> info;
    std::array<float, kPerPage> level {};
    std::array<bool, kPages> pageUsed {};
    int current = -1, page = 0;
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

        setupCombo (bars, { "1", "2", "3", "4", "8", "16", "32", "64", "128", "256", "999" });
        bars.setEditableText (true); // any length 1-999: type it
        bars.onChange = [this] {
            if (loading)
                return;
            const int b = juce::jlimit (1, kMaxBars, bars.getText().getIntValue());
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
                    e.tick = ((timing::quantize (e.tick, div.ticks, div.swingable, s.swing, s.tcStrength, s.legacy96) + s.shiftTicks) % L + L) % L;
                std::stable_sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.tick < b.tick; });
                seq.tracks[(size_t) t].events.assign (evs.begin(), evs.end());
            });
            flash ("Quantized to " + juce::String (div.name) + " @ " + juce::String ((int) s.swing) + "% swing");
        };
        doubleBtn.onClick = [this] {
            ctx.engine.pushUndo ("double");
            bool ok = true;
            editSequence ([&ok] (Sequence& s) {
                if (s.bars * 2 > kMaxBars) { ok = false; return; }
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
            flash (ok ? "Sequence doubled" : "Max 999 bars");
        };

        // track output: the program's sounds, a MIDI channel, or both (like drum tracks that also send MIDI)
        {
            juce::StringArray outs { "DRUM (internal)" };
            for (int ch = 1; ch <= 16; ++ch)
                outs.add ("MIDI ch " + juce::String (ch));
            for (int ch = 1; ch <= 16; ++ch)
                outs.add ("DRUM + MIDI " + juce::String (ch));
            setupCombo (outBox, outs);
            outBox.setTooltip ("Where this track plays: Padlab's sounds, an external MIDI instrument (pick the MIDI output in DISK > Audio/MIDI setup), or both");
            outBox.onChange = [this] {
                if (loading)
                    return;
                const int id = outBox.getSelectedId(), t = ctx.currentTrack();
                editSequence ([id, t] (Sequence& s) {
                    auto& tr = s.tracks[(size_t) t];
                    tr.midiChannel = id <= 1 ? 0 : (id <= 17 ? id - 1 : id - 17);
                    tr.internalSound = id <= 1 || id > 17;
                });
                ctx.changed();
            };
        }
        editBtn.setTooltip ("EDIT: insert / delete / copy bars, copy sequences and tracks, edit ranges of notes, loop");
        editBtn.onClick = [this] {
            if (ctx.showScreen)
                ctx.showScreen (ScreenMode::Edit);
        };

        for (auto* comp : std::initializer_list<juce::Component*> { &seqBox, &bpm, &bars, &timeSig, &programBox, &tc, &swing,
                                                                    &repeatRate, &legacy96, &metronome, &countIn, &clearBtn,
                                                                    &quantizeBtn, &doubleBtn, &grid, &tracks, &outBox, &editBtn })
            addAndMakeVisible (comp);
    }

    juce::String getTitle() const override { return "MAIN"; }
    juce::String getHelpTopic() const override { return "Recording a beat"; }

    void getLiveOverlay (double ticks, bool playing, LiveOverlay& o) override
    {
        if (playing)
            o.playhead = grid.playheadRect (ticks) + grid.getPosition();
        for (int k = 0; k < TrackStrip::kPerPage; ++k)
            o.meters.push_back ({ tracks.meterRect (k) + tracks.getPosition(), tracks.meterLevel (k), false });
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
        {
            int ch = 0;
            bool internal = true;
            ctx.engine.withProject ([&] (Project& p) {
                const auto& tr = p.sequences[(size_t) seqIdx].tracks[(size_t) s.currentTrack];
                ch = tr.midiChannel;
                internal = tr.internalSound;
            });
            outBox.setSelectedId (ch == 0 ? 1 : (internal ? 17 + ch : 1 + ch), juce::dontSendNotification);
        }
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
        outBox.setBounds (row2.removeFromLeft (juce::jmin (150, row2.getWidth() - 330)).reduced (2, 0));
        editBtn.setBounds (row2.removeFromRight (64));
        row2.removeFromRight (4);
        doubleBtn.setBounds (row2.removeFromRight (70));
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
    juce::TextButton clearBtn { "Clear track" }, quantizeBtn { "Quantize" }, doubleBtn { "Double" }, editBtn { "EDIT" };
    juce::ComboBox outBox;
    bool loading = false;
};
} // namespace

std::unique_ptr<Screen> createMainScreen (AppContext& c) { return std::make_unique<MainScreen> (c); }

} // namespace padlab
