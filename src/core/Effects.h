#pragma once

// The two send effects: a reverb and a tempo-synced stereo delay with damped feedback.
// Allocation-free after prepare(); runs on the audio thread.

#include "core/Project.h"

namespace padlab
{
class Effects
{
public:
    void prepare (double sampleRate);
    void reset();

    /** Call once per block before process(); bpm drives the delay time. */
    void setParameters (const FxSettings& fx, double bpm);

    /** Processes the send buses (reverb L/R, delay L/R) and adds the wet returns to outL/outR. */
    void process (const float* revL, const float* revR, const float* dlyL, const float* dlyR, float* outL, float* outR,
                  int numSamples);

    double getDelaySamples() const { return delaySamples; }

private:
    double sampleRate = 44100.0;
    juce::Reverb reverb;
    juce::AudioBuffer<float> revWork, delayLine;
    int writePos = 0;
    double delaySamples = 22050.0, targetDelay = 22050.0;
    float feedback = 0.35f, reverbGain = 0.6f, delayGain = 0.5f, damp[2] {};
    bool jumpDelay = true; // first parameters after reset: no glide
};
} // namespace padlab
