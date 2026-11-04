#include "core/Transport.h"
#include <juce_core/juce_core.h>

using namespace padlab;

namespace
{
struct Hit
{
    int64_t sample;
    Action action;
};

// Drives a Transport block by block, injecting live inputs at absolute sample times.
struct Harness
{
    static constexpr double sr = 48000.0;
    static constexpr int block = 512;

    std::unique_ptr<Project> projectStore = std::make_unique<Project>(); // ~1 MB: not on the stack
    Project& project = *projectStore;
    PerformanceSettings settings;
    Transport transport;
    ActionList actions;
    int64_t now = 0;
    std::vector<Hit> hits;
    std::vector<std::pair<int64_t, LiveInput>> pending;

    Harness()
    {
        transport.prepare (sr);
        settings.metronome = false;
        settings.countIn = false;
        auto& seq = project.sequences[0];
        seq.bpm = 120.0; // 0.04 ticks per sample -> 25 samples per tick
        seq.bars = 1;
        // the engine reserves room on the track being recorded; do the same here
        for (int s = 0; s < 2; ++s)
            for (int t = 0; t < 3; ++t)
                project.sequences[(size_t) s].tracks[(size_t) t].events.reserve (kEventCapacity);
    }

    void input (int64_t atSample, LiveInput::Type type, int pad, int value = 100)
    {
        LiveInput in;
        in.type = type;
        in.pad = pad;
        in.value = value;
        pending.push_back ({ atSample, in });
    }

    void run (int64_t samples)
    {
        const int64_t end = now + samples;
        while (now < end)
        {
            const int n = (int) std::min<int64_t> (block, end - now);
            std::vector<LiveInput> ins;
            for (auto it = pending.begin(); it != pending.end();)
            {
                if (it->first >= now && it->first < now + n)
                {
                    auto in = it->second;
                    in.offset = (int) (it->first - now);
                    ins.push_back (in);
                    it = pending.erase (it);
                }
                else
                    ++it;
            }
            transport.process (project, settings, ins.data(), (int) ins.size(), n, actions);
            for (int i = 0; i < actions.size; ++i)
                hits.push_back ({ now + actions.items[(size_t) i].offset, actions.items[(size_t) i] });
            now += n;
        }
    }

    std::vector<int64_t> triggersOf (int pad) const
    {
        std::vector<int64_t> v;
        for (auto& h : hits)
            if (h.action.type == Action::Trigger && h.action.pad == pad)
                v.push_back (h.sample);
        return v;
    }

    std::vector<int64_t> clicks() const
    {
        std::vector<int64_t> v;
        for (auto& h : hits)
            if (h.action.type == Action::Click)
                v.push_back (h.sample);
        return v;
    }

    void addEvent (int track, int64_t tick, int pad, int vel = 100)
    {
        NoteEvent e;
        e.tick = tick;
        e.pad = (int16_t) pad;
        e.velocity = (uint8_t) vel;
        project.sequences[0].tracks[(size_t) track].insertSorted (e);
    }

    const std::vector<NoteEvent>& events (int track = 0) const
    {
        return project.sequences[(size_t) transport.getCurrentSequence()].tracks[(size_t) track].events;
    }
};

constexpr int64_t samplesPerTick = 25;
constexpr int64_t barSamples = 3840 * samplesPerTick; // 96000
} // namespace

struct TransportTests : juce::UnitTest
{
    TransportTests() : juce::UnitTest ("Transport / sequencer", "padlab") {}

    static bool near (int64_t a, int64_t b, int64_t tol = 1) { return std::llabs (a - b) <= tol; }

