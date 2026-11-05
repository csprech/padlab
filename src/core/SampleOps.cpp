#include "SampleOps.h"
#include <cstring>

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

namespace
{
    const char* const kNativeExtensions = "wav;aif;aiff;flac;ogg";
    // compressed formats decoded by the operating system (Core Audio on macOS)
    const char* const kCompressedExtensions = "mp3;m4a;aac;mp4";
}

Mp3Gapless mp3GaplessInfo (const juce::File& file)
{
    juce::FileInputStream in (file);
    if (! in.openedOk())
        return {};
    juce::MemoryBlock head;
    in.readIntoMemoryBlock (head, 1 << 16);
    const auto* d = static_cast<const juce::uint8*> (head.getData());
    const size_t n = head.getSize();

    // skip an ID3v2 tag (its size is "syncsafe": 7 bits per byte)
    size_t off = 0;
    if (n >= 10 && d[0] == 'I' && d[1] == 'D' && d[2] == '3')
        off = 10 + (((size_t) d[6] & 0x7f) << 21 | ((size_t) d[7] & 0x7f) << 14 | ((size_t) d[8] & 0x7f) << 7 | ((size_t) d[9] & 0x7f))
              + ((d[5] & 0x10) != 0 ? 10 : 0);
    if (off + 4 > n || d[off] != 0xff || (d[off + 1] & 0xe0) != 0xe0)
        return {};

    const int version = (d[off + 1] >> 3) & 3; // 3 = MPEG-1, 2 = MPEG-2, 0 = MPEG-2.5
    const int layer = (d[off + 1] >> 1) & 3;   // 1 = Layer III
    if (layer != 1 || version == 1)
        return {};
    const bool mpeg1 = version == 3, mono = ((d[off + 3] >> 6) & 3) == 3;
    const size_t sideInfo = mpeg1 ? (mono ? 17 : 32) : (mono ? 9 : 17);
    const juce::int64 samplesPerFrame = mpeg1 ? 1152 : 576;

    // the Xing/Info header lives in the first (silent) frame
    const size_t x = off + 4 + sideInfo;
    if (x + 12 > n || ! (std::memcmp (d + x, "Xing", 4) == 0 || std::memcmp (d + x, "Info", 4) == 0))
        return {};
    const juce::uint32 flags = juce::ByteOrder::bigEndianInt (d + x + 4);
    if ((flags & 1) == 0)
        return {};
    const juce::int64 frames = juce::ByteOrder::bigEndianInt (d + x + 8);

    // LAME extension (also written by ffmpeg's libmp3lame): 12-bit encoder delay + 12-bit end padding
    size_t lame = x + 8;
    for (juce::uint32 bit : { 1u, 2u, 4u, 8u })
        if ((flags & bit) != 0)
            lame += bit == 4u ? 100 : 4; // frames, bytes, TOC, quality
    if (lame + 24 > n)
        return {};
    const juce::uint32 v = ((juce::uint32) d[lame + 21] << 16) | ((juce::uint32) d[lame + 22] << 8) | d[lame + 23];
    const juce::int64 delay = v >> 12, padding = v & 0xfff;
    Mp3Gapless g;
    g.rawLength = frames * samplesPerFrame;
    g.delay = delay;
    g.padding = padding;
    g.valid = g.length() > 0;
    return g;
}

juce::int64 mp3GaplessLength (const juce::File& file)
{
    const auto g = mp3GaplessInfo (file);
    return g.valid ? g.length() : -1;
}

bool isCompressedAudioFile (const juce::File& file)
{
    return file.hasFileExtension (kCompressedExtensions);
}

bool isSupportedAudioFile (const juce::File& file)
{
    if (file.hasFileExtension (kNativeExtensions))
        return true;
    if (! isCompressedAudioFile (file))
        return false;
    juce::SharedResourcePointer<Formats> formats;
    return formats->manager.findFormatForFileExtension (file.getFileExtension()) != nullptr;
}

juce::String supportedFileWildcard()
{
    juce::StringArray patterns;
    for (auto& ext : juce::StringArray::fromTokens (juce::String (kNativeExtensions) + ";" + kCompressedExtensions, ";", {}))
        if (isSupportedAudioFile (juce::File::getCurrentWorkingDirectory().getChildFile ("x." + ext)))
            patterns.add ("*." + ext);
    return patterns.joinIntoString (";");
}

SamplePtr loadFile (const juce::File& file, double maxSeconds)
{
    juce::SharedResourcePointer<Formats> formats;
    std::unique_ptr<juce::AudioFormatReader> reader (formats->manager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return nullptr;

    auto length = reader->lengthInSamples;
    juce::int64 startCut = 0;
    const bool mp3 = file.hasFileExtension ("mp3");
    const auto gapless = mp3 ? mp3GaplessInfo (file) : Mp3Gapless {};
    if (gapless.valid)
    {
        // MP3 encoders add silence at the start (delay) and end (padding) and record both in the
        // header. Core Audio sometimes applies the delay itself, sometimes not (it then returns the
        // raw frame count): handle both, so loops come out sample-exact and one-shots start on time.
        if (length >= gapless.rawLength)
            startCut = gapless.delay;
        length = juce::jmin (length - startCut, gapless.length());
    }
    const auto frames = (int) std::min<juce::int64> (length, (juce::int64) (maxSeconds * reader->sampleRate));
    auto s = std::make_shared<Sample>();
    s->name = file.getFileNameWithoutExtension();
    s->sampleRate = reader->sampleRate;
    const int chans = (int) juce::jlimit (1u, 2u, reader->numChannels);
    s->buffer.setSize (chans, frames);
    s->buffer.clear();
    if (! reader->read (&s->buffer, 0, frames, startCut, true, chans > 1))
        return nullptr;

    if (mp3 && ! gapless.valid)
    {
        // No gapless header, so the encoder's start delay (up to ~2 MP3 frames of silence) is still
        // there. If the sound begins inside that window it's a one-shot (a kick, a snare...): start
        // it just before its transient so it isn't late. Longer silence is deliberate and is kept.
        // (Measured against the peak: MP3 smears faint pre-echo ahead of sharp attacks.)
        constexpr int kMaxDelay = 2 * 1152;
        const float threshold = 0.05f * getPeak (*s);
        int onset = 0;
        for (bool found = false; ! found && onset < juce::jmin (kMaxDelay + 1, frames); )
        {
            for (int c = 0; c < chans; ++c)
                found = found || std::abs (s->buffer.getSample (c, onset)) > threshold;
            if (! found)
                ++onset;
        }
        const int cut = juce::jmax (0, onset - (int) (0.001 * s->sampleRate)); // keep 1 ms of run-up
        if (threshold > 0.0f && onset <= kMaxDelay && cut > 0)
        {
            juce::AudioBuffer<float> trimmed (chans, frames - cut);
            for (int c = 0; c < chans; ++c)
                trimmed.copyFrom (c, 0, s->buffer, c, cut, frames - cut);
            s->buffer = std::move (trimmed);
        }
    }

    // Decoded audio is plain PCM from here on: it's kept and saved exactly like a WAV.
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
