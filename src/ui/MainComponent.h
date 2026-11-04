#pragma once

#include "ui/AppContext.h"
#include "ui/DataWheel.h"
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
    void renderBackground (juce::Graphics&, float scale);
    void showScreen (ScreenMode m);
    void flash (const juce::String& message);
    void loadQLinksFromPad();
    void qlinkChanged (int knob);
    void tapTempo();
    void recordPressed (RecordMode mode);
    void setBank (int b);
    void noteTouched (juce::Component* c);
    void dataStep (int steps);

    Engine& engine;
    AppContext ctx;
    theme::HardwareLookAndFeel lnf;
    theme::LcdLookAndFeel lcdLnf;
    juce::TooltipWindow tooltips { this, 600 };

    std::array<std::unique_ptr<Screen>, 5> screens;
    juce::Rectangle<int> screenArea, lcdArea, statusArea, rightPanel, brandArea, qlinkArea, padModeArea, bankArea,
        modeArea, transportArea, fileArea, deckArea, wheelArea;
    theme::LcdGlass glass;
    theme::LcdInkShadow lcdInkShadow;

    /** Fast-changing LCD elements (playhead, meters) drawn above the screen content,
        so playback doesn't re-render the whole LCD every frame. */
    struct LiveOverlay : juce::Component
    {
        LiveOverlay() { setInterceptsMouseClicks (false, false); }
        void update (Screen::LiveOverlay next)
        {
            auto dirty = [this] (juce::Rectangle<int> r) { if (! r.isEmpty()) repaint (r.expanded (3)); };
            if (next.playhead != state.playhead)
            {
                dirty (state.playhead);
                dirty (next.playhead);
            }
            const auto n = std::max (state.meters.size(), next.meters.size());
            for (size_t i = 0; i < n; ++i)
            {
                const bool same = i < state.meters.size() && i < next.meters.size() && state.meters[i] == next.meters[i];
                if (! same)
                {
                    if (i < state.meters.size()) dirty (state.meters[i].area);
                    if (i < next.meters.size()) dirty (next.meters[i].area);
                }
            }
            state = std::move (next);
        }
        void setCursor (juce::Rectangle<int> r)
        {
            if (r == cursor)
                return;
            if (! cursor.isEmpty()) repaint (cursor.expanded (6));
            cursor = r;
            if (! cursor.isEmpty()) repaint (cursor.expanded (6));
        }
        void paint (juce::Graphics& g) override
        {
            auto inkRect = [&g] (juce::Rectangle<int> r) {
                g.setColour (theme::lcd::ink.withAlpha (0.18f));
                g.fillRect (r.translated (2, 2));
                g.setColour (theme::lcd::ink);
                g.fillRect (r);
            };
            for (auto& m : state.meters)
            {
                auto r = m.area;
                inkRect (m.vertical ? r.removeFromBottom ((int) std::round (r.getHeight() * m.level))
                                : r.removeFromLeft ((int) std::round (r.getWidth() * m.level)));
            }
            if (! state.playhead.isEmpty())
                inkRect (state.playhead);
            if (! cursor.isEmpty())
            {
                // DATA wheel cursor: corner brackets around the field the wheel edits
                const auto c = cursor.expanded (3);
                const int L = 6, t = 2;
                for (auto corner : { c.getTopLeft(), c.getTopRight(), c.getBottomLeft(), c.getBottomRight() })
                {
                    const int sx = corner.x == c.getX() ? 1 : -1, sy = corner.y == c.getY() ? 1 : -1;
                    const int x0 = sx > 0 ? corner.x : corner.x - L, y0 = sy > 0 ? corner.y : corner.y - t;
                    inkRect ({ x0, y0, L, t });
                    inkRect ({ sx > 0 ? corner.x : corner.x - t, sy > 0 ? corner.y : corner.y - L, t, L });
                }
            }
        }
        Screen::LiveOverlay state;
        juce::Rectangle<int> cursor;
    } playhead;
    int statusTick = 0;
    juce::Image background; // static chassis, rendered once per size
    juce::Label modeTitle, projectTitle, positionLabel, statusLabel, messageLabel;
    uint32_t messageUntil = 0;

    PadGrid pads;
    DataWheel dataWheel;
    juce::Component::SafePointer<juce::Component> dataTarget; // the value the DATA wheel edits

    struct TouchTracker : juce::MouseListener
    {
        explicit TouchTracker (MainComponent& m) : owner (m) {}
        void mouseDown (const juce::MouseEvent& e) override { owner.noteTouched (e.eventComponent); }
        MainComponent& owner;
    } touchTracker { *this };

    // Q-Links
    std::array<juce::Slider, 4> qlinks;
    std::array<juce::Label, 4> qlinkLabels;
    bool loadingQ = false;

    // pad modes
    theme::HardButton fullLevel { "FULL\nLEVEL" }, sixteenLevels { "16\nLEVELS" }, noteRepeat { "NOTE\nREPEAT" },
        erase { "ERASE" }, trackMute { "TRACK\nMUTE", theme::colours::ledGreen },
        padMute { "PAD\nMUTE", theme::colours::ledGreen };
    std::array<std::unique_ptr<theme::HardButton>, kNumBanks> banks;

    // screen modes
    using Cap = theme::HardButton::Cap;
    theme::HardButton mainBtn { "MAIN", theme::colours::ledRed, Cap::Blue },
        progBtn { "PROGRAM\nEDIT", theme::colours::ledRed, Cap::Blue },
        sampleBtn { "SAMPLE\nEDIT", theme::colours::ledRed, Cap::Blue },
        samplerBtn { "SAMPLER", theme::colours::ledRed, Cap::Blue }, mixerBtn { "MIXER", theme::colours::ledRed, Cap::Blue };

    // transport
    theme::HardButton recBtn { "REC", theme::colours::ledRed, Cap::Red },
        overdubBtn { "OVER\nDUB", theme::colours::ledRed, Cap::Red },
        stopBtn { "STOP", theme::colours::ledRed, Cap::White },
        playBtn { "PLAY", theme::colours::ledGreen, Cap::White },
        playStartBtn { "PLAY\nSTART", theme::colours::ledGreen, Cap::White },
        undoBtn { "UNDO", theme::colours::ledRed, Cap::Light }, tapBtn { "TAP\nTEMPO", theme::colours::ledRed, Cap::Light };

    // file
    juce::TextButton newBtn { "New" }, openBtn { "Open" }, saveBtn { "Save" }, settingsBtn { "Audio/MIDI" };

    std::unique_ptr<juce::FileChooser> chooser;
    std::array<bool, kPadsPerBank> keyDown {};
    std::vector<double> taps;
    uint32_t lastStopMs = 0;
};

} // namespace padlab
