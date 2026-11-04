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

/** How a program's chop pads play when you hit several:
    Poly = they overlap (every pad poly, no mute group);
    Mono = one at a time (every pad mono and all in one mute group, so each chop cuts off the last). */
enum class Voicing { Poly = 0, Mono };

/** Pads of `program` that play a slice of `sample` (layer 1 on that sample, with a slice set). */
std::vector<int> chopPads (const Program& program, const Sample* sample);

/** Sets the voicing of every chop pad of `sample` in `program`; other pads are left alone. Mono reuses
    the group the chops already share, or takes the lowest mute group no other pad uses.
    Returns how many pads it set. */
int setVoicing (Program& program, const Sample* sample, Voicing v);

/** The voicing those pads have now: Mono when they all share a mute group, else Poly. */
Voicing getVoicing (const Program& program, const Sample* sample);

} // namespace padlab::chop
