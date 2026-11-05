#include "../src/core/MidiClock.h"
#include <juce_core/juce_core.h>

using namespace padlab;

struct MidiClockTests : juce::UnitTest
{
    MidiClockTests() : juce::UnitTest ("MIDI clock", "padlab") {}

    static int count (const std::vector<MidiClock::Event>& v, MidiClock::Msg m)
    {
        int n = 0;
        for (auto& e : v)
            n += e.type == m ? 1 : 0;
        return n;
    }

    void runTest() override
    {
        beginTest ("24 clocks per beat after Start, Stop at the end");
        {
            MidiClock c;
            std::vector<MidiClock::Event> out;
            c.update (true, 0.0, 0.0, out);
            expect (! out.empty() && out[0].type == MidiClock::Msg::Start);
            for (double t = 1.0; t < 960.0 * 4; t += 7.3) // uneven timer steps over 4 beats
                c.update (true, t, 0.0, out);
            c.update (true, 960.0 * 4, 0.0, out); // exactly the downbeat of beat 5
            expectEquals (count (out, MidiClock::Msg::Clock), 4 * 24 + 1);
            c.update (false, 3900.0, 0.0, out);
            expect (out.back().type == MidiClock::Msg::Stop);
        }
        beginTest ("starting mid-sequence sends Song Position then Continue");
        {
            MidiClock c;
            std::vector<MidiClock::Event> out;
            c.update (true, 0.0, 960.0 * 4, out); // bar 2 in 4/4
            expect (out.size() >= 2 && out[0].type == MidiClock::Msg::SongPosition && out[1].type == MidiClock::Msg::Continue);
            expectEquals (out[0].songPosition, 16); // 16 sixteenths
        }
        beginTest ("clocks keep counting across sequence loops");
        {
            MidiClock c;
            std::vector<MidiClock::Event> out;
            c.update (true, 0.0, 0.0, out);
            for (double t = 0.0; t <= 3840.0 * 3; t += 10.0) // running tick count, 3 one-bar loops
                c.update (true, t, 0.0, out);
            expectEquals (count (out, MidiClock::Msg::Clock), 3 * 4 * 24 + 1);
        }
    }
};

static MidiClockTests midiClockTests;
