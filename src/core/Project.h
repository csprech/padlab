#pragma once

// Padlab data model.
//
//   Project
//    ├─ samplePool   : shared audio buffers
//    ├─ programs[8]  : drum programs (128 pads = 8 banks x 16, 4 velocity layers per pad)
//    └─ sequences[16]: tempo, length, 8 tracks of note events at 960 PPQN
//
// All of this is owned by the Engine and guarded by its SpinLock; the audio thread
// reads and (while recording) writes events under that lock.

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <memory>
#include <vector>

namespace padlab
{
constexpr int kPPQN          = 960;
constexpr int kPadsPerBank   = 16;
constexpr int kNumBanks      = 8;
constexpr int kNumPads       = kPadsPerBank * kNumBanks; // 128
constexpr int kNumLayers     = 4;
constexpr int kNumTracks     = 64;  // per sequence
constexpr int kNumSequences  = 99;
constexpr int kNumPrograms   = 24;
constexpr int kMaxBars       = 999;
constexpr int kMaxVoices     = 64;
constexpr int kNumSongs      = 8;
constexpr int kMaxSongSteps  = 99;
constexpr int kEventCapacity = 4096;  // per track; reserved for the track being recorded so the audio thread never allocates

// The sampler renders into one buffer: a stereo pair per track, then the two FX send buses.
constexpr int kNumFxBuses    = 2;     // 0 = reverb, 1 = delay
constexpr int kFxChannelBase = kNumTracks * 2;
constexpr int kMixChannels   = kNumTracks * 2 + kNumFxBuses * 2;

struct Sample
{
    juce::String name;
    juce::AudioBuffer<float> buffer; // 1 or 2 channels
    double sampleRate = 44100.0;

    int getNumFrames() const { return buffer.getNumSamples(); }
    double getLengthSeconds() const { return buffer.getNumSamples() / sampleRate; }
};
using SamplePtr = std::shared_ptr<Sample>;

struct Layer
{
    SamplePtr sample;
    int velStart = 0, velEnd = 127;
    float tuneSemis = 0.0f;
    float levelDb   = 0.0f;
    float pan       = 0.0f;  // -1..1
    int sliceStart  = 0;     // frames, non-destructive slice
    int sliceEnd    = -1;    // -1 = end of sample
};

enum class FilterType { Off = 0, LowPass, HighPass, BandPass };
enum class TriggerMode { OneShot = 0, NoteOn };
enum class PolyMode { Poly = 0, Mono };

struct Pad
{
    std::array<Layer, kNumLayers> layers;
    float levelDb   = 0.0f;
    float pan       = 0.0f;
    float tuneSemis = 0.0f;
    float attackMs  = 0.0f;
    float decayMs   = 300.0f;
    float sustain   = 1.0f;   // 0..1
    float releaseMs = 30.0f;
    FilterType filter = FilterType::Off;
    float cutoffHz  = 20000.0f;
    float resonance = 0.1f;   // 0..1
    int muteGroup   = 0;      // 0 = off, 1..32: pads in the same group choke each other
    TriggerMode trigger = TriggerMode::OneShot;
    PolyMode poly = PolyMode::Poly;
    bool mute = false;        // pad mute (pad mixer)
    float reverbSend = 0.0f;  // 0..1 to the reverb bus
    float delaySend  = 0.0f;  // 0..1 to the delay bus

    bool hasSample() const;
};

struct Program
{
    juce::String name = "Program";
    std::array<Pad, kNumPads> pads;
    float levelDb = 0.0f;  // channel strip
    float pan     = 0.0f;
};

/** Note Variation: one sound parameter varied per note (the slider / 16 Levels).
    value 0..127, 64 = unchanged. Tune is carried in NoteEvent::tune instead. */
enum class VariationType : uint8_t { None = 0, Tune = 1, Decay = 2, Attack = 3, Filter = 4 };

struct NoteVariation
{
    VariationType type = VariationType::None;
    uint8_t value = 64;
    bool operator== (const NoteVariation& o) const { return type == o.type && value == o.value; }
};

struct NoteEvent
{
    enum Flags : uint8_t { SkipOnce = 1, Fresh = 2 };

    int64_t tick = 0;
    int16_t pad = 0;
    uint8_t velocity = 100;
    uint8_t flags = 0;
    int32_t length = kPPQN / 4; // ticks (used by Note-On pads)
    float tune = 0.0f;          // per-note tune offset (16 Levels / Note Variation: tune)
    NoteVariation variation;    // decay / attack / filter variation
};

struct Track
{
    juce::String name;
    int programIndex = 0;
    bool mute = false;
    bool solo = false;
    int midiChannel = 0;        // 0 = no MIDI out, 1..16 = send notes to that channel
    bool internalSound = true;  // play the program's sounds (off = a pure MIDI track)
    std::vector<NoteEvent> events; // kept sorted by tick

    /** UI / file loading: may allocate. No-op when the track is full. */
    void insertSorted (const NoteEvent& e);
    /** Audio thread (recording): never allocates; false when there's no reserved room. */
    bool insertSortedNoAlloc (const NoteEvent& e);
    bool isFull() const { return events.size() >= (size_t) kEventCapacity; }
    bool canRecord() const { return events.size() < events.capacity() && ! isFull(); }
    bool sendsMidi() const { return midiChannel >= 1 && midiChannel <= 16; }
};

/** MIDI note a pad sends on MIDI tracks: bank A pads 1-16 = notes 36-51, bank B 52-67... */
inline int padToMidiNote (int pad) { return std::clamp (36 + pad, 0, 127); }

struct Sequence
{
    juce::String name = "Sequence";
    double bpm = 90.0;
    int bars = 2;
    int beatsPerBar = 4;
    bool loop = true;       // off: play once and stop
    int loopToBar = 0;      // 0-based bar the loop jumps back to
    std::array<Track, kNumTracks> tracks;

    int64_t getLengthTicks() const { return (int64_t) bars * beatsPerBar * kPPQN; }
    int64_t getBarTicks() const { return (int64_t) beatsPerBar * kPPQN; }
    int64_t getLoopStartTicks() const { return std::clamp<int64_t> ((int64_t) loopToBar * getBarTicks(), 0, getLengthTicks() - getBarTicks()); }
    bool anySolo() const;
};

/** Song mode: sequences played in order, each repeated a number of times. */
struct SongStep
{
    int sequence = 0;
    int repeats = 1;
};

struct Song
{
    juce::String name = "Song";
    std::array<SongStep, kMaxSongSteps> steps {};
    int numSteps = 0;
    bool loop = false;
    int loopToStep = 0;

    bool insertStep (int index, SongStep step);
    void removeStep (int index);
};

/** Reverb + tempo-synced delay on the two send buses. */
struct FxSettings
{
    float reverbSize = 0.55f, reverbDamping = 0.45f, reverbReturnDb = -4.0f;
    int delayDivision = 1;     // index into timing::divisions() (1/8 by default)
    float delayFeedback = 0.35f, delayReturnDb = -6.0f;
};

struct Project
{
    juce::String name = "Untitled";
    std::vector<SamplePtr> samplePool;
    std::array<Program, kNumPrograms> programs;
    std::array<Sequence, kNumSequences> sequences;
    std::array<Song, kNumSongs> songs;
    float masterLevelDb = 0.0f;
    int vintageMode = 0; // 0 clean, 1 = 12-bit 40 kHz character, 2 = 16-bit warm converter character
    FxSettings fx;

    Project();
    SamplePtr findSample (const juce::String& name) const;
    void addSample (SamplePtr s);
    juce::String makeUniqueSampleName (const juce::String& base) const;
};

} // namespace padlab
