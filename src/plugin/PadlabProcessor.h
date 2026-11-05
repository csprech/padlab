#pragma once

// Padlab as an audio processor, for hosts (the Tabletop studio, or a plug-in host): the same engine and panel as
// the app, without an audio device of its own. Its jacks become the processor's buses:
//   input  0, 1       REC IN L, R (what SAMPLING records when the input is switched on)
//   output 0, 1       STEREO OUT L, R
//   MIDI in / out     MIDI IN (pads: notes 36-51, Q-Links: CC 70-74) / MIDI OUT (tracks sent to MIDI, clock)
// Its state is the whole project, samples and all.

#include "../engine/Engine.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace padlab
{
class PadlabProcessor : public juce::AudioProcessor
{
public:
    PadlabProcessor();
    ~PadlabProcessor() override;

    /** The panel's natural size, for hosts that show it at a fixed size. */
    static constexpr int kPanelWidth = 1500, kPanelHeight = 940;

    Engine& getEngine() { return engine; }

    const juce::String getName() const override { return "Padlab"; }
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    /** The project folder (padlab.json and its samples) zipped, plus MAIN VOLUME and the pad bank. */
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    Engine engine { true };
    juce::AudioBuffer<float> inputCopy;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadlabProcessor)
};
} // namespace padlab
