#include "core/SampleOps.h"
#include "core/Sampler.h"
#include <juce_core/juce_core.h>

using namespace padlab;

// Fixtures: 1 s mono 44.1 kHz, a single click at exactly 0.5 s (frame 22050) and a 220 Hz
// tone burst from 0.7 to 0.9 s; encoded with LAME (MP3) and ffmpeg (AAC/M4A).
static juce::File fixture (const char* name) { return juce::File (PADLAB_TEST_FIXTURES).getChildFile (name); }

struct SampleOpsTests : juce::UnitTest
{
    SampleOpsTests() : juce::UnitTest ("Audio import", "padlab") {}

    static int peakFrame (const Sample& s, int from, int to)
    {
        int best = from;
        float bestV = 0.0f;
        for (int i = from; i < juce::jmin (to, s.buffer.getNumSamples()); ++i)
            if (std::abs (s.buffer.getSample (0, i)) > bestV)
            {
                bestV = std::abs (s.buffer.getSample (0, i));
                best = i;
            }
        return best;
    }

    void runTest() override
    {
        struct Case { const char* name; double rate; int channels; bool exactLength; };
        for (auto c : { Case { "click.mp3", 44100, 1, true },                // LAME, MPEG-1
                        Case { "click-48k-stereo.mp3", 48000, 2, true },     // ffmpeg/libmp3lame, stereo
                        Case { "click-22k.mp3", 22050, 1, false },           // MPEG-2, no gapless header
                        Case { "click.m4a", 44100, 1, true } })             // AAC
        {
            beginTest (juce::String ("converts ") + c.name + " to WAV audio with the click in place");
            const auto f = fixture (c.name);
            expect (f.existsAsFile(), "fixture missing: " + f.getFullPathName());
            expect (sampleops::isSupportedAudioFile (f));
            expect (sampleops::isCompressedAudioFile (f));
            auto s = sampleops::loadFile (f);
            expect (s != nullptr, "decoder failed");
            if (s == nullptr)
                continue;
            expectWithinAbsoluteError (s->sampleRate, c.rate, 0.5);
            expectEquals (s->buffer.getNumChannels(), c.channels);
            const int expectedClick = (int) (c.rate / 2);
            const int click = peakFrame (*s, 0, (int) (c.rate * 0.68));
            const int frames = s->buffer.getNumSamples();
            logMessage (juce::String (c.name) + ": " + juce::String (frames) + " frames, click at " + juce::String (click)
                        + " (expected " + juce::String (expectedClick) + ")");
            if (c.exactLength)
                expect (std::abs (click - expectedClick) <= 2, "click at " + juce::String (click));
            else // no gapless header: the leading 0.5 s is real silence, so the encoder delay can't be removed
                expect (click >= expectedClick && click <= expectedClick + 2 * 1152, "click at " + juce::String (click));
            if (c.exactLength)
                expectEquals (frames, (int) c.rate, "gapless length");
            else
                expect (frames >= (int) c.rate && frames <= (int) c.rate + 2 * 1152, "length " + juce::String (frames));
            // the tone burst survived
            const float burst = s->buffer.getMagnitude (0, (int) (0.75 * c.rate), (int) (0.1 * c.rate));
            expect (burst > 0.15f && burst < 0.35f, "burst " + juce::String (burst));

            // ...and it's stored like any WAV: round-trips through the project's 24-bit writer
            auto tmp = juce::File::createTempFile (".wav");
            expect (sampleops::writeWav (*s, tmp));
            auto back = sampleops::loadFile (tmp);
            expect (back != nullptr && back->buffer.getNumSamples() == frames);
            tmp.deleteFile();
        }

        beginTest ("MP3 gapless header");
        expectEquals ((int) sampleops::mp3GaplessLength (fixture ("click.mp3")), 44100);
        expectEquals ((int) sampleops::mp3GaplessLength (fixture ("click-48k-stereo.mp3")), 48000);
        expectEquals ((int) sampleops::mp3GaplessLength (fixture ("click-22k.mp3")), -1);
        expectEquals ((int) sampleops::mp3GaplessLength (fixture ("click.m4a")), -1);
        expectEquals ((int) sampleops::mp3GaplessLength (fixture ("kick.mp3")), 17640);

        for (auto name : { "kick.mp3", "kick-22k-noheader.mp3" })
        {
            beginTest (juce::String ("an MP3 kick replaces the kit's kick and plays on time: ") + name);
            auto kick = sampleops::loadFile (fixture (name));
            expect (kick != nullptr);
            if (kick == nullptr)
                continue;
            // it starts on its transient, not after the encoder's leading silence
            // (5% of peak: the source kick crosses that within its first couple of samples)
            const float peak = sampleops::getPeak (*kick);
            int onset = 0;
            while (onset < kick->buffer.getNumSamples() && std::abs (kick->buffer.getSample (0, onset)) <= 0.05f * peak)
                ++onset;
            const double onsetMs = onset * 1000.0 / kick->sampleRate;
            logMessage (juce::String (name) + ": " + juce::String (kick->getLengthSeconds(), 3) + " s, transient at "
                        + juce::String (onsetMs, 2) + " ms");
            expect (onsetMs <= 2.5, "transient at " + juce::String (onsetMs, 2) + " ms");
            // the gapless kick is exactly 0.4 s; without a header the end padding can't be known
            if (juce::String (name) == "kick.mp3")
                expectEquals (kick->buffer.getNumSamples(), 17640);
            else
                expect (kick->getLengthSeconds() >= 0.39 && kick->getLengthSeconds() <= 0.4 + 2 * 1152 / 22050.0);

            Program prog;
            auto& layer = prog.pads[0].layers[0];
            layer.sample = kick;
            Sampler sampler;
            sampler.prepare (48000.0);
            juce::AudioBuffer<float> out (kNumTracks * 2, 4800);
            out.clear();
            sampler.trigger (prog, 0, 0, 127, 0.0f, -1);
            sampler.render (out, 0, 4800);
            expect (out.getMagnitude (0, 0, 144) > 0.2f, "kick hits within 3 ms of the trigger");
            expect (out.getRMSLevel (0, 0, 4800) > 0.1f, "kick body");
        }

        beginTest ("file filters");
        expect (sampleops::isSupportedAudioFile (juce::File ("/x/a.WAV")));
        expect (! sampleops::isSupportedAudioFile (juce::File ("/x/a.txt")));
        expect (! sampleops::isCompressedAudioFile (juce::File ("/x/a.wav")));
        const auto wc = sampleops::supportedFileWildcard();
        expect (wc.contains ("*.wav") && wc.contains ("*.mp3") && wc.contains ("*.m4a"), wc);
    }
};

static SampleOpsTests sampleOpsTests;
