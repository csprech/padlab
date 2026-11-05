#include "Project.h"
#include <algorithm>

namespace padlab
{
bool Pad::hasSample() const
{
    for (auto& l : layers)
        if (l.sample != nullptr)
            return true;
    return false;
}

void Track::insertSorted (const NoteEvent& e)
{
    if (isFull())
        return;
    auto it = std::upper_bound (events.begin(), events.end(), e.tick,
                                [] (int64_t t, const NoteEvent& ev) { return t < ev.tick; });
    events.insert (it, e);
}

bool Track::insertSortedNoAlloc (const NoteEvent& e)
{
    if (! canRecord())
        return false;
    insertSorted (e); // size < capacity, so vector::insert doesn't reallocate
    return true;
}

bool Sequence::anySolo() const
{
    return std::any_of (tracks.begin(), tracks.end(), [] (const Track& t) { return t.solo; });
}

Project::Project()
{
    for (int s = 0; s < kNumSequences; ++s)
    {
        sequences[(size_t) s].name = "Sequence " + juce::String (s + 1).paddedLeft ('0', 2);
        for (int t = 0; t < kNumTracks; ++t)
        {
            auto& tr = sequences[(size_t) s].tracks[(size_t) t];
            tr.name = "Track " + juce::String (t + 1);
            tr.programIndex = t % kNumPrograms;
        }
    }
    for (int p = 0; p < kNumPrograms; ++p)
        programs[(size_t) p].name = "Program " + juce::String (p + 1);
}

SamplePtr Project::findSample (const juce::String& n) const
{
    for (auto& s : samplePool)
        if (s->name == n)
            return s;
    return nullptr;
}

void Project::addSample (SamplePtr s)
{
    if (std::find (samplePool.begin(), samplePool.end(), s) == samplePool.end())
        samplePool.push_back (std::move (s));
}

juce::String Project::makeUniqueSampleName (const juce::String& base) const
{
    auto clean = base.isEmpty() ? juce::String ("Sample") : base;
    if (findSample (clean) == nullptr)
        return clean;
    for (int i = 2;; ++i)
    {
        auto candidate = clean + " " + juce::String (i);
        if (findSample (candidate) == nullptr)
            return candidate;
    }
}

} // namespace padlab

namespace padlab
{
bool Song::insertStep (int index, SongStep step)
{
    if (numSteps >= kMaxSongSteps)
        return false;
    index = juce::jlimit (0, numSteps, index);
    for (int i = numSteps; i > index; --i)
        steps[(size_t) i] = steps[(size_t) i - 1];
    step.sequence = juce::jlimit (0, kNumSequences - 1, step.sequence);
    step.repeats = juce::jlimit (1, 99, step.repeats);
    steps[(size_t) index] = step;
    ++numSteps;
    return true;
}

void Song::removeStep (int index)
{
    if (index < 0 || index >= numSteps)
        return;
    for (int i = index; i < numSteps - 1; ++i)
        steps[(size_t) i] = steps[(size_t) i + 1];
    --numSteps;
    loopToStep = juce::jlimit (0, juce::jmax (0, numSteps - 1), loopToStep);
}
} // namespace padlab
