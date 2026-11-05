#include "Timing.h"
#include <algorithm>
#include <cmath>

namespace padlab::timing
{
const std::array<Division, kNumDivisions>& divisions()
{
    static const std::array<Division, kNumDivisions> table { {
        { "1/4", 960, false },
        { "1/8", 480, true },
        { "1/8T", 320, false },
        { "1/16", 240, true },
        { "1/16T", 160, false },
        { "1/32", 120, false },
        { "1/32T", 80, false },
        { "Off", 1, false },
    } };
    return table;
}

int defaultDivisionIndex() { return 3; }

int64_t floorDiv (int64_t a, int64_t b)
{
    auto q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        --q;
    return q;
}

int64_t swungOffset (int divTicks, bool swingable, double swingPercent, bool legacy96)
{
    if (! swingable)
        return divTicks;

    const double pct = std::clamp (swingPercent, 50.0, 75.0) / 100.0;
    const double raw = pct * 2.0 * divTicks;
    if (legacy96)
        return (int64_t) std::llround (raw / 10.0) * 10;
    return (int64_t) std::llround (raw);
}

int64_t quantize (int64_t tick, int divTicks, bool swingable, double swingPercent,
                  double strength, bool legacy96)
{
    if (divTicks <= 1)
        return tick;

    const int64_t pairLen = 2 * (int64_t) divTicks;
    const int64_t off = swungOffset (divTicks, swingable, swingPercent, legacy96);
    const int64_t pairStart = floorDiv (tick, pairLen) * pairLen;

    const int64_t candidates[3] = { pairStart, pairStart + off, pairStart + pairLen };
    int64_t best = candidates[0];
    for (auto c : candidates)
        if (std::llabs (c - tick) < std::llabs (best - tick))
            best = c;

    strength = std::clamp (strength, 0.0, 1.0);
    return tick + (int64_t) std::llround ((double) (best - tick) * strength);
}

int64_t nextGridPoint (int64_t fromTick, int divTicks, bool swingable, double swingPercent,
                       bool legacy96)
{
    if (divTicks <= 1)
        return fromTick;

    const int64_t pairLen = 2 * (int64_t) divTicks;
    const int64_t off = swungOffset (divTicks, swingable, swingPercent, legacy96);
    const int64_t pairStart = floorDiv (fromTick, pairLen) * pairLen;

    if (pairStart >= fromTick)
        return pairStart;
    if (pairStart + off >= fromTick)
        return pairStart + off;
    return pairStart + pairLen;
}

} // namespace padlab::timing
