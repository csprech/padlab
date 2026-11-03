#pragma once

// Timing Correct (quantize) and swing.
//
// Swing follows Roger Linn's description: "delay the second 16th note within each 8th note".
// For a swingable division of D ticks, grid points come in pairs spanning 2D ticks.
// The first point of a pair sits on the beat; the second sits at swing% x 2D.
//   50%  -> straight      66% -> triplet feel      75% -> dotted feel
//
// legacy96 snaps the swung offset to a 96 PPQN grid (10 ticks at 960 PPQN), the
// resolution of the late-80s / early-90s machines, for research comparisons.

#include <array>
#include <cstdint>

namespace padlab::timing
{
struct Division
{
    const char* name;
    int ticks;
    bool swingable;
};

constexpr int kNumDivisions = 8;
const std::array<Division, kNumDivisions>& divisions();
int defaultDivisionIndex(); // 1/16

int64_t floorDiv (int64_t a, int64_t b);

/** Offset of the second grid point within a pair, in ticks. */
int64_t swungOffset (int divTicks, bool swingable, double swingPercent, bool legacy96);

/** Nearest swung grid point to `tick`, moved by `strength` (0..1) toward it. */
int64_t quantize (int64_t tick, int divTicks, bool swingable, double swingPercent,
                  double strength = 1.0, bool legacy96 = false);

/** First swung grid point >= fromTick. */
int64_t nextGridPoint (int64_t fromTick, int divTicks, bool swingable, double swingPercent,
                       bool legacy96 = false);

} // namespace padlab::timing
