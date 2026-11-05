#include "Chop.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>

namespace padlab::chop
{
namespace
{
    std::vector<float> monoMix (const Sample& s)
    {
        const int n = s.buffer.getNumSamples();
        std::vector<float> m ((size_t) n, 0.0f);
        const int ch = s.buffer.getNumChannels();
        for (int c = 0; c < ch; ++c)
        {
            const float* d = s.buffer.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                m[(size_t) i] += d[i] / (float) ch;
        }
        return m;
    }

    int refineOnset (const std::vector<float>& x, int centre, int radius)
    {
        const int n = (int) x.size();
        const int lo = std::max (0, centre - radius);
        const int hi = std::min (n, centre + radius);
        constexpr int block = 32;
        float maxBlock = 0.0f;
        int maxAt = lo;
        for (int b = lo; b < hi; b += block)
        {
            float m = 0.0f;
            for (int i = b; i < std::min (hi, b + block); ++i)
                m = std::max (m, std::abs (x[(size_t) i]));
            if (m > maxBlock)
            {
                maxBlock = m;
                maxAt = b;
            }
        }
        // walk back to where the attack begins
        int pos = maxAt;
        while (pos - block >= lo)
        {
            float m = 0.0f;
            for (int i = pos - block; i < pos; ++i)
                m = std::max (m, std::abs (x[(size_t) i]));
            if (m < maxBlock * 0.15f)
                break;
            pos -= block;
        }
        // first sample in the block that rises above the noise floor
        for (int i = pos; i < std::min (n, pos + block); ++i)
            if (std::abs (x[(size_t) i]) >= maxBlock * 0.1f)
                return i;
        return pos;
    }
}

std::vector<int> detectTransients (const Sample& s, double sensitivity, double minSliceMs)
{
    const auto x = monoMix (s);
    const int n = (int) x.size();
    std::vector<int> out { 0 };
    constexpr int order = 10, N = 1 << order, hop = 256;
    if (n < N)
        return out;

    juce::dsp::FFT fft (order);
    std::vector<float> window ((size_t) N), frame ((size_t) N * 2), prev ((size_t) N / 2 + 1, 0.0f);
    for (int i = 0; i < N; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N);

    const int numFrames = (n - N) / hop + 1;
    std::vector<float> flux ((size_t) numFrames, 0.0f);
    for (int f = 0; f < numFrames; ++f)
    {
        std::fill (frame.begin(), frame.end(), 0.0f);
        for (int i = 0; i < N; ++i)
            frame[(size_t) i] = x[(size_t) (f * hop + i)] * window[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (frame.data(), true);
        float sum = 0.0f;
        for (int b = 1; b <= N / 2; ++b)
        {
            const float mag = std::log1p (10.0f * frame[(size_t) b]);
            sum += std::max (0.0f, mag - prev[(size_t) b]);
            prev[(size_t) b] = mag;
        }
        flux[(size_t) f] = f == 0 ? 0.0f : sum;
    }

    const float maxFlux = *std::max_element (flux.begin(), flux.end());
    if (maxFlux <= 0.0f)
        return out;
    for (auto& v : flux)
        v /= maxFlux;

    const double sens = std::clamp (sensitivity, 0.0, 100.0) / 100.0;
    const float delta = (float) (0.5 * (1.0 - sens) + 0.03);
    const int minGap = std::max (1, (int) (minSliceMs * 0.001 * s.sampleRate));
    constexpr int meanRadius = 8, peakRadius = 3;

    for (int f = 1; f < numFrames; ++f)
    {
        float mean = 0.0f;
        int cnt = 0;
        for (int j = std::max (0, f - meanRadius); j <= std::min (numFrames - 1, f + meanRadius); ++j, ++cnt)
            mean += flux[(size_t) j];
        mean /= (float) cnt;

        const float v = flux[(size_t) f];
        if (v < mean + delta)
            continue;
        bool isPeak = true;
        for (int j = std::max (0, f - peakRadius); j <= std::min (numFrames - 1, f + peakRadius); ++j)
            if (j != f && (flux[(size_t) j] > v || (juce::exactlyEqual (flux[(size_t) j], v) && j < f)))
                isPeak = false;
        if (! isPeak)
            continue;

        // energy entered the analysis window in its last hop
        const int centre = f * hop + N - hop;
        const int onset = refineOnset (x, centre, 2 * hop);
        if (onset - out.back() >= minGap)
            out.push_back (onset);
    }
    return out;
}

std::vector<int> equalRegions (const Sample& s, int count)
{
    count = std::max (1, count);
    const int n = s.buffer.getNumSamples();
    std::vector<int> out;
    for (int i = 0; i < count; ++i)
        out.push_back ((int) ((int64_t) n * i / count));
    out.erase (std::unique (out.begin(), out.end()), out.end());
    return out;
}

std::vector<int> byBpm (const Sample& s, double bpm, double beatsPerSlice)
{
    std::vector<int> out;
    if (bpm <= 0.0 || beatsPerSlice <= 0.0)
        return { 0 };
    const double step = 60.0 / bpm * beatsPerSlice * s.sampleRate;
    const int n = s.buffer.getNumSamples();
    for (double p = 0.0; p < n - 1; p += step)
        out.push_back ((int) std::llround (p));
    return out;
}

double estimateLoopBpm (const Sample& s, int bars)
{
    const double seconds = s.getLengthSeconds();
    if (seconds <= 0.0 || bars <= 0)
        return 0.0;
    double bpm = bars * 4.0 * 60.0 / seconds;
    while (bpm > 0.0 && bpm < 70.0)
        bpm *= 2.0;
    while (bpm > 180.0)
        bpm /= 2.0;
    return bpm;
}

// ---------------------------------------------------------------- chop voicing

std::vector<int> chopPads (const Program& program, const Sample* sample)
{
    std::vector<int> pads;
    if (sample == nullptr)
        return pads;
    for (int i = 0; i < kNumPads; ++i)
    {
        const auto& l = program.pads[(size_t) i].layers[0];
        if (l.sample.get() == sample && (l.sliceStart > 0 || l.sliceEnd >= 0))
            pads.push_back (i);
    }
    return pads;
}

Voicing getVoicing (const Program& program, const Sample* sample)
{
    const auto pads = chopPads (program, sample);
    if (pads.empty())
        return Voicing::Poly;
    const int group = program.pads[(size_t) pads[0]].muteGroup;
    if (group <= 0)
        return Voicing::Poly;
    for (int p : pads)
        if (program.pads[(size_t) p].muteGroup != group)
            return Voicing::Poly;
    return Voicing::Mono;
}

int setVoicing (Program& program, const Sample* sample, Voicing v)
{
    const auto pads = chopPads (program, sample);
    if (pads.empty())
        return 0;
    int group = 0;
    if (v == Voicing::Mono)
    {
        if (getVoicing (program, sample) == Voicing::Mono)
            group = program.pads[(size_t) pads[0]].muteGroup; // already one group: keep it
        else
        {
            // the lowest group no other pad uses, so the chops don't start choking unrelated sounds
            std::array<bool, 33> used {};
            for (int i = 0; i < kNumPads; ++i)
                if (std::find (pads.begin(), pads.end(), i) == pads.end())
                    used[(size_t) juce::jlimit (0, 32, program.pads[(size_t) i].muteGroup)] = true;
            group = 32;
            for (int g = 1; g <= 32; ++g)
                if (! used[(size_t) g])
                {
                    group = g;
                    break;
                }
        }
    }
    for (int p : pads)
    {
        auto& pad = program.pads[(size_t) p];
        pad.poly = v == Voicing::Mono ? PolyMode::Mono : PolyMode::Poly;
        pad.muteGroup = group;
    }
    return (int) pads.size();
}

} // namespace padlab::chop
