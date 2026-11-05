#include "PadlabProcessor.h"
#include "../core/ProjectIO.h"
#include "../ui/MainComponent.h"

namespace padlab
{
namespace
{
    class Editor : public juce::AudioProcessorEditor
    {
    public:
        explicit Editor (PadlabProcessor& p) : juce::AudioProcessorEditor (p), panel (p.getEngine())
        {
            addAndMakeVisible (panel);
            setSize (PadlabProcessor::kPanelWidth, PadlabProcessor::kPanelHeight);
        }
        void resized() override { panel.setBounds (getLocalBounds()); }

    private:
        MainComponent panel;
    };

    juce::File scratchFolder()
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("padlab-state-" + juce::Uuid().toString());
    }
} // namespace

PadlabProcessor::PadlabProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Rec In", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Stereo Out", juce::AudioChannelSet::stereo(), true))
{
}

PadlabProcessor::~PadlabProcessor() = default;

bool PadlabProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    return layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
           && (layout.getMainInputChannelSet().isDisabled() || layout.getMainInputChannelSet() == juce::AudioChannelSet::stereo());
}

void PadlabProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    inputCopy.setSize (2, juce::jmax (1, maxBlock));
    engine.prepareHosted (sampleRate, maxBlock);
}

void PadlabProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const int n = buffer.getNumSamples();
    const int numIn = n <= inputCopy.getNumSamples() ? juce::jmin (2, getTotalNumInputChannels()) : 0;
    for (int c = 0; c < numIn; ++c)
        inputCopy.copyFrom (c, 0, buffer, c, 0, n); // (the inputs share channels with the outputs)
    const float* ins[2] = { inputCopy.getReadPointer (0), inputCopy.getReadPointer (numIn > 1 ? 1 : 0) };
    float* outs[2] = { buffer.getWritePointer (0), buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr };
    engine.processHosted (ins, numIn, outs, outs[1] != nullptr ? 2 : 1, n, midi);
}

juce::AudioProcessorEditor* PadlabProcessor::createEditor() { return new Editor (*this); }

void PadlabProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    // the project as Padlab saves it (copied under the lock, written outside it), then zipped up
    projectio::SaveJob job;
    engine.withProject ([&] (Project& p) { job = projectio::prepareSave (p); });
    const auto folder = scratchFolder();
    juce::String error;
    if (! projectio::writeSave (job, folder, error))
        return;
    auto* meta = new juce::DynamicObject();
    meta->setProperty ("format", "padlab-state");
    meta->setProperty ("mainVolume", engine.getMainVolume());
    meta->setProperty ("bank", engine.getCurrentBank());
    meta->setProperty ("input", engine.isInputEnabled());
    folder.getChildFile ("studio-state.json").replaceWithText (juce::JSON::toString (juce::var (meta), true));

    juce::ZipFile::Builder zip;
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, true))
        zip.addFile (f, f.getFileExtension() == ".wav" ? 0 : 6, f.getRelativePathFrom (folder).replaceCharacter ('\\', '/'));
    juce::MemoryOutputStream mos (dest, false);
    zip.writeToStream (mos, nullptr);
    folder.deleteRecursively();
}

void PadlabProcessor::setStateInformation (const void* data, int size)
{
    juce::MemoryInputStream mis (data, (size_t) size, false);
    juce::ZipFile zip (mis);
    if (zip.getNumEntries() == 0)
        return;
    const auto folder = scratchFolder();
    if (zip.uncompressTo (folder).failed())
    {
        folder.deleteRecursively();
        return;
    }
    const auto meta = juce::JSON::parse (folder.getChildFile ("studio-state.json"));
    auto project = std::make_unique<Project>();
    juce::String error;
    if (meta.getProperty ("format", {}).toString() == "padlab-state" && projectio::load (folder, *project, error))
    {
        engine.replaceProject (std::move (project));
        engine.setMainVolume ((float) (double) meta.getProperty ("mainVolume", 0.8));
        engine.setCurrentBank ((int) meta.getProperty ("bank", 0));
        engine.setInputEnabled ((bool) meta.getProperty ("input", false));
    }
    folder.deleteRecursively();
}
} // namespace padlab
