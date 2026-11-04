#include "core/Transport.h"
#include <algorithm>
#include <cmath>

namespace padlab
{
void ActionList::sortByOffset()
{
    std::stable_sort (items.begin(), items.begin() + size,
                      [] (const Action& a, const Action& b) { return a.offset < b.offset; });
}

void Transport::prepare (double sr) { sampleRate = sr > 0 ? sr : 44100.0; }

void Transport::play (bool fromStart, bool countIn)
{
    if (state != State::Stopped)
        return;
    if (fromStart)
        position = 0.0;
    unwrappedBase = 0.0;
    passCount = 0;
    countInRemaining = 0.0;
    state = (recordMode != RecordMode::None && countIn) ? State::CountIn : State::Playing;
    countInPending = (state == State::CountIn);
}

void Transport::playSong (const Project& p, int song, int fromStep)
{
    if (state != State::Stopped)
        return;
    const auto& sg = p.songs[(size_t) std::clamp (song, 0, kNumSongs - 1)];
    if (sg.numSteps <= 0)
        return;
    playingSong = true;
    songEnded = false;
    songIndex = std::clamp (song, 0, kNumSongs - 1);
    songStep = std::clamp (fromStep, 0, sg.numSteps - 1);
    songRepeat = 0;
    currentSequence = std::clamp (sg.steps[(size_t) songStep].sequence, 0, kNumSequences - 1);
    queuedSequence = -1;
    recordMode = RecordMode::None; // songs play back; recording happens in sequences
    position = 0.0;
    unwrappedBase = 0.0;
    passCount = 0;
    countInRemaining = 0.0;
    state = State::Playing;
}

bool Transport::advanceSong (const Project& p)
{
    const auto& sg = p.songs[(size_t) songIndex];
    if (sg.numSteps <= 0)
        return false;
    if (++songRepeat < std::max (1, sg.steps[(size_t) std::clamp (songStep, 0, sg.numSteps - 1)].repeats))
        return true;
    songRepeat = 0;
    if (++songStep >= sg.numSteps)
    {
        if (! sg.loop)
            return false;
        songStep = std::clamp (sg.loopToStep, 0, sg.numSteps - 1);
    }
    currentSequence = std::clamp (sg.steps[(size_t) songStep].sequence, 0, kNumSequences - 1);
    return true;
}

void Transport::stop()
{
    state = State::Stopped;
    playingSong = false;
    recordMode = RecordMode::None;
    pendingFlagClear = true;
}

void Transport::armRecord (RecordMode mode)
{
    if (recordMode != RecordMode::None && mode == RecordMode::None)
        pendingFlagClear = true;
    recordMode = mode;
}

void Transport::queueSequence (int index)
{
    index = std::clamp (index, 0, kNumSequences - 1);
    if (state == State::Stopped)
    {
        currentSequence = index;
        queuedSequence = -1;
        position = 0.0;
    }
    else
    {
        queuedSequence = (index == currentSequence) ? -1 : index;
    }
}

void Transport::locate (int64_t tick) { position = (double) std::max<int64_t> (0, tick); }

void Transport::clearFreshFlags (Track& t)
{
    for (auto& e : t.events)
        e.flags &= (uint8_t) ~NoteEvent::Fresh;
}

void Transport::mapInput (const PerformanceSettings& s, int physicalPad, int value,
                          int& targetPad, int& velocity, float& tune, NoteVariation& var) const
{
    targetPad = physicalPad;
    velocity = std::clamp (value, 1, 127);
    tune = 0.0f;
    var = {};

    if (s.sixteenLevels)
    {
        const int idx = physicalPad % kPadsPerBank;
        targetPad = std::clamp (s.sixteenLevelsSource, 0, kNumPads - 1);
        if (s.sixteenLevelsParam == 0)
            return void (velocity = (idx + 1) * 8 - 1); // 7 .. 127
        if (s.sixteenLevelsParam == 1)
            tune = (float) (idx - 8);                   // -8 .. +7 semitones
        else
            var = { (VariationType) std::clamp (s.sixteenLevelsParam, 2, 4), (uint8_t) std::lround (idx * 127.0 / 15.0) };
    }
    else if (s.noteVariationOn && (s.noteVariationPad < 0 || s.noteVariationPad == targetPad))
    {
        const int v = std::clamp (s.noteVariationValue, 0, 127);
        if (s.noteVariationType == VariationType::Tune)
            tune = (float) std::round ((v - 64) / 64.0 * 12.0 * 10.0) / 10.0f; // +/- 1 octave
        else if (s.noteVariationType != VariationType::None)
            var = { s.noteVariationType, (uint8_t) v };
    }

    if (s.fullLevel)
        velocity = 127;
}

// ------------------------------------------------------------------ helpers

namespace
{
    int toOffset (double cursor, double tickInSeg, double ticksPerSample, int numSamples)
    {
        const int o = (int) std::floor (cursor + tickInSeg / ticksPerSample);
        return std::clamp (o, 0, std::max (0, numSamples - 1));
    }

