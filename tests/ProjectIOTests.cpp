#include "core/DrumSynth.h"
#include "core/ProjectIO.h"
#include <juce_core/juce_core.h>

using namespace padlab;

struct ProjectIOTests : juce::UnitTest
{
    ProjectIOTests() : juce::UnitTest ("Project save/load", "padlab") {}

    void runTest() override
    {
        beginTest ("factory project round-trips through disk");
        auto pStore = std::make_unique<Project>(); // ~1 MB each: keep them off the stack
        auto& p = *pStore;
        drumsynth::loadFactoryProject (p);
        p.name = "RoundTrip";
        p.programs[0].pads[5].tuneSemis = -3.5f;
        p.programs[0].pads[5].filter = FilterType::LowPass;
        p.programs[0].pads[5].layers[0].sliceStart = 100;
        p.sequences[3].bpm = 87.5;
        p.sequences[3].tracks[2].mute = true;
        NoteEvent e;
        e.tick = 1234;
        e.pad = 17;
        e.velocity = 66;
        e.length = 99;
        e.tune = 2.0f;
        e.variation = { VariationType::Attack, 99 };
        p.sequences[3].tracks[2].insertSorted (e);
        p.vintageMode = 1;
        p.programs[20].name = "Program twenty-one";
        p.programs[20].pads[3].reverbSend = 0.25f;
        p.programs[20].pads[3].delaySend = 0.75f;
        p.programs[20].pads[3].layers[0].sample = p.samplePool.front();
        p.sequences[90].bars = 120;
        p.sequences[90].loop = false;
        p.sequences[90].loopToBar = 7;
        p.sequences[90].tracks[50].midiChannel = 10;
        p.sequences[90].tracks[50].internalSound = false;
        p.sequences[90].tracks[50].insertSorted (e);
        p.fx.delayDivision = 2;
        p.fx.reverbSize = 0.8f;
        p.songs[2].name = "Verse/Chorus";
        p.songs[2].insertStep (0, { 3, 4 });
        p.songs[2].insertStep (1, { 5, 2 });
        p.songs[2].loop = true;
        p.songs[2].loopToStep = 1;

        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("padlab-test-" + juce::String (juce::Random::getSystemRandom().nextInt()))
                       .withFileExtension ("padlab");
        juce::String err;
        expect (projectio::save (p, dir, err), err);
        expect (dir.getChildFile ("project.json").existsAsFile());
        expectEquals (dir.getChildFile ("samples").getNumberOfChildFiles (juce::File::findFiles), (int) p.samplePool.size());

        auto qStore = std::make_unique<Project>();
        auto& q = *qStore;
        expect (projectio::load (dir, q, err), err);
        expectEquals (q.name, juce::String ("RoundTrip"));
        expectEquals ((int) q.samplePool.size(), (int) p.samplePool.size());
        expectEquals (q.programs[0].name, p.programs[0].name);
        expectEquals (q.programs[0].pads[5].tuneSemis, -3.5f);
        expect (q.programs[0].pads[5].filter == FilterType::LowPass);
        expectEquals (q.programs[0].pads[5].layers[0].sliceStart, 100);
        expect (q.programs[0].pads[0].layers[0].sample != nullptr);
        expectEquals (q.programs[0].pads[0].layers[0].sample->name, juce::String ("Kick"));
        expectEquals (q.programs[0].pads[2].muteGroup, 1);
        expectEquals (q.sequences[3].bpm, 87.5);
        expect (q.sequences[3].tracks[2].mute);
        expectEquals ((int) q.sequences[0].tracks[0].events.size(), (int) p.sequences[0].tracks[0].events.size());
        const auto& ev = q.sequences[3].tracks[2].events;
        expectEquals ((int) ev.size(), 1);
        if (! ev.empty())
        {
            expectEquals ((int) ev[0].tick, 1234);
            expectEquals ((int) ev[0].pad, 17);
            expectEquals ((int) ev[0].velocity, 66);
            expectEquals ((int) ev[0].length, 99);
            expectEquals (ev[0].tune, 2.0f);
            expect (ev[0].variation == NoteVariation { VariationType::Attack, 99 });
        }
        expectEquals (q.vintageMode, 1);
        expectEquals (q.programs[20].name, juce::String ("Program twenty-one"));
        expectEquals (q.programs[20].pads[3].reverbSend, 0.25f);
        expectEquals (q.programs[20].pads[3].delaySend, 0.75f);
        expectEquals (q.sequences[90].bars, 120);
        expect (! q.sequences[90].loop);
        expectEquals (q.sequences[90].loopToBar, 7);
        expectEquals (q.sequences[90].tracks[50].midiChannel, 10);
        expect (! q.sequences[90].tracks[50].internalSound);
        expectEquals ((int) q.sequences[90].tracks[50].events.size(), 1);
        expectEquals (q.fx.delayDivision, 2);
        expectEquals (q.fx.reverbSize, 0.8f);
        expectEquals (q.sequences[90].tracks[49].programIndex, 49 % kNumPrograms); // untouched tracks keep defaults
        expectEquals (q.songs[2].name, juce::String ("Verse/Chorus"));
        expectEquals (q.songs[2].numSteps, 2);
        expectEquals (q.songs[2].steps[1].sequence, 5);
        expectEquals (q.songs[2].steps[0].repeats, 4);
        expect (q.songs[2].loop);
        expectEquals (q.songs[2].loopToStep, 1);
        expectEquals (q.songs[0].numSteps, 0);
        const auto& a = p.programs[0].pads[0].layers[0].sample->buffer;
        const auto& b = q.programs[0].pads[0].layers[0].sample->buffer;
        expectEquals (b.getNumSamples(), a.getNumSamples());
        expectWithinAbsoluteError (b.getSample (0, 500), a.getSample (0, 500), 1.0e-4f);

        beginTest ("rejects files that aren't Padlab projects");
        expect (! projectio::fromJson (juce::JSON::parse ("{\"format\":\"other\"}"), q, dir, err));

        dir.deleteRecursively();
    }
};

static ProjectIOTests projectIOTests;
