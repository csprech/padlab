#pragma once

#include "core/Project.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace padlab::sampleops
{
void normalize (Sample& s, float peakDb = -0.3f);
void reverse (Sample& s);
float getPeak (const Sample& s);

/** New sample holding frames [start, end). */
SamplePtr extract (const Sample& s, int start, int end, const juce::String& newName);

/** Loads WAV/AIFF/FLAC/OGG, and MP3/M4A/AAC where the OS can decode them (macOS).
    Compressed files are decoded to PCM, i.e. converted to WAV audio: from then on they're
    stored, edited and saved (as 24-bit WAV) like any other sample.
    Returns nullptr on failure. Limited to maxSeconds. */
SamplePtr loadFile (const juce::File& file, double maxSeconds = 600.0);

bool writeWav (const Sample& s, const juce::File& file);

bool isSupportedAudioFile (const juce::File& file);
/** MP3/M4A/AAC: imported by converting to WAV audio. */
bool isCompressedAudioFile (const juce::File& file);
/** Gapless info from an MP3's LAME/Info header (written by LAME, ffmpeg, iTunes...). */
struct Mp3Gapless
{
    bool valid = false;
    juce::int64 rawLength = 0, delay = 0, padding = 0; // raw = frames x samples-per-frame
    juce::int64 length() const { return rawLength - delay - padding; }
};
Mp3Gapless mp3GaplessInfo (const juce::File& file);

/** Exact sample count of an MP3 from its LAME/Info header (start delay and end padding
    removed), or -1 if the file doesn't carry one. */
juce::int64 mp3GaplessLength (const juce::File& file);
/** File-chooser pattern for every format loadFile can read, e.g. "*.wav;*.mp3;...". */
juce::String supportedFileWildcard();
} // namespace padlab::sampleops
