#include "core/Sampler.h"
#include <cmath>

namespace padlab
{
namespace
{
    inline float hermite (const float* d, int n, double pos)
    {
        const int i = (int) pos;
        const float t = (float) (pos - i);
        auto at = [d, n] (int idx) { return d[juce::jlimit (0, n - 1, idx)]; };
        const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    inline float dbToGain (float db) { return db <= -96.0f ? 0.0f : std::pow (10.0f, db / 20.0f); }
}

void Sampler::prepare (double sr)
{
    sampleRate = sr > 0 ? sr : 44100.0;
    for (auto& v : voices)
    {
        v.env.setSampleRate (sampleRate);
        v.active = false;
        v.sample.reset();
    }
}

void Sampler::killAll()
{
    for (auto& v : voices)
    {
        v.active = false;
        v.sample.reset();
    }
}

int Sampler::getActiveVoiceCount() const
{
    int n = 0;
    for (auto& v : voices)
        n += v.active ? 1 : 0;
    return n;
}

Sampler::Voice& Sampler::allocateVoice()
{
    Voice* oldest = &voices[0];
    for (auto& v : voices)
    {
        if (! v.active)
            return v;
        if (v.age < oldest->age)
            oldest = &v;
    }
    return *oldest; // steal
}

void Sampler::choke (int track, int pad, int muteGroup, bool includeSamePad)
{
    const int fadeLen = std::max (1, (int) (0.003 * sampleRate));
    for (auto& v : voices)
    {
        if (! v.active || v.track != track || v.fadeRemaining >= 0)
            continue;
        const bool samePad = (v.pad == pad);
        const bool sameGroup = (muteGroup > 0 && v.muteGroup == muteGroup && ! samePad);
        if ((includeSamePad && samePad) || sameGroup)
        {
            v.fadeRemaining = fadeLen;
            v.fadeLength = fadeLen;
        }
    }
}

void Sampler::trigger (const Program& program, int track, int pad, int velocity, float tuneOffset, int releaseAfter,
                       NoteVariation var)
{
    if (pad < 0 || pad >= kNumPads)
        return;
    const Pad& p = program.pads[(size_t) pad];
    if (! p.hasSample())
        return;

    choke (track, pad, p.muteGroup, p.poly == PolyMode::Mono);

    const float velGain = std::pow (juce::jlimit (1, 127, velocity) / 127.0f, 1.3f);

    for (const auto& layer : p.layers)
    {
        if (layer.sample == nullptr || velocity < layer.velStart || velocity > layer.velEnd)
            continue;
        const int frames = layer.sample->getNumFrames();
        if (frames <= 1)
            continue;

        Voice& v = allocateVoice();
        v.active = true;
        v.track = track;
        v.pad = pad;
        v.muteGroup = p.muteGroup;
        v.age = ++ageCounter;
        v.sample = layer.sample;

        const int start = juce::jlimit (0, frames - 1, layer.sliceStart);
        const int end = layer.sliceEnd < 0 ? frames : juce::jlimit (start + 1, frames, layer.sliceEnd);
        v.pos = start;
        v.endFrame = end;

        const double semis = (double) p.tuneSemis + layer.tuneSemis + tuneOffset;
        v.inc = (layer.sample->sampleRate / sampleRate) * std::pow (2.0, semis / 12.0);

        const float gain = velGain * dbToGain (p.levelDb + layer.levelDb);
        const float pan = juce::jlimit (-1.0f, 1.0f, p.pan + layer.pan);
        const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        v.gainL = gain * std::cos (angle) * juce::MathConstants<float>::sqrt2;
        v.gainR = gain * std::sin (angle) * juce::MathConstants<float>::sqrt2;
        v.reverbSend = juce::jlimit (0.0f, 1.0f, p.reverbSend);
        v.delaySend = juce::jlimit (0.0f, 1.0f, p.delaySend);

        // Note Variation (64 = unchanged): decay x0.25..x4, attack 0..500 ms, cutoff +/-4 octaves
        const float vv = (float) var.value;
        float attackMs = p.attackMs, decayMs = p.decayMs, cutoffHz = p.cutoffHz;
        FilterType filterType = p.filter;
        if (var.type == VariationType::Decay)
            decayMs *= std::pow (2.0f, (vv - 64.0f) / 32.0f);
        else if (var.type == VariationType::Attack)
            attackMs = 500.0f * (vv / 127.0f) * (vv / 127.0f);
        else if (var.type == VariationType::Filter)
        {
            cutoffHz = (filterType == FilterType::Off ? 16000.0f : cutoffHz) * std::pow (2.0f, (vv - 64.0f) / 16.0f);
            if (filterType == FilterType::Off && var.value < 64)
                filterType = FilterType::LowPass;
        }

        juce::ADSR::Parameters ap;
        ap.attack = std::max (0.0005f, attackMs / 1000.0f);
        ap.decay = std::max (0.001f, decayMs / 1000.0f);
        ap.sustain = juce::jlimit (0.0f, 1.0f, p.sustain);
        ap.release = std::max (0.002f, p.releaseMs / 1000.0f);
        v.env.setSampleRate (sampleRate);
        v.env.setParameters (ap);
        v.env.reset();
        v.env.noteOn();

        v.oneShot = (p.trigger == TriggerMode::OneShot);
        v.releaseCountdown = v.oneShot ? -1 : releaseAfter;
        v.fadeRemaining = -1;

        v.filter = filterType;
        if (v.filter != FilterType::Off)
        {
            const double fc = juce::jlimit (20.0, sampleRate * 0.45, (double) cutoffHz);
            const float g = (float) std::tan (juce::MathConstants<double>::pi * fc / sampleRate);
            v.k = 2.0f - 1.95f * juce::jlimit (0.0f, 1.0f, p.resonance);
            v.a1 = 1.0f / (1.0f + g * (g + v.k));
            v.a2 = g * v.a1;
            v.a3 = g * v.a2;
            v.ic1[0] = v.ic1[1] = v.ic2[0] = v.ic2[1] = 0.0f;
        }
    }
}

void Sampler::release (int track, int pad)
{
    for (auto& v : voices)
        if (v.active && v.track == track && v.pad == pad && ! v.oneShot && v.releaseCountdown < 0)
            v.env.noteOff();
}

void Sampler::render (juce::AudioBuffer<float>& out, int startSample, int numSamples)
{
    for (auto& v : voices)
    {
        if (! v.active)
            continue;
        if (v.track < 0 || v.track * 2 + 1 >= out.getNumChannels())
        {
            v.active = false;
            continue;
        }

        const auto& buf = v.sample->buffer;
        const int n = buf.getNumSamples();
        const float* srcL = buf.getReadPointer (0);
        const float* srcR = buf.getNumChannels() > 1 ? buf.getReadPointer (1) : srcL;
        float* dstL = out.getWritePointer (v.track * 2, startSample);
        float* dstR = out.getWritePointer (v.track * 2 + 1, startSample);
        const bool sends = out.getNumChannels() >= kMixChannels && (v.reverbSend > 0.0f || v.delaySend > 0.0f);
        float* revL = sends ? out.getWritePointer (kFxChannelBase + 0, startSample) : nullptr;
        float* revR = sends ? out.getWritePointer (kFxChannelBase + 1, startSample) : nullptr;
        float* dlyL = sends ? out.getWritePointer (kFxChannelBase + 2, startSample) : nullptr;
        float* dlyR = sends ? out.getWritePointer (kFxChannelBase + 3, startSample) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            if (v.pos >= v.endFrame)
            {
                v.active = false;
                break;
            }
            if (v.releaseCountdown > 0 && --v.releaseCountdown == 0)
                v.env.noteOff();

            float l = hermite (srcL, n, v.pos);
            float r = (srcR == srcL) ? l : hermite (srcR, n, v.pos);
            v.pos += v.inc;

            if (v.filter != FilterType::Off)
            {
                float* io[2] = { &l, &r };
                for (int c = 0; c < 2; ++c)
                {
                    const float v0 = *io[c];
                    const float v3 = v0 - v.ic2[c];
                    const float v1 = v.a1 * v.ic1[c] + v.a2 * v3;
                    const float v2 = v.ic2[c] + v.a2 * v.ic1[c] + v.a3 * v3;
                    v.ic1[c] = 2.0f * v1 - v.ic1[c];
                    v.ic2[c] = 2.0f * v2 - v.ic2[c];
                    switch (v.filter)
                    {
                        case FilterType::LowPass:  *io[c] = v2; break;
                        case FilterType::BandPass: *io[c] = v1; break;
                        case FilterType::HighPass: *io[c] = v0 - v.k * v1 - v2; break;
                        case FilterType::Off: break;
                    }
                }
            }

            float g = v.env.getNextSample();
            if (v.fadeRemaining >= 0)
            {
                g *= (float) v.fadeRemaining / (float) v.fadeLength;
                if (--v.fadeRemaining < 0)
                {
                    v.active = false;
                    break;
                }
            }
            if (! v.env.isActive())
            {
                v.active = false;
                break;
            }

            const float ol = l * g * v.gainL, orr = r * g * v.gainR;
            dstL[i] += ol;
            dstR[i] += orr;
            if (sends)
            {
                revL[i] += ol * v.reverbSend;
                revR[i] += orr * v.reverbSend;
                dlyL[i] += ol * v.delaySend;
                dlyR[i] += orr * v.delaySend;
            }
        }

        if (! v.active)
            v.sample.reset(); // NB: the engine keeps retired samples alive, so this never frees on the audio thread
    }
}

} // namespace padlab
