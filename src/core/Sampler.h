#pragma once

// Polyphonic drum sampler: 64-voice pool shared by all tracks, oldest-voice stealing,
// velocity layers, per-pad tune/level/pan, ADSR, state-variable filter, mono mode
// and mute (choke) groups. Allocation-free after prepare().

#include "core/Project.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace padlab
{
class Sampler
{
public:
    void prepare (double sampleRate);

    /** Start a pad. releaseAfter: samples until note-off for Note-On pads; -1 = until release(). */
    void trigger (const Program& program, int track, int pad, int velocity, float tuneOffset, int releaseAfter);

    /** Note-off for held live pads (Note-On trigger mode only). */
    void release (int track, int pad);

    /** Mix all active voices into per-track stereo buffers (channels 2t, 2t+1). */
    void render (juce::AudioBuffer<float>& trackBuffers, int startSample, int numSamples);

    void killAll();
    int getActiveVoiceCount() const;

private:
    struct Voice
    {
        bool active = false;
        int track = -1, pad = -1, muteGroup = 0;
        uint32_t age = 0;
        SamplePtr sample;
        double pos = 0.0, inc = 1.0;
        int endFrame = 0;
        float gainL = 1.0f, gainR = 1.0f;
        juce::ADSR env;
        bool oneShot = true;
        int releaseCountdown = -1;
        int fadeRemaining = -1; // choke fade
        int fadeLength = 1;

        FilterType filter = FilterType::Off;
        float a1 = 0, a2 = 0, a3 = 0, k = 1;
        float ic1[2] {}, ic2[2] {};
    };

    Voice& allocateVoice();
    void choke (int track, int pad, int muteGroup, bool includeSamePad);

    std::array<Voice, kMaxVoices> voices;
    double sampleRate = 44100.0;
    uint32_t ageCounter = 0;
};

} // namespace padlab
