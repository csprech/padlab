#include "SeqEdit.h"
#include <algorithm>

namespace padlab::seqedit
{
namespace
{
    void sortEvents (Track& t)
    {
        std::stable_sort (t.events.begin(), t.events.end(), [] (const NoteEvent& a, const NoteEvent& b) { return a.tick < b.tick; });
        if (t.events.size() > (size_t) kEventCapacity)
            t.events.resize ((size_t) kEventCapacity);
    }
}

bool insertBars (Sequence& seq, int atBar, int count)
{
    if (count <= 0 || seq.bars + count > kMaxBars)
        return false;
    atBar = std::clamp (atBar, 0, seq.bars);
    const int64_t at = (int64_t) atBar * seq.getBarTicks(), shift = (int64_t) count * seq.getBarTicks();
    for (auto& t : seq.tracks)
        for (auto& e : t.events)
            if (e.tick >= at)
                e.tick += shift;
    seq.bars += count;
    if (seq.loopToBar >= atBar)
        seq.loopToBar = std::min (seq.loopToBar + count, seq.bars - 1);
    return true;
}

bool deleteBars (Sequence& seq, int fromBar, int toBar)
{
    fromBar = std::clamp (fromBar, 0, seq.bars - 1);
    toBar = std::clamp (toBar, fromBar, seq.bars - 1);
    const int n = toBar - fromBar + 1;
    if (n >= seq.bars)
        return false; // keep at least one bar
    const int64_t a = (int64_t) fromBar * seq.getBarTicks(), b = (int64_t) (toBar + 1) * seq.getBarTicks();
    for (auto& t : seq.tracks)
    {
        t.events.erase (std::remove_if (t.events.begin(), t.events.end(), [=] (const NoteEvent& e) { return e.tick >= a && e.tick < b; }),
                        t.events.end());
        for (auto& e : t.events)
            if (e.tick >= b)
                e.tick -= b - a;
    }
    seq.bars -= n;
    if (seq.loopToBar > toBar)
        seq.loopToBar -= n;
    else if (seq.loopToBar >= fromBar)
        seq.loopToBar = fromBar;
    seq.loopToBar = std::clamp (seq.loopToBar, 0, seq.bars - 1);
    return true;
}

bool copyBars (const Sequence& srcIn, int fromBar, int toBar, Sequence& dst, int beforeBar, int copies)
{
    const Sequence src = srcIn; // src may be dst
    fromBar = std::clamp (fromBar, 0, src.bars - 1);
    toBar = std::clamp (toBar, fromBar, src.bars - 1);
    const int n = toBar - fromBar + 1;
    copies = std::max (1, copies);
    if (! insertBars (dst, beforeBar, n * copies))
        return false;
    beforeBar = std::clamp (beforeBar, 0, dst.bars - n * copies);
    const int64_t srcA = (int64_t) fromBar * src.getBarTicks(), srcB = (int64_t) (toBar + 1) * src.getBarTicks();
    const int64_t dstAt = (int64_t) beforeBar * dst.getBarTicks();
    for (int t = 0; t < kNumTracks; ++t)
    {
        auto& out = dst.tracks[(size_t) t];
        for (int c = 0; c < copies; ++c)
            for (const auto& e : src.tracks[(size_t) t].events)
                if (e.tick >= srcA && e.tick < srcB)
                {
                    auto copy = e;
                    // bars are copied by position within the bar, so differing time signatures still line up bar-wise
                    const int64_t inBar = (e.tick - srcA) % src.getBarTicks();
                    const int64_t bar = (e.tick - srcA) / src.getBarTicks();
                    if (inBar >= dst.getBarTicks())
                        continue;
                    copy.tick = dstAt + ((int64_t) c * n + bar) * dst.getBarTicks() + inBar;
                    copy.flags = 0;
                    out.events.push_back (copy);
                }
        sortEvents (out);
    }
    return true;
}

void copySequence (const Sequence& src, Sequence& dst)
{
    if (&src != &dst)
        dst = src;
}

void copyTrack (const Track& src, Track& dst)
{
    const auto name = dst.name;
    dst = src;
    dst.name = name;
    dst.mute = dst.solo = false;
}

int editEvents (Track& track, const Range& range, Op op, int amount, int64_t length)
{
    int changed = 0;
    if (op == Op::Delete)
    {
        const auto before = track.events.size();
        track.events.erase (std::remove_if (track.events.begin(), track.events.end(), [&] (const NoteEvent& e) { return range.contains (e); }),
                            track.events.end());
        return (int) (before - track.events.size());
    }
    for (auto& e : track.events)
    {
        if (! range.contains (e))
            continue;
        ++changed;
        switch (op)
        {
            case Op::VelocityAdd:     e.velocity = (uint8_t) std::clamp ((int) e.velocity + amount, 1, 127); break;
            case Op::VelocityPercent: e.velocity = (uint8_t) std::clamp ((int) std::lround (e.velocity * amount / 100.0), 1, 127); break;
            case Op::VelocitySet:     e.velocity = (uint8_t) std::clamp (amount, 1, 127); break;
            case Op::Move:            e.tick = length > 0 ? (((e.tick + amount) % length) + length) % length : e.tick; break;
            case Op::ShiftPads:       e.pad = (int16_t) std::clamp ((int) e.pad + amount, 0, kNumPads - 1); break;
            case Op::LengthSet:       e.length = std::max (1, amount); break;
            case Op::Delete:          break;
        }
    }
    if (op == Op::Move)
        sortEvents (track);
    return changed;
}

} // namespace padlab::seqedit
