#include "core/SampleOps.h"

namespace padlab::sampleops
{
float getPeak (const Sample& s)
{
    float peak = 0.0f;
    for (int c = 0; c < s.buffer.getNumChannels(); ++c)
        peak = std::max (peak, s.buffer.getMagnitude (c, 0, s.buffer.getNumSamples()));
    return peak;
}

void normalize (Sample& s, float peakDb)
{
    const float peak = getPeak (s);
    if (peak > 1.0e-6f)
        s.buffer.applyGain (juce::Decibels::decibelsToGain (peakDb) / peak);
}

void reverse (Sample& s)
{
    for (int c = 0; c < s.buffer.getNumChannels(); ++c)
        s.buffer.reverse (c, 0, s.buffer.getNumSamples());
}

SamplePtr extract (const Sample& s, int start, int end, const juce::String& newName)
{
    const int n = s.buffer.getNumSamples();
    start = juce::jlimit (0, n, start);
    end = juce::jlimit (start, n, end < 0 ? n : end);
    auto out = std::make_shared<Sample>();
    out->name = newName;
    out->sampleRate = s.sampleRate;
    out->buffer.setSize (s.buffer.getNumChannels(), std::max (1, end - start));
    out->buffer.clear();
    for (int c = 0; c < s.buffer.getNumChannels(); ++c)
        out->buffer.copyFrom (c, 0, s.buffer, c, start, end - start);
    return out;
}

namespace
{
    struct Formats
    {
        Formats() { manager.registerBasicFormats(); }
        juce::AudioFormatManager manager;
    };
}

bool isSupportedAudioFile (const juce::File& file)
{
    return file.hasFileExtension ("wav;aif;aiff;flac;ogg");
}

SamplePtr loadFile (const juce::File& file, double maxSeconds)
{
    juce::SharedResourcePointer<Formats> formats;
    std::unique_ptr<juce::AudioFormatReader> reader (formats->manager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return nullptr;

    const auto frames = (int) std::min<juce::int64> (reader->lengthInSamples,
                                                      (juce::int64) (maxSeconds * reader->sampleRate));
    auto s = std::make_shared<Sample>();
    s->name = file.getFileNameWithoutExtension();
    s->sampleRate = reader->sampleRate;
    const int chans = (int) juce::jlimit (1u, 2u, reader->numChannels);
    s->buffer.setSize (chans, frames);
    reader->read (&s->buffer, 0, frames, 0, true, chans > 1);
    return s;
}

bool writeWav (const Sample& s, const juce::File& file)
{
    file.deleteFile();
    auto stream = file.createOutputStream();
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (stream.release());
    auto options = juce::AudioFormatWriterOptions{}
                       .withSampleRate (s.sampleRate)
                       .withNumChannels (s.buffer.getNumChannels())
                       .withBitsPerSample (24);
    auto writer = wav.createWriterFor (os, options);
    if (writer == nullptr)
        return false;
    return writer->writeFromAudioSampleBuffer (s.buffer, 0, s.buffer.getNumSamples());
}

} // namespace padlab::sampleops
