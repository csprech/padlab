#pragma once

// The sequencer/transport. Runs on the audio thread once per block and turns
//   (project data + live pad input + performance settings)
// into a list of sample-accurate Actions for the sampler, while recording,
// erasing, looping, counting in and switching sequences.

#include "core/Project.h"
#include "core/Timing.h"

namespace padlab
{
struct LiveInput
{
    enum Type : uint8_t { PadDown, PadUp, Pressure };
    Type type = PadDown;
    int pad = 0;       // 0..127 (absolute, bank already applied)
    int value = 100;   // velocity or pressure 0..127
    int offset = 0;    // sample offset inside the block
};

struct Action
{
    enum Type : uint8_t { Trigger, Release, Click };
    Type type = Trigger;
    int offset = 0;
    int track = 0;
    int pad = 0;
    int velocity = 100;
    float tune = 0.0f;
    int releaseAfter = -1; // samples; -1 = hold until a Release action (live pads)
    bool accent = false;   // Click: downbeat
};

struct ActionList
{
    static constexpr int capacity = 1024;
    std::array<Action, capacity> items;
    int size = 0;

    void clear() { size = 0; }
    void add (const Action& a) { if (size < capacity) items[(size_t) size++] = a; }
    void sortByOffset();
};

enum class RecordMode { None, Record, Overdub };

struct PerformanceSettings
{
    int currentTrack = 0;
    bool fullLevel = false;
    bool sixteenLevels = false;
    int sixteenLevelsSource = 0;      // absolute pad
    int sixteenLevelsParam = 0;       // 0 = velocity, 1 = tune
    bool noteRepeat = false;
    int repeatDivision = 3;           // index into timing::divisions()
    int timingCorrect = 3;            // index into timing::divisions()
    double swing = 50.0;
    double tcStrength = 1.0;
    bool legacy96 = false;
    bool eraseHeld = false;
    bool metronome = true;
    bool metronomeOnlyWhenRecording = false;
    bool countIn = true;
    bool autoOverdub = true;          // REC switches to overdub after one full pass
};

class Transport
{
public:
    enum class State { Stopped, CountIn, Playing };

    void prepare (double sampleRate);

    // ---- commands (audio thread)
    void play (bool fromStart, bool countIn);
    void stop();
    void armRecord (RecordMode mode); // RecordMode::None disarms
    void queueSequence (int index);   // switches at loop end while playing, now when stopped
    void locate (int64_t tick);

    // ---- per block
    void process (Project& project, const PerformanceSettings& settings,
                  const LiveInput* inputs, int numInputs, int numSamples, ActionList& out);

    // ---- state
    State getState() const { return state; }
    bool isPlaying() const { return state != State::Stopped; }
    bool isRecording() const { return state == State::Playing && recordMode != RecordMode::None; }
    RecordMode getRecordMode() const { return recordMode; }
    double getPositionTicks() const { return position; }
    double getCountInTicks() const { return countInRemaining; }
    int getCurrentSequence() const { return currentSequence; }
    int getQueuedSequence() const { return queuedSequence; }
    uint32_t getPassCount() const { return passCount; }

    /** Called when a REC pass completes and switches to overdub (lets the engine clear Fresh flags). */
    static void clearFreshFlags (Track& t);

private:
    struct HeldPad
    {
        bool held = false;
        int targetPad = 0;
        int velocity = 0;
        float tune = 0.0f;
        int64_t recordTickUnwrapped = -1; // for note length on release
        int64_t recordedTick = -1;
        int64_t freeAnchor = 0;           // note repeat anchor while stopped
    };

    void mapInput (const PerformanceSettings& s, int physicalPad, int value,
                   int& targetPad, int& velocity, float& tune) const;
    bool isTargetHeld (int targetPad) const;
    void scanSegment (Project& p, const PerformanceSettings& s, double segStart, double segEnd,
                      double cursor, double tps, int numSamples, ActionList& out);
    void repeatSegment (Project& p, const PerformanceSettings& s, double segStart, double segEnd,
                        double cursor, double tps, int numSamples, bool recordInto, ActionList& out);
    void clicksInRange (const Sequence& seq, double start, double end, double cursor, double tps,
                        int numSamples, ActionList& out);
    void handleWrap (Project& p, const PerformanceSettings& s);
    void handleLiveInput (Project& p, const PerformanceSettings& s, const LiveInput& in, int numSamples,
                          double tps, bool playing, double rawUnwrapped, double blockEndUnwrapped,
                          ActionList& out);
    void recordHit (Project& p, const PerformanceSettings& s, int targetPad, int velocity, float tune,
                    double rawUnwrapped, double blockEndUnwrapped, HeldPad& held);

    double sampleRate = 44100.0;
    State state = State::Stopped;
    RecordMode recordMode = RecordMode::None;
    double position = 0.0;            // ticks inside the current sequence
    double countInRemaining = 0.0;    // ticks
    double freeTicks = 0.0;           // free-running clock for note repeat while stopped
    double unwrappedBase = 0.0;       // monotonically increasing tick counter while playing
    int currentSequence = 0;
    int queuedSequence = -1;
    uint32_t passCount = 0;
    bool countInPending = false;
    bool pendingFlagClear = false;
    std::array<HeldPad, kNumPads> held {};
};

} // namespace padlab