    void runTest() override
    {
        {
            beginTest ("events play at sample-accurate positions and loop");
            Harness h;
            h.addEvent (0, 0, 5);
            h.addEvent (0, 960, 6);
            h.transport.play (true, false);
            h.run (barSamples * 2 + 10);
            auto k = h.triggersOf (5);
            auto s = h.triggersOf (6);
            expectEquals ((int) k.size(), 3);
            expectEquals ((int) s.size(), 2);
            if (k.size() == 3)
            {
                expect (near (k[0], 0));
                expect (near (k[1], barSamples));
                expect (near (k[2], 2 * barSamples));
            }
            if (s.size() == 2)
                expect (near (s[0], 960 * samplesPerTick), "beat 2 at sample 24000, got " + juce::String (s[0]));
        }
        {
            beginTest ("muted and non-solo tracks are silent; pad mute silences one pad");
            Harness h;
            h.addEvent (0, 0, 1);
            h.addEvent (1, 0, 2);
            h.addEvent (2, 0, 3);
            h.project.sequences[0].tracks[1].mute = true;
            h.project.sequences[0].tracks[2].solo = true;
            h.transport.play (true, false);
            h.run (1000);
            expectEquals ((int) h.triggersOf (1).size(), 0);
            expectEquals ((int) h.triggersOf (2).size(), 0);
            expectEquals ((int) h.triggersOf (3).size(), 1);

            Harness m;
            m.addEvent (0, 0, 4);
            m.project.programs[0].pads[4].mute = true;
            m.transport.play (true, false);
            m.run (1000);
            expectEquals ((int) m.triggersOf (4).size(), 0);
        }
        {
            beginTest ("overdub records with timing correct + swing, without double triggering");
            Harness h;
            h.settings.timingCorrect = 3; // 1/16
            h.settings.swing = 66.0;
            h.transport.armRecord (RecordMode::Overdub);
            h.transport.play (true, false);
            h.input (300 * samplesPerTick, LiveInput::PadDown, 9, 90);
            h.input (310 * samplesPerTick, LiveInput::PadUp, 9);
            h.run (barSamples + 40 * samplesPerTick * 10);

            const auto& ev = h.events();
            expectEquals ((int) ev.size(), 1);
            if (! ev.empty())
            {
                expectEquals ((int) ev[0].tick, 317);
                expectEquals ((int) ev[0].velocity, 90);
                expectEquals ((int) ev[0].pad, 9);
                expectEquals ((int) ev[0].length, 10);
            }
            auto t = h.triggersOf (9);
            // once live (at tick 300), not again at 317 in pass 1, then at 317 in pass 2
            expectEquals ((int) t.size(), 2);
            if (t.size() == 2)
            {
                expect (near (t[0], 300 * samplesPerTick));
                expect (near (t[1], barSamples + 317 * samplesPerTick));
            }
        }
        {
            beginTest ("late hit snaps back without being skipped next pass");
            Harness h;
            h.transport.armRecord (RecordMode::Overdub);
            h.transport.play (true, false);
            h.input (250 * samplesPerTick, LiveInput::PadDown, 2);
            h.run (barSamples + 300 * samplesPerTick);
            auto t = h.triggersOf (2);
            expectEquals ((int) t.size(), 2);
            if (t.size() == 2)
                expect (near (t[1], barSamples + 240 * samplesPerTick));
        }
        {
            beginTest ("hit just before the loop end lands on tick 0 of the next pass");
            Harness h;
            h.transport.armRecord (RecordMode::Overdub);
            h.transport.play (true, false);
            h.input (barSamples - 10 * samplesPerTick, LiveInput::PadDown, 7);
            h.run (2 * barSamples + 100);
            expectEquals ((int) h.events().size(), 1);
            if (! h.events().empty())
                expectEquals ((int) h.events()[0].tick, 0);
            auto t = h.triggersOf (7);
            expectEquals ((int) t.size(), 2); // live hit, then start of pass 3 (pass 2 start skipped)
        }
        {
            beginTest ("REC replaces the current track on the first pass, then switches to overdub");
            Harness h;
            h.addEvent (0, 0, 1);
            h.addEvent (0, 1920, 1);
            h.addEvent (1, 0, 3); // other track untouched
            h.transport.armRecord (RecordMode::Record);
            h.transport.play (true, false);
            h.input (960 * samplesPerTick, LiveInput::PadDown, 2);
            h.run (barSamples / 2 + 10);
            expect (h.transport.getRecordMode() == RecordMode::Record);
            h.run (barSamples / 2 + 10);
            expect (h.transport.getRecordMode() == RecordMode::Overdub);
            expectEquals ((int) h.events (0).size(), 1);
            if (! h.events (0).empty())
                expectEquals ((int) h.events (0)[0].pad, 2);
            expectEquals ((int) h.events (1).size(), 1);
            expectEquals ((int) h.triggersOf (1).size(), 0);
        }
        {
            beginTest ("note repeat fires on the swung grid while playing and records");
            Harness h;
            h.settings.noteRepeat = true;
            h.settings.repeatDivision = 3; // 1/16
            h.settings.swing = 60.0;
            h.transport.armRecord (RecordMode::Overdub);
            h.transport.play (true, false);
            h.input (10, LiveInput::PadDown, 4, 80);
            h.input (1000 * samplesPerTick, LiveInput::PadUp, 4);
            h.run (1100 * samplesPerTick);
            auto t = h.triggersOf (4);
            // grid after the press: 288, 480, 768, 960
            expectEquals ((int) t.size(), 4);
            if (t.size() == 4)
            {
                expect (near (t[0], 288 * samplesPerTick));
                expect (near (t[1], 480 * samplesPerTick));
                expect (near (t[2], 768 * samplesPerTick));
            }
            expectEquals ((int) h.events().size(), 4);
        }
        {
            beginTest ("note repeat velocity follows pressure");
            Harness h;
            h.settings.noteRepeat = true;
            h.transport.play (true, false);
            h.input (10, LiveInput::PadDown, 4, 100);
            h.input (500 * samplesPerTick, LiveInput::Pressure, 4, 30);
            h.run (800 * samplesPerTick);
            int lastVel = 0;
            for (auto& hit : h.hits)
                if (hit.action.type == Action::Trigger)
                    lastVel = hit.action.velocity;
            expectEquals (lastVel, 30);
        }
        {
            beginTest ("note repeat while stopped plays immediately, then at tempo");
            Harness h;
            h.settings.noteRepeat = true;
            h.settings.repeatDivision = 1; // 1/8 = 480 ticks
            h.input (100, LiveInput::PadDown, 0);
            h.run (100 + 480 * samplesPerTick * 2 + 10);
            auto t = h.triggersOf (0);
            expectEquals ((int) t.size(), 3);
            if (t.size() == 3)
            {
                expect (near (t[0], 100));
                expect (near (t[1], 100 + 480 * samplesPerTick, 2));
            }
        }
        {
            beginTest ("count-in clicks one bar before playback, then plays tick 0");
            Harness h;
            h.settings.metronome = true;
            h.addEvent (0, 0, 1);
            h.transport.armRecord (RecordMode::Overdub);
            h.transport.play (true, true);
            h.run (barSamples + 100);
            auto c = h.clicks();
            expect (c.size() >= 5);
            auto t = h.triggersOf (1);
            expectEquals ((int) t.size(), 1);
            if (! t.empty())
                expect (near (t[0], barSamples), "first event after count-in, got " + juce::String (t[0]));
        }
        {
            beginTest ("queued sequence switches at the loop end");
            Harness h;
            h.project.sequences[1].bars = 1;
            h.project.sequences[1].bpm = 120.0;
            NoteEvent e;
            e.pad = 11;
            h.project.sequences[1].tracks[0].insertSorted (e);
            h.transport.play (true, false);
            h.run (1000);
            h.transport.queueSequence (1);
            h.run (barSamples);
            expectEquals (h.transport.getCurrentSequence(), 1);
            auto t = h.triggersOf (11);
            expectEquals ((int) t.size(), 1);
            if (! t.empty())
                expect (near (t[0], barSamples));
        }
        {
            beginTest ("16 levels maps pads to velocity steps of the source pad");
            Harness h;
            h.settings.sixteenLevels = true;
            h.settings.sixteenLevelsSource = 3;
            h.input (0, LiveInput::PadDown, 0);
            h.input (10, LiveInput::PadDown, 15);
            h.input (20, LiveInput::PadDown, 16 + 7); // bank B pad 8
            h.run (100);
            auto t = h.triggersOf (3);
            expectEquals ((int) t.size(), 3);
            std::vector<int> vels;
            for (auto& hit : h.hits)
                if (hit.action.type == Action::Trigger)
                    vels.push_back (hit.action.velocity);
            if (vels.size() == 3)
            {
                expectEquals (vels[0], 7);
                expectEquals (vels[1], 127);
                expectEquals (vels[2], 63);
            }
        }
        {
            beginTest ("full level forces velocity 127");
            Harness h;
            h.settings.fullLevel = true;
            h.input (0, LiveInput::PadDown, 2, 20);
            h.run (100);
            expectEquals (h.hits.empty() ? 0 : h.hits[0].action.velocity, 127);
        }
        {
            beginTest ("erase + held pad deletes that pad's notes as they pass");
            Harness h;
            h.addEvent (0, 960, 1);
            h.addEvent (0, 1920, 1);
            h.addEvent (0, 1920, 2);
            h.transport.play (true, false);
            h.settings.eraseHeld = true;
            h.input (10, LiveInput::PadDown, 1);
            h.run (barSamples);
            expectEquals ((int) h.events().size(), 1);
            expectEquals ((int) h.triggersOf (1).size(), 0);
        }
        {
            beginTest ("loop to bar: after the first pass, playback loops from that bar");
            Harness h;
            auto& seq = h.project.sequences[0];
            seq.bars = 3;
            seq.loopToBar = 1; // bars 2-3 loop
            h.addEvent (0, 0, 20);                 // bar 1
            h.addEvent (0, 3840, 21);              // bar 2
            h.transport.play (true, false);
            h.run (barSamples * 5 + 10);
            const auto b1 = h.triggersOf (20), b2 = h.triggersOf (21);
            expectEquals ((int) b1.size(), 1, "bar 1 plays once");
            expectEquals ((int) b2.size(), 3, "bar 2 on every pass");
            if (b2.size() == 3)
            {
                expect (near (b2[0], barSamples) && near (b2[1], 3 * barSamples) && near (b2[2], 5 * barSamples));
            }
        }
        {
            beginTest ("loop off: the sequence plays once and stops");
            Harness h;
            h.project.sequences[0].loop = false;
            h.addEvent (0, 0, 20);
            h.transport.play (true, false);
            h.run (barSamples * 3);
            expectEquals ((int) h.triggersOf (20).size(), 1);
            expect (h.transport.getState() == Transport::State::Stopped);
        }
        {
            beginTest ("recording just before the loop end lands on the loop-to bar");
            Harness h;
            auto& seq = h.project.sequences[0];
            seq.bars = 2;
            seq.loopToBar = 1;
            h.settings.timingCorrect = 3; // 1/16
            h.transport.armRecord (RecordMode::Overdub);
            h.transport.play (true, false);
            // second pass of the loop (bar 2 again) ends at 3 bars of time; hit 10 ticks before it
            h.input (3 * barSamples - 10 * samplesPerTick, LiveInput::PadDown, 4);
            h.run (3 * barSamples + 1000);
            bool found = false;
            for (auto& e : h.events())
                if (e.pad == 4)
                    found = e.tick == 3840; // start of bar 2, not bar 1
            expect (found, "snapped forward into the loop start");
        }
        {
            beginTest ("recording never allocates: no reserved room means the hit plays but isn't recorded");
            Harness h;
            h.project.sequences[0].tracks[5].events.shrink_to_fit();
            h.settings.currentTrack = 5;
            h.transport.armRecord (RecordMode::Overdub);
            h.transport.play (true, false);
            h.input (1000, LiveInput::PadDown, 2);
            h.run (5000);
            expectEquals ((int) h.triggersOf (2).size(), 1);
            expect (h.project.sequences[0].tracks[5].events.empty());
        }
        {
            beginTest ("timing shift moves recorded notes after timing correct");
            Harness h;
            h.settings.timingCorrect = 3; // 1/16
            h.settings.shiftTicks = 12;
            h.transport.armRecord (RecordMode::Overdub);
            h.transport.play (true, false);
            h.input (960 * samplesPerTick + 40 * samplesPerTick, LiveInput::PadDown, 2); // a little after beat 2
            h.run (barSamples / 2);
            bool found = false;
            for (auto& e : h.events())
                if (e.pad == 2)
                    found = e.tick == 960 + 12;
            expect (found, "snapped to beat 2, then 12 ticks late");
        }
        {
            beginTest ("song mode plays each step's sequence its number of repeats, then stops");
            Harness h;
            for (int i : { 0, 1 })
            {
                h.project.sequences[(size_t) i].bars = 1;
                h.project.sequences[(size_t) i].bpm = 120.0;
                NoteEvent e;
                e.pad = (int16_t) (20 + i);
                h.project.sequences[(size_t) i].tracks[0].insertSorted (e);
            }
            auto& song = h.project.songs[0];
            song.insertStep (0, { 0, 2 });  // A x2
            song.insertStep (1, { 1, 1 });  // B x1
            h.transport.playSong (h.project, 0, 0);
            expect (h.transport.isPlayingSong());
            h.run (barSamples * 4);
            const auto a = h.triggersOf (20), b = h.triggersOf (21);
            expectEquals ((int) a.size(), 2, "sequence A twice");
            expectEquals ((int) b.size(), 1, "then B once");
            if (a.size() == 2 && b.size() == 1)
            {
                expect (near (a[0], 0) && near (a[1], barSamples));
                expect (near (b[0], 2 * barSamples));
            }
            expect (h.transport.getState() == Transport::State::Stopped, "stops at the end of the song");
            expect (! h.transport.isPlayingSong());
        }
        {
            beginTest ("song loop jumps back to the loop step");
            Harness h;
            for (int i : { 0, 1 })
            {
                h.project.sequences[(size_t) i].bars = 1;
                h.project.sequences[(size_t) i].bpm = 120.0;
                NoteEvent e;
                e.pad = (int16_t) (20 + i);
                h.project.sequences[(size_t) i].tracks[0].insertSorted (e);
            }
            auto& song = h.project.songs[0];
            song.insertStep (0, { 0, 1 });
            song.insertStep (1, { 1, 1 });
            song.loop = true;
            song.loopToStep = 1;
            h.transport.playSong (h.project, 0, 0);
            h.run (barSamples * 4 + 10);
            expectEquals ((int) h.triggersOf (20).size(), 1, "intro once");
            expectEquals ((int) h.triggersOf (21).size(), 4, "then the loop step forever");
            expect (h.transport.isPlayingSong());
        }
        {
            beginTest ("note variation: the slider's value goes out with the hit and is recorded");
            Harness h;
            h.settings.noteVariationOn = true;
            h.settings.noteVariationType = VariationType::Decay;
            h.settings.noteVariationValue = 20;
            h.settings.noteVariationPad = 4;
            h.settings.timingCorrect = 7; // off
            h.transport.armRecord (RecordMode::Overdub);
            h.transport.play (true, false);
            h.input (100, LiveInput::PadDown, 4);
            h.input (200, LiveInput::PadDown, 5); // not the target pad: unchanged
            h.run (1000);
            int checked = 0;
            for (auto& hit : h.hits)
                if (hit.action.type == Action::Trigger)
                {
                    if (hit.action.pad == 4)
                        expect (hit.action.variation == NoteVariation { VariationType::Decay, 20 });
                    else
                        expect (hit.action.variation.type == VariationType::None);
                    ++checked;
                }
            expectEquals (checked, 2);
            bool recorded = false;
            for (auto& e : h.events())
                if (e.pad == 4)
                    recorded = e.variation == NoteVariation { VariationType::Decay, 20 };
            expect (recorded, "variation stored on the recorded note");

            // tune variation rides in the tune field: 127 -> about +12 semitones
            Harness t;
            t.settings.noteVariationOn = true;
            t.settings.noteVariationType = VariationType::Tune;
            t.settings.noteVariationValue = 0;
            t.input (0, LiveInput::PadDown, 1);
            t.run (100);
            expect (! t.hits.empty() && std::abs (t.hits[0].action.tune + 12.0f) < 0.01f);
        }
        {
            beginTest ("16 levels can spread decay / attack / filter across the pads");
            Harness h;
            h.settings.sixteenLevels = true;
            h.settings.sixteenLevelsSource = 2;
            h.settings.sixteenLevelsParam = 4; // filter
            h.input (0, LiveInput::PadDown, 0);
            h.input (10, LiveInput::PadDown, 15);
            h.run (100);
            std::vector<NoteVariation> vars;
            for (auto& hit : h.hits)
                if (hit.action.type == Action::Trigger)
                    vars.push_back (hit.action.variation);
            expectEquals ((int) vars.size(), 2);
            if (vars.size() == 2)
            {
                expect (vars[0] == NoteVariation { VariationType::Filter, 0 });
                expect (vars[1] == NoteVariation { VariationType::Filter, 127 });
            }
        }
    }
};

static TransportTests transportTests;
