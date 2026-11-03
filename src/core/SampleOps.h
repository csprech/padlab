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

/** Loads WAV/AIFF/FLAC; returns nullptr on failure. Limited to maxSeconds. */
SamplePtr loadFile (const juce::File& file, double maxSeconds = 600.0);

bool writeWav (const Sample& s, const juce::File& file);

bool isSupportedAudioFile (const juce::File& file);
} // namespace padlab::sampleops
