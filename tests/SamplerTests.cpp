#include "core/Sampler.h"
#include <juce_core/juce_core.h>

using namespace padlab;

namespace
{
SamplePtr constantSample (int frames, float value = 0.5f, double sr = 48000.0)
{
    auto s = std::make_shared<Sample>();
    s->name = "const";
    s->sampleRate = sr;
    s->buffer.setSize (1, frames);
    for (int i = 0; i < frames; ++i)
        s->buffer.setSample (0, i, value);
    return s;
}

float rms (const juce::AudioBuffer<float>& b, int ch, int start, int num)
{
    return b.getRMSLevel (ch, start, num);
}
} // namespace

struct SamplerTests : juce::UnitTest
{
    SamplerTests() : juce::UnitTest ("Sampler", "padlab") {}

    void runTest() override
    {
        constexpr double sr = 48000.0;
        juce::AudioBuffer<float> out (kNumTracks * 2, 4800);

        {
            beginTest ("a pad plays its sample into its track's channels, and stops at the end");
            Sampler s;
            s.prepare (sr);
            Program prog;
            prog.pads[0].layers[0].sample = constantSample (1000);
            out.clear();
            s.trigger (prog, 2, 0, 127, 0.0f, -1);
            expectEquals (s.getActiveVoiceCount(), 1);
            s.render (out, 0, 2000);
            expect (rms (out, 4, 100, 800) > 0.1f, "track 3 left has signal");
            expect (rms (out, 0, 0, 2000) == 0.0f, "track 1 silent");
            expect (rms (out, 4, 1100, 800) == 0.0f, "silent after sample end");
            expectEquals (s.getActiveVoiceCount(), 0);
        }
        {
            beginTest ("+12 semitones plays twice as fast");
            Sampler s;
            s.prepare (sr);
            Program prog;
            prog.pads[0].layers[0].sample = constantSample (1000);
            prog.pads[0].tuneSemis = 12.0f;
            out.clear();
            s.trigger (prog, 0, 0, 127, 0.0f, -1);
            s.render (out, 0, 2000);
            expect (rms (out, 0, 100, 350) > 0.1f);
            expect (rms (out, 0, 520, 400) == 0.0f);
        }
        {
            beginTest ("velocity layers pick the matching layer");
            Sampler s;
            s.prepare (sr);
            Program prog;
            prog.pads[0].layers[0].sample = constantSample (2000, 0.2f);
            prog.pads[0].layers[0].velEnd = 63;
            prog.pads[0].layers[1].sample = constantSample (2000, 0.8f);
            prog.pads[0].layers[1].velStart = 64;
            s.trigger (prog, 0, 0, 30, 0.0f, -1);
            expectEquals (s.getActiveVoiceCount(), 1);
            s.trigger (prog, 0, 0, 100, 0.0f, -1);
            expectEquals (s.getActiveVoiceCount(), 2);
        }
        {
            beginTest ("mute group chokes other pads in the group (open/closed hat)");
            Sampler s;
            s.prepare (sr);
            Program prog;
            prog.pads[0].layers[0].sample = constantSample (48000);
            prog.pads[1].layers[0].sample = constantSample (48000);
            prog.pads[0].muteGroup = prog.pads[1].muteGroup = 1;
            out.clear();
            s.trigger (prog, 0, 0, 127, 0.0f, -1);
            s.render (out, 0, 100);
            s.trigger (prog, 0, 1, 127, 0.0f, -1);
            s.render (out, 100, 1000);
            expectEquals (s.getActiveVoiceCount(), 1);
        }
        {
            beginTest ("mono pads retrigger instead of stacking; poly pads stack");
            Sampler s;
            s.prepare (sr);
            Program prog;
            prog.pads[0].layers[0].sample = constantSample (48000);
            prog.pads[0].poly = PolyMode::Mono;
            out.clear();
            s.trigger (prog, 0, 0, 127, 0.0f, -1);
            s.render (out, 0, 10);
            s.trigger (prog, 0, 0, 127, 0.0f, -1);
            s.render (out, 10, 1000);
            expectEquals (s.getActiveVoiceCount(), 1);

            prog.pads[0].poly = PolyMode::Poly;
            s.killAll();
            s.trigger (prog, 0, 0, 127, 0.0f, -1);
            s.trigger (prog, 0, 0, 127, 0.0f, -1);
            expectEquals (s.getActiveVoiceCount(), 2);
        }
        {
            beginTest ("note-on pads release after their length");
            Sampler s;
            s.prepare (sr);
            Program prog;
            prog.pads[0].layers[0].sample = constantSample (48000);
            prog.pads[0].trigger = TriggerMode::NoteOn;
            prog.pads[0].releaseMs = 5.0f;
            out.clear();
            s.trigger (prog, 0, 0, 127, 0.0f, 480);
            s.render (out, 0, 2000);
            expectEquals (s.getActiveVoiceCount(), 0);
            expect (rms (out, 0, 100, 300) > 0.1f);
        }
        {
            beginTest ("voice pool steals the oldest voice when full");
            Sampler s;
            s.prepare (sr);
            Program prog;
            prog.pads[0].layers[0].sample = constantSample (48000);
            for (int i = 0; i < kMaxVoices + 10; ++i)
                s.trigger (prog, 0, 0, 127, 0.0f, -1);
            expectEquals (s.getActiveVoiceCount(), kMaxVoices);
        }
        {
            beginTest ("slice points play only the slice");
            Sampler s;
            s.prepare (sr);
            Program prog;
            prog.pads[0].layers[0].sample = constantSample (10000);
            prog.pads[0].layers[0].sliceStart = 2000;
            prog.pads[0].layers[0].sliceEnd = 2500;
            out.clear();
            s.trigger (prog, 0, 0, 127, 0.0f, -1);
            s.render (out, 0, 1000);
            expect (rms (out, 0, 50, 400) > 0.1f);
            expect (rms (out, 0, 510, 400) == 0.0f);
        }
        {
            beginTest ("note variation: decay and attack change the envelope of that note only");
            Program prog;
            prog.pads[0].layers[0].sample = constantSample (4800);
            prog.pads[0].sustain = 0.0f;
            prog.pads[0].decayMs = 40.0f;
            auto levelAt = [&] (NoteVariation var, int from) {
                Sampler s;
                s.prepare (sr);
                out.clear();
                s.trigger (prog, 0, 0, 127, 0.0f, -1, var);
                s.render (out, 0, 4800);
                return rms (out, 0, from, 200);
            };
            const float plain = levelAt ({}, 1500);
            const float longer = levelAt ({ VariationType::Decay, 127 }, 1500);
            const float shorter = levelAt ({ VariationType::Decay, 0 }, 1500);
            expect (longer > plain * 2.0f, "decay x4 rings longer");
            expect (shorter < plain * 0.5f, "decay x0.25 dies sooner");
            const float attackStart = levelAt ({ VariationType::Attack, 127 }, 0);
            expect (attackStart < levelAt ({}, 0) * 0.2f, "slow attack fades in");
        }
    }
};

static SamplerTests samplerTests;
