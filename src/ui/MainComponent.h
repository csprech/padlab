#pragma once

#include "ui/AppContext.h"
#include "ui/DataWheel.h"
#include "ui/PadGrid.h"
#include "ui/PanelParts.h"
#include "ui/Screens.h"
#include "ui/Theme.h"

namespace padlab
{
/** The whole front panel, laid out like a classic pad sampler: LCD with soft keys and level knobs
    top-left, keypad / DATA wheel / CURSOR / LOCATE / transport below it, a floppy bay and vents
    top-right, and the pads in their plate with the bank / level / mute column beside them. */
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
    void moveCursor (int dx, int dy);
    void keypadPressed (int key); // 0..9, 10 = SHIFT, 11 = ENTER
    void commitEntry();
    void cancelEntry();
    void locateStep (int dir);
    void locateBar (int dir);
    void openWindow();
    void showDiskMenu();
    Screen& activeScreen() { return *screens[(size_t) ctx.screen]; }
    bool shiftActive() const;

    Engine& engine;
    AppContext ctx;
    theme::HardwareLookAndFeel lnf;
    theme::LcdLookAndFeel lcdLnf;
    juce::TooltipWindow tooltips { this, 600 };

    std::array<std::unique_ptr<Screen>, kNumScreens> screens;
    juce::Rectangle<int> brandArea, screenArea, lcdArea, statusArea, tabArea, knobArea, softKeyArea, lowerArea, nvArea,
        keypadArea, wheelArea, clusterArea, cursorArea, locateArea, transportArea, rightPanel, topRightArea, ventArea,
        badgeArea, qlinkArea, plateArea, stripArea, padsArea;
    theme::LcdGlass glass;
    theme::LcdInkShadow lcdInkShadow;
    SoftKeyTabs softTabs;

    /** Fast-changing LCD elements (playhead, meters, data cursor) drawn above the screen content,
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
                // DATA cursor: corner brackets around the field the wheel / keypad edits
                const auto c = cursor.expanded (3);
                const int L = 6, t = 2;
                for (auto corner : { c.getTopLeft(), c.getTopRight(), c.getBottomLeft(), c.getBottomRight() })
                {
                    const int sx = corner.x == c.getX() ? 1 : -1, sy = corner.y == c.getY() ? 1 : -1;
                    inkRect ({ sx > 0 ? corner.x : corner.x - L, sy > 0 ? corner.y : corner.y - t, L, t });
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
    CursorPad cursorPad;
    FloppyDrive floppy;
    BankLeds bankLeds;
    juce::Component::SafePointer<juce::Component> dataTarget; // the field the DATA wheel / keypad edits

    struct TouchTracker : juce::MouseListener
    {
        explicit TouchTracker (MainComponent& m) : owner (m) {}
        void mouseDown (const juce::MouseEvent& e) override { owner.noteTouched (e.eventComponent); }
        MainComponent& owner;
    } touchTracker { *this };

    // level knobs and Q-Links
    juce::Slider mainVolume, recGain;
    std::array<juce::Slider, 4> qlinks;
    std::array<juce::Label, 4> qlinkLabels;
    bool loadingQ = false;

    // note variation
    juce::Slider nvFader;
    using Cap = theme::HardButton::Cap;
    theme::HardButton afterBtn { "AFTER", theme::colours::ledRed, Cap::Light };

    // keypad: 7 8 9 / 4 5 6 / 1 2 3 / SHIFT 0 ENTER
    std::array<std::unique_ptr<theme::HardButton>, 12> keypad;
    bool shiftLatched = false;
    enum class Entry { None, Value, GoTo };
    Entry entry = Entry::None;
    juce::String entryDigits;

    theme::HardButton mainScreenBtn { "MAIN\nSCREEN", theme::colours::ledRed, Cap::Light },
        openWindowBtn { "OPEN\nWINDOW", theme::colours::ledRed, Cap::Light };
    theme::HardButton tapBtn { "TAP TEMPO\nNOTE REPEAT", theme::colours::ledRed, Cap::Light },
        undoBtn { "UNDO\nSEQ", theme::colours::ledRed, Cap::Light }, eraseBtn { "ERASE", theme::colours::ledRed, Cap::Light };
    uint32_t tapPressedAt = 0;
    bool repeatWasLatched = false, padHitWhileTapHeld = false;

    // locate
    theme::HardButton stepBackBtn { "STEP\n<", theme::colours::ledRed, Cap::Light },
        stepFwdBtn { "STEP\n>", theme::colours::ledRed, Cap::Light }, gotoBtn { "GO TO", theme::colours::ledRed, Cap::Light },
        barBackBtn { "BAR\n<<", theme::colours::ledRed, Cap::Light }, barFwdBtn { "BAR\n>>", theme::colours::ledRed, Cap::Light };

    // transport
    theme::HardButton recBtn { "REC", theme::colours::ledRed, Cap::Red },
        overdubBtn { "OVER\nDUB", theme::colours::ledRed, Cap::Red },
        stopBtn { "STOP", theme::colours::ledRed, Cap::White },
        playBtn { "PLAY", theme::colours::ledGreen, Cap::White },
        playStartBtn { "PLAY\nSTART", theme::colours::ledGreen, Cap::White };

    // soft keys under the LCD
    std::array<std::unique_ptr<theme::HardButton>, 6> softKeys;

    // pad plate column
    theme::HardButton padBankBtn { "PAD\nBANK", theme::colours::ledRed, Cap::Light },
        fullLevel { "FULL\nLEVEL" }, sixteenLevels { "16\nLEVELS" },
        trackMute { "TRACK\nMUTE", theme::colours::ledGreen }, padMute { "PAD\nMUTE", theme::colours::ledGreen };

    std::unique_ptr<juce::FileChooser> chooser;
    std::array<bool, kPadsPerBank> keyDown {};
    std::vector<double> taps;
    uint32_t lastStopMs = 0;
};

} // namespace padlab
