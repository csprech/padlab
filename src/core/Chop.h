#pragma once

// Chop: split a sample into slices. Every function returns sorted slice *start*
// frames beginning with 0; a slice ends where the next begins (last ends at sample end).

#include "core/Project.h"
#include <vector>

namespace padlab::chop
{
/** Threshold mode: spectral-flux transient detection. sensitivity 0..100 (higher = more slices). */
std::vector<int> detectTransients (const Sample& s, double sensitivity, double minSliceMs);

/** Regions mode: N equal-length slices. */
std::vector<int> equalRegions (const Sample& s, int count);

/** BPM mode: a slice every `beatsPerSlice` beats at `bpm`. */
std::vector<int> byBpm (const Sample& s, double bpm, double beatsPerSlice);

/** Estimate tempo assuming the sample is a loop of `bars` bars of 4/4. */
double estimateLoopBpm (const Sample& s, int bars);

} // namespace padlab::chop
