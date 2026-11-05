#include "../src/core/Chop.h"
#include "../src/core/DrumSynth.h"
#include "../src/core/Sampler.h"
#include <juce_core/juce_core.h>

using namespace padlab;

struct ChopTests : juce::UnitTest
{
    ChopTests() : juce::UnitTest ("Chop", "padlab") {}

    void runTest() override
    {
        constexpr double sr = 44100.0;

        beginTest ("transient detection finds decaying hits within 6 ms");
        {
            Sample s;
            s.sampleRate = sr;
            s.buffer.setSize (1, (int) sr);
            s.buffer.clear();
            juce::Random rng (3);
            const double hitTimes[] = { 0.10, 0.33, 0.55, 0.81 };
            for (double t0 : hitTimes)
            {
                const int start = (int) (t0 * sr);
                for (int i = 0; i < (int) (0.15 * sr) && start + i < s.buffer.getNumSamples(); ++i)
                    s.buffer.addSample (0, start + i, (float) ((rng.nextDouble() * 2 - 1) * std::exp (-i / (0.03 * sr))));
            }
            // low noise floor
            for (int i = 0; i < s.buffer.getNumSamples(); ++i)
                s.buffer.addSample (0, i, (float) ((rng.nextDouble() * 2 - 1) * 0.001));

            auto slices = chop::detectTransients (s, 50.0, 50.0);
            expectEquals ((int) slices.size(), 5, "0 + four hits");
            if (slices.size() == 5)
                for (int i = 0; i < 4; ++i)
                {
                    const double ms = std::abs (slices[(size_t) i + 1] / sr - hitTimes[i]) * 1000.0;
                    expect (ms < 6.0, "hit " + juce::String (i) + " off by " + juce::String (ms, 2) + " ms");
                }
        }

        beginTest ("transient detection on the factory demo break finds the drum hits");
        {
            auto kit = drumsynth::makeFactoryKit();
            auto loop = drumsynth::makeDemoBreak (kit, 90.0);
            auto slices = chop::detectTransients (*loop, 60.0, 60.0);
            logMessage ("demo break slices: " + juce::String ((int) slices.size()));
            expect (slices.size() >= 12 && slices.size() <= 40);
        }

        beginTest ("min slice time suppresses close detections");
        {
            Sample s;
            s.sampleRate = sr;
            s.buffer.setSize (1, (int) sr);
            s.buffer.clear();
            for (double t0 : { 0.2, 0.21, 0.6 })
                for (int i = 0; i < 500; ++i)
                    s.buffer.setSample (0, (int) (t0 * sr) + i, (float) std::sin (i * 0.3) * std::exp (-i / 200.0f));
            auto slices = chop::detectTransients (s, 80.0, 100.0);
            expectEquals ((int) slices.size(), 3);
        }

        beginTest ("equal regions and BPM slicing");
        {
            Sample s;
            s.sampleRate = sr;
            s.buffer.setSize (1, 44100);
            auto r = chop::equalRegions (s, 4);
            expectEquals ((int) r.size(), 4);
            expectEquals (r[1], 11025);
            auto b = chop::byBpm (s, 120.0, 1.0); // 1 s = 2 beats
            expectEquals ((int) b.size(), 2);
            expectEquals (b[1], 22050);
        }

        beginTest ("loop tempo estimate");
        {
            Sample s;
            s.sampleRate = sr;
            s.buffer.setSize (1, (int) (sr * 60.0 / 90.0 * 8)); // two bars at 90
            expectWithinAbsoluteError (chop::estimateLoopBpm (s, 2), 90.0, 0.01);
        }

        beginTest ("chop voicing: mono chops cut each other off, poly chops overlap; other pads untouched");
        {
            auto loop = std::make_shared<Sample>();
            loop->name = "loop";
            loop->sampleRate = sr;
            loop->buffer.setSize (1, (int) sr * 2);
            loop->buffer.clear();
            for (int i = 0; i < loop->buffer.getNumSamples(); ++i)
                loop->buffer.setSample (0, i, 0.5f);
            auto hat = std::make_shared<Sample> (*loop);
            hat->name = "hat";

            Program prog;
            for (int i = 0; i < 4; ++i) // four chops on pads 1-4
            {
                auto& l = prog.pads[(size_t) i].layers[0];
                l.sample = loop;
                l.sliceStart = i * (int) sr / 2;
                l.sliceEnd = i < 3 ? (i + 1) * (int) sr / 2 : -1;
            }
            prog.pads[4].layers[0].sample = loop; // the whole loop on pad 5: not a chop
            prog.pads[5].layers[0].sample = hat;  // an open hi-hat already using mute group 1
            prog.pads[5].muteGroup = 1;

            expectEquals ((int) chop::chopPads (prog, loop.get()).size(), 4, "finds the four chops, not the whole-loop pad");
            expect (chop::getVoicing (prog, loop.get()) == chop::Voicing::Poly);

            expectEquals (chop::setVoicing (prog, loop.get(), chop::Voicing::Mono), 4);
            expect (chop::getVoicing (prog, loop.get()) == chop::Voicing::Mono);
            const int group = prog.pads[0].muteGroup;
            expectEquals (group, 2, "takes a free group, not the hi-hat's");
            for (int i = 0; i < 4; ++i)
            {
                expectEquals (prog.pads[(size_t) i].muteGroup, group);
                expect (prog.pads[(size_t) i].poly == PolyMode::Mono);
            }
            expectEquals (prog.pads[4].muteGroup, 0, "the whole-loop pad is left alone");
            expectEquals (prog.pads[5].muteGroup, 1, "so is the hi-hat");
            chop::setVoicing (prog, loop.get(), chop::Voicing::Mono);
            expectEquals (prog.pads[0].muteGroup, group, "setting mono again keeps the group");

            auto voicesAfterTwoChops = [&prog] {
                Sampler smp;
                smp.prepare (sr);
                juce::AudioBuffer<float> out (kMixChannels, 2048);
                out.clear();
                smp.trigger (prog, 0, 0, 127, 0.0f, -1);
                smp.render (out, 0, 256);
                smp.trigger (prog, 0, 1, 127, 0.0f, -1);
                smp.render (out, 256, 1024); // past the 3 ms choke fade
                return smp.getActiveVoiceCount();
            };
            expectEquals (voicesAfterTwoChops(), 1, "mono: the second chop cuts the first off");

            expectEquals (chop::setVoicing (prog, loop.get(), chop::Voicing::Poly), 4);
            for (int i = 0; i < 4; ++i)
            {
                expectEquals (prog.pads[(size_t) i].muteGroup, 0);
                expect (prog.pads[(size_t) i].poly == PolyMode::Poly);
            }
            expectEquals (voicesAfterTwoChops(), 2, "poly: both chops ring");
            expectEquals (prog.pads[5].muteGroup, 1, "the hi-hat keeps its group");
        }
    }
};

static ChopTests chopTests;
