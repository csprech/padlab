#include "core/Effects.h"
#include "core/Sampler.h"
#include "core/SeqEdit.h"
#include <juce_core/juce_core.h>

using namespace padlab;

namespace
{
NoteEvent ev (int64_t tick, int pad, int vel = 100)
{
    NoteEvent e;
    e.tick = tick;
    e.pad = (int16_t) pad;
    e.velocity = (uint8_t) vel;
    return e;
}
constexpr int64_t bar = 4 * kPPQN;
} // namespace

struct SeqEditTests : juce::UnitTest
{
    SeqEditTests() : juce::UnitTest ("Sequence edit / effects", "padlab") {}

    void runTest() override
    {
        beginTest ("insert bars shifts later events and lengthens the sequence");
        {
            Sequence s;
            s.bars = 2;
            s.tracks[0].insertSorted (ev (100, 1));
            s.tracks[5].insertSorted (ev (bar + 10, 2));
            expect (seqedit::insertBars (s, 1, 2));
            expectEquals (s.bars, 4);
            expectEquals ((int) s.tracks[0].events[0].tick, 100);
            expectEquals ((int) s.tracks[5].events[0].tick, (int) (3 * bar + 10));
        }
        beginTest ("delete bars removes their events and closes the gap");
        {
            Sequence s;
            s.bars = 4;
            s.tracks[0].insertSorted (ev (10, 1));
            s.tracks[0].insertSorted (ev (bar + 10, 2));
            s.tracks[0].insertSorted (ev (3 * bar + 10, 3));
            expect (seqedit::deleteBars (s, 1, 2));
            expectEquals (s.bars, 2);
            expectEquals ((int) s.tracks[0].events.size(), 2);
            expectEquals ((int) s.tracks[0].events[1].tick, (int) (bar + 10));
            expect (! seqedit::deleteBars (s, 0, 1), "can't delete every bar");
        }
        beginTest ("copy bars inserts copies at the destination");
        {
            Sequence s;
            s.bars = 2;
            s.tracks[2].insertSorted (ev (0, 7));
            s.tracks[2].insertSorted (ev (bar + 480, 8));
            expect (seqedit::copyBars (s, 0, 1, s, 2, 2)); // bars 1-2 appended twice
            expectEquals (s.bars, 6);
            expectEquals ((int) s.tracks[2].events.size(), 6);
            expectEquals ((int) s.tracks[2].events[2].tick, (int) (2 * bar));
            expectEquals ((int) s.tracks[2].events[5].tick, (int) (5 * bar + 480));
        }
        beginTest ("event range edit: velocity, move, shift pads, delete");
        {
            Track t;
            t.insertSorted (ev (0, 1, 100));
            t.insertSorted (ev (960, 2, 100));
            t.insertSorted (ev (1920, 1, 100));
            seqedit::Range r;
            r.fromTick = 0;
            r.toTick = 1500;
            expectEquals (seqedit::editEvents (t, r, seqedit::Op::VelocityPercent, 50, bar), 2);
            expectEquals ((int) t.events[0].velocity, 50);
            expectEquals ((int) t.events[2].velocity, 100);
            seqedit::Range pad1;
            pad1.padLo = pad1.padHi = 1;
            expectEquals (seqedit::editEvents (t, pad1, seqedit::Op::Move, -240, bar), 2);
            expectEquals ((int) t.events.back().tick, (int) (bar - 240)); // tick 0 wrapped to the end
            expectEquals (seqedit::editEvents (t, pad1, seqedit::Op::ShiftPads, 4, bar), 2);
            expectEquals (seqedit::editEvents (t, seqedit::Range {}, seqedit::Op::Delete, 0, bar), 3);
            expect (t.events.empty());
        }
        beginTest ("pad sends reach the reverb and delay buses");
        {
            auto smp = std::make_shared<Sample>();
            smp->sampleRate = 48000.0;
            smp->buffer.setSize (1, 2000);
            for (int i = 0; i < 2000; ++i)
                smp->buffer.setSample (0, i, 0.5f);
            Program prog;
            prog.pads[0].layers[0].sample = smp;
            prog.pads[0].reverbSend = 0.5f;
            Sampler s;
            s.prepare (48000.0);
            juce::AudioBuffer<float> out (kMixChannels, 1000);
            out.clear();
            s.trigger (prog, 3, 0, 127, 0.0f, -1);
            s.render (out, 0, 1000);
            const float dry = out.getRMSLevel (6, 100, 800), rev = out.getRMSLevel (kFxChannelBase, 100, 800);
            expect (dry > 0.1f);
            expectWithinAbsoluteError (rev, dry * 0.5f, dry * 0.02f);
            expect (out.getRMSLevel (kFxChannelBase + 2, 100, 800) == 0.0f, "no delay send");
        }
        beginTest ("delay echoes on the beat division and decays by the feedback");
        {
            Effects fx;
            fx.prepare (48000.0);
            FxSettings set;
            set.delayDivision = 0; // 1/4
            set.delayFeedback = 0.5f;
            set.delayReturnDb = 0.0f;
            set.reverbReturnDb = -100.0f;
            fx.setParameters (set, 120.0); // quarter = 0.5 s = 24000 samples
            const int n = 80000;
            std::vector<float> zero ((size_t) n, 0.0f), impulse ((size_t) n, 0.0f), outL ((size_t) n, 0.0f), outR ((size_t) n, 0.0f);
            impulse[0] = 1.0f;
            fx.process (zero.data(), zero.data(), impulse.data(), impulse.data(), outL.data(), outR.data(), n);
            auto peakNear = [&] (int at) {
                float p = 0.0f;
                for (int i = at - 4; i <= at + 4; ++i)
                    p = std::max (p, std::abs (outL[(size_t) i]));
                return p;
            };
            expectWithinAbsoluteError ((double) fx.getDelaySamples(), 24000.0, 1.0);
            const float e1 = peakNear (24000), e2 = peakNear (48000);
            expect (e1 > 0.9f, "first echo " + juce::String (e1));
            expect (e2 > 0.15f && e2 < e1 * 0.6f, "second echo quieter " + juce::String (e2));
            expect (peakNear (12000) < 1.0e-4f, "silence between echoes");
        }
        beginTest ("reverb leaves a decaying tail");
        {
            Effects fx;
            fx.prepare (48000.0);
            FxSettings set;
            set.delayReturnDb = -100.0f;
            set.reverbReturnDb = 0.0f;
            fx.setParameters (set, 120.0);
            const int n = 48000;
            std::vector<float> zero ((size_t) n, 0.0f), impulse ((size_t) n, 0.0f), outL ((size_t) n, 0.0f), outR ((size_t) n, 0.0f);
            for (int i = 0; i < 200; ++i)
                impulse[(size_t) i] = 0.5f;
            fx.process (impulse.data(), impulse.data(), zero.data(), zero.data(), outL.data(), outR.data(), n);
            auto rms = [&] (int from) {
                double a = 0;
                for (int i = from; i < from + 2000; ++i)
                    a += outL[(size_t) i] * outL[(size_t) i];
                return std::sqrt (a / 2000.0);
            };
            expect (rms (4000) > 1.0e-3, "tail present");
            expect (rms (40000) < rms (4000), "tail decays");
        }
    }
};

static SeqEditTests seqEditTests;
