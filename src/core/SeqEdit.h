#pragma once

// Sequence editing (the EDIT functions of classic sequencers): insert / delete / copy bars,
// copy sequences and tracks, and edit a range of events. Bars are 0-based here.

#include "Project.h"

namespace padlab::seqedit
{
/** Inserts `count` empty bars before bar `atBar` (atBar == bars appends). False if it would exceed kMaxBars. */
bool insertBars (Sequence& seq, int atBar, int count);

/** Deletes bars [fromBar, toBar] (inclusive) and closes the gap. Keeps at least one bar. */
bool deleteBars (Sequence& seq, int fromBar, int toBar);

/** Copies bars [fromBar, toBar] of `src`, `copies` times, inserting them before bar `beforeBar` of `dst`
    (all tracks). src and dst may be the same sequence. */
bool copyBars (const Sequence& src, int fromBar, int toBar, Sequence& dst, int beforeBar, int copies);

/** Copies a whole sequence (events, tempo, length, track setup) over another. */
void copySequence (const Sequence& src, Sequence& dst);

/** Copies a track's events and setup (program, MIDI output) over another track. */
void copyTrack (const Track& src, Track& dst);

/** A range of events: [fromTick, toTick) and pads [padLo, padHi]. */
struct Range
{
    int64_t fromTick = 0, toTick = INT64_MAX;
    int padLo = 0, padHi = kNumPads - 1;
    bool contains (const NoteEvent& e) const { return e.tick >= fromTick && e.tick < toTick && e.pad >= padLo && e.pad <= padHi; }
};

enum class Op
{
    VelocityAdd,     // amount: -127..127
    VelocityPercent, // amount: percent (e.g. 80)
    VelocitySet,     // amount: 1..127
    Move,            // amount: ticks (+ later, - earlier); events that fall outside the sequence wrap
    ShiftPads,       // amount: pads (MIDI tracks: semitones)
    LengthSet,       // amount: ticks
    Delete
};

/** Applies `op` to every event of `track` in `range`; returns how many events were changed. */
int editEvents (Track& track, const Range& range, Op op, int amount, int64_t sequenceLength);

} // namespace padlab::seqedit