    bool trackAudible (const Track& t, bool anySolo)
    {
        if (t.mute)
            return false;
        return ! anySolo || t.solo;
    }

    const timing::Division& division (int index)
    {
        return timing::divisions()[(size_t) std::clamp (index, 0, timing::kNumDivisions - 1)];
    }
}

void Transport::scanSegment (Project& p, const PerformanceSettings& s, double segStart, double segEnd,
                             double cursor, double tps, int numSamples, ActionList& out)
{
    auto& seq = p.sequences[(size_t) currentSequence];
    const bool anySolo = seq.anySolo();
    const bool erasingPass = isRecording() && recordMode == RecordMode::Record;

    for (int ti = 0; ti < kNumTracks; ++ti)
    {
        auto& track = seq.tracks[(size_t) ti];
        auto& prog = p.programs[(size_t) std::clamp (track.programIndex, 0, kNumTracks - 1)];
        const bool isCurrent = (ti == s.currentTrack);
        const bool audible = trackAudible (track, anySolo);

        auto& ev = track.events;
        auto it = std::lower_bound (ev.begin(), ev.end(), (int64_t) std::ceil (segStart),
                                    [] (const NoteEvent& e, int64_t t) { return e.tick < t; });

        while (it != ev.end() && (double) it->tick < segEnd)
        {
            // REC: overwrite what was there before (but keep what this pass recorded)
            if (isCurrent && erasingPass && (it->flags & NoteEvent::Fresh) == 0)
            {
                it = ev.erase (it);
                continue;
            }
            // ERASE + held pad: delete that pad's notes as the playhead passes
            if (isCurrent && s.eraseHeld && isTargetHeld (it->pad))
            {
                it = ev.erase (it);
                continue;
            }
            if (it->flags & NoteEvent::SkipOnce)
            {
                it->flags &= (uint8_t) ~NoteEvent::SkipOnce;
                ++it;
                continue;
            }

            const auto& pad = prog.pads[(size_t) std::clamp ((int) it->pad, 0, kNumPads - 1)];
            if (audible && ! pad.mute)
            {
                Action a;
                a.type = Action::Trigger;
                a.offset = toOffset (cursor, (double) it->tick - segStart, tps, numSamples);
                a.track = ti;
                a.pad = it->pad;
                a.velocity = it->velocity;
                a.tune = it->tune;
                a.variation = it->variation;
                a.releaseAfter = std::max (1, (int) std::lround (it->length / tps));
                out.add (a);
            }
            ++it;
        }
    }
}

bool Transport::isTargetHeld (int targetPad) const
{
    for (auto& h : held)
        if (h.held && h.targetPad == targetPad)
            return true;
    return false;
}

void Transport::repeatSegment (Project& p, const PerformanceSettings& s, double segStart, double segEnd,
                               double cursor, double tps, int numSamples, bool recordInto,
                               ActionList& out)
{
    if (! s.noteRepeat || s.eraseHeld)
        return;

    const auto& div = division (s.repeatDivision);
    auto& seq = p.sequences[(size_t) currentSequence];
    auto& track = seq.tracks[(size_t) std::clamp (s.currentTrack, 0, kNumTracks - 1)];
    const int releaseSamples = std::max (1, (int) std::lround ((div.ticks * 0.5) / tps));

    for (auto& h : held)
    {
        if (! h.held)
            continue;

        int64_t g = timing::nextGridPoint ((int64_t) std::ceil (segStart), div.ticks, div.swingable,
                                           s.swing, s.legacy96);
        while ((double) g < segEnd)
        {
            Action a;
            a.type = Action::Trigger;
            a.offset = toOffset (cursor, (double) g - segStart, tps, numSamples);
            a.track = s.currentTrack;
            a.pad = h.targetPad;
            a.velocity = h.velocity;
            a.tune = h.tune;
            a.variation = h.variation;
            a.releaseAfter = releaseSamples;
            out.add (a);

            if (recordInto && ! track.isFull())
            {
                NoteEvent e;
                e.tick = g;
                e.pad = (int16_t) h.targetPad;
                e.velocity = (uint8_t) h.velocity;
                e.tune = h.tune;
                e.variation = h.variation;
                e.length = div.ticks / 2;
                e.flags = (recordMode == RecordMode::Record) ? NoteEvent::Fresh : 0;
                track.insertSorted (e);
            }
            g = timing::nextGridPoint (g + 1, div.ticks, div.swingable, s.swing, s.legacy96);
        }
    }
}

void Transport::clicksInRange (const Sequence& seq, double start, double end, double cursor, double tps,
                               int numSamples, ActionList& out)
{
    const int64_t barTicks = (int64_t) seq.beatsPerBar * kPPQN;
    int64_t b = (int64_t) std::ceil (start / kPPQN) * kPPQN;
    for (; (double) b < end; b += kPPQN)
    {
        Action a;
        a.type = Action::Click;
        a.offset = toOffset (cursor, (double) b - start, tps, numSamples);
        a.accent = (timing::floorDiv (b, barTicks) * barTicks == b);
        out.add (a);
    }
}

void Transport::handleWrap (Project& p, const PerformanceSettings& s)
{
    auto& seq = p.sequences[(size_t) currentSequence];
    unwrappedBase += (double) seq.getLengthTicks();
    position = 0.0;
    ++passCount;

    if (isRecording() && recordMode == RecordMode::Record && s.autoOverdub)
    {
        recordMode = RecordMode::Overdub;
        clearFreshFlags (seq.tracks[(size_t) std::clamp (s.currentTrack, 0, kNumTracks - 1)]);
    }

    if (playingSong)
    {
        if (! advanceSong (p))
            songEnded = true;
        return;
    }

    if (queuedSequence >= 0)
    {
        currentSequence = queuedSequence;
        queuedSequence = -1;
    }
}

void Transport::recordHit (Project& p, const PerformanceSettings& s, int targetPad, int velocity, float tune,
                           NoteVariation var, double rawUnwrapped, double blockEndUnwrapped, HeldPad& h)
{
    auto& seq = p.sequences[(size_t) currentSequence];
    auto& track = seq.tracks[(size_t) std::clamp (s.currentTrack, 0, kNumTracks - 1)];
    if (track.isFull())
        return;

    const int64_t L = seq.getLengthTicks();
    // which pass does this hit belong to? (a wrap may have happened inside this block)
    double base = unwrappedBase;
    if (rawUnwrapped < base)
        base -= (double) L;
    const int64_t rawLocal = (int64_t) std::llround (rawUnwrapped - base);

    const auto& tc = division (s.timingCorrect);
    int64_t q = timing::quantize (rawLocal, tc.ticks, tc.swingable, s.swing, s.tcStrength, s.legacy96);
    const double qUnwrapped = base + (double) q;

    NoteEvent e;
    e.tick = ((q % L) + L) % L;
    e.pad = (int16_t) targetPad;
    e.velocity = (uint8_t) velocity;
    e.tune = tune;
    e.variation = var;
    e.length = kPPQN / 4;
    e.flags = 0;
    if (recordMode == RecordMode::Record)
        e.flags |= NoteEvent::Fresh;
    if (qUnwrapped >= blockEndUnwrapped)
        e.flags |= NoteEvent::SkipOnce; // we already heard it live; don't double-trigger when snapped forward
    track.insertSorted (e);

    h.recordTickUnwrapped = (int64_t) std::llround (rawUnwrapped);
    h.recordedTick = e.tick;
}

// ------------------------------------------------------------------ process

void Transport::process (Project& p, const PerformanceSettings& s, const LiveInput* inputs, int numInputs,
                         int numSamples, ActionList& out)
{
    out.clear();
    if (numSamples <= 0)
        return;

    if (pendingFlagClear)
    {
        for (auto& seq : p.sequences)
            for (auto& t : seq.tracks)
                for (auto& e : t.events)
                    e.flags = 0;
        pendingFlagClear = false;
    }

    auto& seq = p.sequences[(size_t) currentSequence];
    const double tps = seq.bpm / 60.0 * kPPQN / sampleRate;
    const double blockTicks = numSamples * tps;
    const bool metronomeOn = s.metronome && (! s.metronomeOnlyWhenRecording || recordMode != RecordMode::None);

    // ---------------------------------------------------------------- stopped
    if (state == State::Stopped)
    {
        for (int i = 0; i < numInputs; ++i)
            handleLiveInput (p, s, inputs[i], numSamples, tps, false, 0.0, 0.0, out);

        if (s.noteRepeat && ! s.eraseHeld)
        {
            const auto& div = division (s.repeatDivision);
            const double start = freeTicks, end = freeTicks + blockTicks;
            for (auto& h : held)
            {
                if (! h.held)
                    continue;
                // grid relative to the moment the pad was pressed; the press itself already sounded
                const double relStart = std::max (start - (double) h.freeAnchor, 1.0);
                const double relEnd = end - (double) h.freeAnchor;
                int64_t g = timing::nextGridPoint ((int64_t) std::ceil (relStart), div.ticks, div.swingable,
                                                   s.swing, s.legacy96);
                while ((double) g < relEnd)
                {
                    Action a;
                    a.type = Action::Trigger;
                    a.offset = toOffset (0.0, (double) h.freeAnchor + (double) g - start, tps, numSamples);
                    a.track = s.currentTrack;
                    a.pad = h.targetPad;
                    a.velocity = h.velocity;
                    a.tune = h.tune;
                    a.variation = h.variation;
                    a.releaseAfter = std::max (1, (int) std::lround ((div.ticks * 0.5) / tps));
                    out.add (a);
                    g = timing::nextGridPoint (g + 1, div.ticks, div.swingable, s.swing, s.legacy96);
                }
            }
        }
        freeTicks += blockTicks;
        out.sortByOffset();
        return;
    }

    double cursor = 0.0; // samples consumed
    double countInCutoff = -1.0;

    // ---------------------------------------------------------------- count-in
    if (state == State::CountIn)
    {
        if (countInPending)
        {
            countInRemaining = (double) seq.beatsPerBar * kPPQN;
            countInPending = false;
        }
        const double countLen = (double) seq.beatsPerBar * kPPQN;
        const double elapsed = countLen - countInRemaining;
        const double take = std::min (countInRemaining, blockTicks);
        clicksInRange (seq, elapsed, elapsed + take, 0.0, tps, numSamples, out);
        countInRemaining -= take;
        cursor = take / tps;

        for (int i = 0; i < numInputs; ++i)
            if (inputs[i].offset < cursor)
                handleLiveInput (p, s, inputs[i], numSamples, tps, false, 0.0, 0.0, out);

        if (countInRemaining > 1.0e-6)
        {
            freeTicks += blockTicks;
            out.sortByOffset();
            return;
        }
        state = State::Playing;
        countInRemaining = 0.0;
        countInCutoff = cursor;
    }

    // ---------------------------------------------------------------- playing
    const double blockStartUnwrapped = unwrappedBase + position - cursor * tps;
    const double blockEndUnwrapped = blockStartUnwrapped + blockTicks;

    while (cursor < (double) numSamples - 1.0e-9)
    {
        auto& cur = p.sequences[(size_t) currentSequence];
        const double curTps = cur.bpm / 60.0 * kPPQN / sampleRate;
        const double L = (double) cur.getLengthTicks();
        const double remainingTicks = ((double) numSamples - cursor) * curTps;
        const double segStart = position;
        const double segEnd = std::min (position + remainingTicks, L);

        scanSegment (p, s, segStart, segEnd, cursor, curTps, numSamples, out);
        if (metronomeOn)
            clicksInRange (cur, segStart, segEnd, cursor, curTps, numSamples, out);
        repeatSegment (p, s, segStart, segEnd, cursor, curTps, numSamples, isRecording(), out);

        cursor += (segEnd - segStart) / curTps;
        position = segEnd;
        if (position >= L - 1.0e-9)
        {
            handleWrap (p, s);
            if (songEnded)
            {
                // end of the song: stop on the last step's final bar
                songEnded = false;
                stop();
                break;
            }
        }
    }

    for (int i = 0; i < numInputs; ++i)
    {
        const auto& in = inputs[i];
        if (in.offset < countInCutoff) // already handled during the count-in
            continue;
        const double rawU = blockStartUnwrapped + in.offset * tps;
        handleLiveInput (p, s, in, numSamples, tps, true, rawU, blockEndUnwrapped, out);
    }

    freeTicks += blockTicks;
    out.sortByOffset();
}

void Transport::handleLiveInput (Project& p, const PerformanceSettings& s, const LiveInput& in, int numSamples,
                                 double tps, bool playing, double rawUnwrapped, double blockEndUnwrapped,
                                 ActionList& out)
{
    if (in.pad < 0 || in.pad >= kNumPads)
        return;
    auto& h = held[(size_t) in.pad];
    const int offset = std::clamp (in.offset, 0, std::max (0, numSamples - 1));

    switch (in.type)
    {
        case LiveInput::PadDown:
        {
            int target = 0, vel = 0;
            float tune = 0.0f;
            NoteVariation var;
            mapInput (s, in.pad, in.value, target, vel, tune, var);
            h.held = true;
            h.targetPad = target;
            h.velocity = vel;
            h.tune = tune;
            h.variation = var;
            h.freeAnchor = (int64_t) std::llround (freeTicks + offset * tps);
            h.recordedTick = -1;

            if (s.eraseHeld && playing)
                return; // erase mode: holding the pad deletes, it doesn't play

            const bool repeatGridOnly = s.noteRepeat && playing; // while playing, repeat fires on the grid
            if (! repeatGridOnly)
            {
                Action a;
                a.type = Action::Trigger;
                a.offset = offset;
                a.track = s.currentTrack;
                a.pad = target;
                a.velocity = vel;
                a.tune = tune;
                a.variation = var;
                a.releaseAfter = -1;
                out.add (a);

                if (playing && isRecording())
                    recordHit (p, s, target, vel, tune, var, rawUnwrapped, blockEndUnwrapped, h);
            }
            break;
        }
        case LiveInput::PadUp:
        {
            if (! h.held)
                return;
            h.held = false;

            Action a;
            a.type = Action::Release;
            a.offset = offset;
            a.track = s.currentTrack;
            a.pad = h.targetPad;
            out.add (a);

            if (playing && h.recordedTick >= 0)
            {
                auto& seq = p.sequences[(size_t) currentSequence];
                auto& track = seq.tracks[(size_t) std::clamp (s.currentTrack, 0, kNumTracks - 1)];
                const auto len = std::max<int64_t> (1, (int64_t) std::llround (rawUnwrapped) - h.recordTickUnwrapped);
                for (auto& e : track.events)
                    if (e.tick == h.recordedTick && e.pad == h.targetPad)
                    {
                        e.length = (int32_t) std::min<int64_t> (len, seq.getLengthTicks());
                        break;
                    }
            }
            h.recordedTick = -1;
            break;
        }
        case LiveInput::Pressure:
            if (h.held && ! s.fullLevel && ! (s.sixteenLevels && s.sixteenLevelsParam == 0))
                h.velocity = std::clamp (in.value, 1, 127);
            break;
    }
}

} // namespace padlab
