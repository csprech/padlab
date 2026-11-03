#pragma once

#include "ui/AppContext.h"
#include "ui/PadGrid.h"
#include "ui/Screens.h"
#include "ui/Theme.h"

namespace padlab
{
class MainComponent : public juce::Component,
                      public juce::ChangeListener,
                      private juce::Timer
{
public:
    explicit MainComponent (Engine& engine);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }

    // file actions (also used by the menu bar)
    void newProject();
    void openProject();
    void saveProject (bool saveAs);
    void showAudioSettings();

private:
    void timerCallback() override;
    void showScreen (ScreenMode m);
    void flash (const juce::String& message);
    void loadQLinksFromPad();
    void qlinkChanged (int knob);
    void tapTempo();
    void recordPressed (RecordMode mode);
    void setBank (int b);

    Engine& engine;
    AppContext ctx;
    theme::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    std::array<std::unique_ptr<Screen>, 5> screens;
    juce::Rectangle<int> screenArea, statusArea, rightPanel, brandArea;
    juce::Label modeTitle, projectTitle, positionLabel, statusLabel, messageLabel;
    uint32_t messageUntil = 0;

    PadGrid pads;

    // Q-Links
    std::array<juce::Slider, 4> qlinks;
    std::array<juce::Label, 4> qlinkLabels;
    bool loadingQ = false;

    // pad modes
    theme::HardButton fullLevel { "FULL\nLEVEL" }, sixteenLevels { "16\nLEVELS" }, noteRepeat { "NOTE\nREPEAT" },
        erase { "ERASE", theme::colours::record }, trackMute { "TRACK\nMUTE", theme::colours::play },
        padMute { "PAD\nMUTE", theme::colours::accent2 };
    std::array<std::unique_ptr<theme::HardButton>, kNumBanks> banks;

    // screen modes
    theme::HardButton mainBtn { "MAIN", theme::colours::accent2 }, progBtn { "PROG\nEDIT", theme::colours::accent2 },
        sampleBtn { "SAMPLE\nEDIT", theme::colours::accent2 }, samplerBtn { "SAMPLER", theme::colours::accent2 },
        mixerBtn { "MIXER", theme::colours::accent2 };

    // transport
    theme::HardButton recBtn { "REC", theme::colours::record }, overdubBtn { "OVERDUB", theme::colours::record },
        stopBtn { "STOP" }, playBtn { "PLAY", theme::colours::play }, playStartBtn { "PLAY\nSTART", theme::colours::play },
        undoBtn { "UNDO" }, tapBtn { "TAP\nTEMPO" };

    // file
    juce::TextButton newBtn { "New" }, openBtn { "Open" }, saveBtn { "Save" }, settingsBtn { "Audio/MIDI" };

    std::unique_ptr<juce::FileChooser> chooser;
    std::array<bool, kPadsPerBank> keyDown {};
    std::vector<double> taps;
    uint32_t lastStopMs = 0;
};

} // namespace padlab
