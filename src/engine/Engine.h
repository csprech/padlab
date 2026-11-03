#pragma once

// Real-time engine: owns the Project, the Transport and the Sampler and runs them
// inside the audio callback.
//
// Threading:
//  * The project is guarded by a SpinLock. The audio thread holds it for one block;
//    the UI holds it briefly via withProject().
//  * Pad hits and transport commands go through lock-free FIFOs (one per producer thread).
//  * Samples removed from the project are "retired" and only freed on the message
//    thread once no voice references them, so the audio thread never frees memory.

#include "core/Project.h"
#include "core/Sampler.h"
#include "core/Transport.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <functional>

namespace padlab
{
class Engine : public juce::AudioIODeviceCallback,
               public juce::MidiInputCallback,
               private juce::Timer
{
public:
    Engine();
    ~Engine() override;

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    void setInputEnabled (bool enabled);
    bool isInputEnabled() const;

    // ------------------------------------------------------------ project
    template <typename Fn>
    auto withProject (Fn&& fn)
    {
        const LockGuard g (*this);
        return fn (*project);
    }
    void replaceProject (std::unique_ptr<Project> newProject);
    void retire (SamplePtr s);

    // ------------------------------------------------------------ performance settings
    template <typename Fn>
    void updateSettings (Fn&& fn)
    {
        const LockGuard g (*this);
        fn (settings);
    }
    PerformanceSettings getSettings() const;

    int getCurrentBank() const { return currentBank.load(); }
    void setCurrentBank (int b) { currentBank = juce::jlimit (0, kNumBanks - 1, b); }

    // ------------------------------------------------------------ pads (message thread)
    void padDown (int absolutePad, int velocity);
    void padUp (int absolutePad);
    void padPressure (int absolutePad, int pressure);

    /** Audition a pad of any track's program without recording (sample edit / program edit). */
    void audition (int track, int pad, int velocity = 110);

    // ------------------------------------------------------------ transport (message thread)
    void play (bool fromStart);
    void stop();
    void setRecord (RecordMode mode);
    void queueSequence (int index);
    void locate (int64_t tick);
    void panic();

    Transport::State getTransportState() const { return (Transport::State) uiState.load(); }
    RecordMode getRecordMode() const { return (RecordMode) uiRecordMode.load(); }
    double getPositionTicks() const { return uiPosition.load(); }
    double getCountInTicks() const { return uiCountIn.load(); }
    int getCurrentSequence() const { return uiSequence.load(); }
    int getQueuedSequence() const { return uiQueued.load(); }

    // ------------------------------------------------------------ undo (message thread)
    void pushUndo (const juce::String& what);
    bool undo();
    juce::String getUndoDescription() const;

    // ------------------------------------------------------------ meters / feedback
    uint32_t getPadHitCount (int pad) const { return padHits[(size_t) pad].load(); }
    int getPadHitVelocity (int pad) const { return padHitVel[(size_t) pad].load(); }
    float getTrackPeak (int t) const { return trackPeaks[(size_t) t].load(); }
    float getMasterPeak (int ch) const { return masterPeaks[(size_t) ch].load(); }
    int getActiveVoices() const { return activeVoices.load(); }
    double getSampleRate() const { return sampleRate.load(); }
    float getCpuLoad() const { return (float) deviceManager.getCpuUsage(); }

    // ------------------------------------------------------------ sampling
    enum class RecState { Idle, Armed, Recording, Done };
    void armSampling (bool resampleMaster, float thresholdDb, double maxSeconds);
    void stopSampling();
    RecState getSamplingState() const { return (RecState) recState.load(); }
    double getSampledSeconds() const;
    float getInputPeak (int ch) const { return inputPeaks[(size_t) ch].load(); }
    /** Once Done: takes the recording (message thread) and resets to Idle. */
    SamplePtr takeSampledAudio (const juce::String& name);

    /** Plays frames [start, end) of a sample directly (sample editor audition). */
    void previewSample (SamplePtr s, int start, int end);
    void stopPreview();
    /** Preview playhead in frames, or -1 when idle. */
    int getPreviewPosition() const { return previewPosUi.load(); }

    /** Q-Link CCs from MIDI (CC 70..73 -> knob 0..3), delivered on the message thread. */
    std::function<void (int knob, int value)> onQLinkMidi;

private:
    /** The SpinLock isn't re-entrant: catch accidental nesting on the UI thread in debug builds. */
    struct LockGuard
    {
        explicit LockGuard (const Engine& e) : lockRef (e.lock)
        {
            jassert (! heldByThisThread()); // nested withProject/updateSettings/getSettings would deadlock
            lockRef.enter();
            heldByThisThread() = true;
        }
        ~LockGuard()
        {
            heldByThisThread() = false;
            lockRef.exit();
        }
        static bool& heldByThisThread() { thread_local bool held = false; return held; }
        const juce::SpinLock& lockRef;
    };

    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numIn, float* const* outputs, int numOut,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void timerCallback() override;

    struct Command
    {
        enum Type { Play, PlayStart, Stop, Record, Queue, Locate, Panic, Audition };
        Type type;
        int64_t value = 0;
        int a = 0, b = 0;
    };

    template <typename T, int N>
    struct Fifo
    {
        juce::AbstractFifo fifo { N };
        std::array<T, N> items;
        bool push (const T& t)
        {
            auto w = fifo.write (1);
            if (w.blockSize1 > 0) { items[(size_t) w.startIndex1] = t; return true; }
            if (w.blockSize2 > 0) { items[(size_t) w.startIndex2] = t; return true; }
            return false;
        }
        template <typename Fn>
        void drain (Fn&& fn)
        {
            auto r = fifo.read (fifo.getNumReady());
            for (int i = 0; i < r.blockSize1; ++i) fn (items[(size_t) (r.startIndex1 + i)]);
            for (int i = 0; i < r.blockSize2; ++i) fn (items[(size_t) (r.startIndex2 + i)]);
        }
    };

    void pushPad (LiveInput in, bool fromMidi);
    void renderClick (float* l, float* r, int start, int num);

    juce::AudioDeviceManager deviceManager;
    mutable juce::SpinLock lock;
    std::unique_ptr<Project> project;
    PerformanceSettings settings;
    Transport transport;
    Sampler sampler;
    ActionList actions;
    std::array<LiveInput, 256> liveInputs {};
    juce::AudioBuffer<float> trackBuffers;

    Fifo<LiveInput, 512> uiPads, midiPads;
    Fifo<Command, 256> commands;

    std::atomic<int> currentBank { 0 };
    std::array<int, 128> midiNotePad = [] { std::array<int, 128> a {}; a.fill (-1); return a; }(); // MIDI thread only
    std::atomic<int> uiState { 0 }, uiRecordMode { 0 }, uiSequence { 0 }, uiQueued { -1 };
    std::atomic<double> uiPosition { 0.0 }, uiCountIn { 0.0 };
    std::array<std::atomic<uint32_t>, kNumPads> padHits {};
    std::array<std::atomic<int>, kNumPads> padHitVel {};
    std::array<std::atomic<float>, kNumTracks> trackPeaks {};
    std::array<std::atomic<float>, 2> masterPeaks {}, inputPeaks {};
    std::atomic<int> activeVoices { 0 };
    std::atomic<double> sampleRate { 44100.0 };

    // metronome click
    double clickPhase = 0.0, clickFreq = 1000.0;
    int clickRemaining = 0, clickLength = 1;
    float clickGain = 0.0f;

    // sample preview
    SamplePtr previewSamplePtr;
    double previewPos = 0.0, previewInc = 1.0;
    int previewEnd = 0;
    bool previewActive = false;
    std::atomic<int> previewPosUi { -1 };

    // sampling
    std::atomic<int> recState { 0 };
    std::atomic<bool> recResample { false }, recStopRequest { false };
    std::atomic<float> recThreshold { 0.0f };
    std::atomic<int> recMaxFrames { 0 };
    juce::AudioBuffer<float> recBuffer;
    std::atomic<int> recWritten { 0 };

    // undo
    struct UndoState
    {
        juce::String what;
        int sequence = 0;
        std::array<std::vector<NoteEvent>, kNumTracks> events;
        double bpm = 90.0;
        int bars = 2;
    };
    std::vector<UndoState> undoStack;

    std::vector<SamplePtr> graveyard;
};

} // namespace padlab
