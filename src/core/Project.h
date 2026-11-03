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
constexpr int kNumTracks     = 8;
constexpr int kNumSequences  = 16;
constexpr int kMaxVoices     = 64;
constexpr int kEventCapacity = 4096;  // per track; pre-reserved so the audio thread never reallocates

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

    bool hasSample() const;
};

struct Program
{
    juce::String name = "Program";
    std::array<Pad, kNumPads> pads;
    float levelDb = 0.0f;  // channel strip
    float pan     = 0.0f;
};

struct NoteEvent
{
    enum Flags : uint8_t { SkipOnce = 1, Fresh = 2 };

    int64_t tick = 0;
    int16_t pad = 0;
    uint8_t velocity = 100;
    uint8_t flags = 0;
    int32_t length = kPPQN / 4; // ticks (used by Note-On pads)
    float tune = 0.0f;          // per-note tune offset (16 Levels: tune)
};

struct Track
{
    juce::String name;
    int programIndex = 0;
    bool mute = false;
    bool solo = false;
    std::vector<NoteEvent> events; // kept sorted by tick

    Track();
    Track(const Track&);
    Track& operator= (const Track&);

    void insertSorted (const NoteEvent& e); // no-op when the track is full
    bool isFull() const { return events.size() >= (size_t) kEventCapacity; }
};

struct Sequence
{
    juce::String name = "Sequence";
    double bpm = 90.0;
    int bars = 2;
    int beatsPerBar = 4;
    std::array<Track, kNumTracks> tracks;

    int64_t getLengthTicks() const { return (int64_t) bars * beatsPerBar * kPPQN; }
    bool anySolo() const;
};

struct Project
{
    juce::String name = "Untitled";
    std::vector<SamplePtr> samplePool;
    std::array<Program, kNumTracks> programs;
    std::array<Sequence, kNumSequences> sequences;
    float masterLevelDb = 0.0f;

    Project();
    SamplePtr findSample (const juce::String& name) const;
    void addSample (SamplePtr s);
    juce::String makeUniqueSampleName (const juce::String& base) const;
};

} // namespace padlab
