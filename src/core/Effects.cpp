#include "core/Effects.h"
#include "core/Timing.h"

namespace padlab
{
void Effects::prepare (double sr)
{
    sampleRate = sr > 0 ? sr : 44100.0;
    reverb.setSampleRate (sampleRate);
    revWork.setSize (2, 8192);
    delayLine.setSize (2, (int) (sampleRate * 4.0) + 8); // up to 4 s (a 1/2 note at 30 BPM)
    reset();
}

void Effects::reset()
{
    reverb.reset();
    delayLine.clear();
    writePos = 0;
    damp[0] = damp[1] = 0.0f;
    jumpDelay = true;
}

void Effects::setParameters (const FxSettings& fx, double bpm)
{
    juce::Reverb::Parameters p;
    p.roomSize = juce::jlimit (0.0f, 1.0f, fx.reverbSize);
    p.damping = juce::jlimit (0.0f, 1.0f, fx.reverbDamping);
    p.wetLevel = 1.0f;
    p.dryLevel = 0.0f;
    p.width = 1.0f;
    reverb.setParameters (p);
    reverbGain = juce::Decibels::decibelsToGain (fx.reverbReturnDb, -60.0f);

    const auto& divs = timing::divisions();
    const auto& d = divs[(size_t) juce::jlimit (0, timing::kNumDivisions - 2, fx.delayDivision)]; // never "Off"
    const double ticksPerSecond = juce::jmax (1.0, bpm) / 60.0 * kPPQN;
    targetDelay = juce::jlimit (1.0, (double) delayLine.getNumSamples() - 4.0, d.ticks / ticksPerSecond * sampleRate);
    if (jumpDelay)
    {
        delaySamples = targetDelay;
        jumpDelay = false;
    }
    feedback = juce::jlimit (0.0f, 0.92f, fx.delayFeedback);
    delayGain = juce::Decibels::decibelsToGain (fx.delayReturnDb, -60.0f);
}

void Effects::process (const float* revL, const float* revR, const float* dlyL, const float* dlyR, float* outL, float* outR, int n)
{
    // reverb (juce::Reverb works in place, so copy the send first)
    for (int start = 0; start < n; start += revWork.getNumSamples())
    {
        const int num = juce::jmin (revWork.getNumSamples(), n - start);
        revWork.copyFrom (0, 0, revL + start, num);
        revWork.copyFrom (1, 0, revR + start, num);
        reverb.processStereo (revWork.getWritePointer (0), revWork.getWritePointer (1), num);
        for (int i = 0; i < num; ++i)
        {
            outL[start + i] += revWork.getSample (0, i) * reverbGain;
            if (outR != nullptr)
                outR[start + i] += revWork.getSample (1, i) * reverbGain;
        }
    }

    // delay: fractional read, delay time glides to tempo changes, feedback gently low-passed (tape-ish)
    const int size = delayLine.getNumSamples();
    auto* bl = delayLine.getWritePointer (0);
    auto* br = delayLine.getWritePointer (1);
    const float dampCoef = 0.35f;
    for (int i = 0; i < n; ++i)
    {
        delaySamples += (targetDelay - delaySamples) * 0.0005;
        double rp = writePos - delaySamples;
        while (rp < 0)
            rp += size;
        const int i0 = (int) rp, i1 = (i0 + 1) % size;
        const float frac = (float) (rp - i0);
        const float yl = bl[i0] + (bl[i1] - bl[i0]) * frac;
        const float yr = br[i0] + (br[i1] - br[i0]) * frac;
        damp[0] += dampCoef * (yl - damp[0]);
        damp[1] += dampCoef * (yr - damp[1]);
        bl[writePos] = dlyL[i] + damp[0] * feedback;
        br[writePos] = dlyR[i] + damp[1] * feedback;
        writePos = (writePos + 1) % size;
        outL[i] += yl * delayGain;
        if (outR != nullptr)
            outR[i] += yr * delayGain;
    }
}
} // namespace padlab
