#pragma once

// Open project format: a folder "<Name>.padlab/" containing project.json and samples/*.wav.

#include "core/Project.h"
#include <map>

namespace padlab::projectio
{
juce::var toJson (const Project& p, const std::map<const Sample*, juce::String>& sampleFiles);
bool fromJson (const juce::var& json, Project& out, const juce::File& samplesDir, juce::String& error);

bool save (const Project& p, const juce::File& folder, juce::String& error);
bool load (const juce::File& folder, Project& out, juce::String& error);
} // namespace padlab::projectio
