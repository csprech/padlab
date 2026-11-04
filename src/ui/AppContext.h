#pragma once

#include "engine/Engine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace padlab
{
enum class PadView { Play, TrackMute, PadMute };
enum class ScreenMode { Main, Program, SampleEdit, Sampler, Mixer, Song, StepEdit, Other };
constexpr int kNumScreens = 8;

/** UI-wide state shared by all screens. Listeners get a change message whenever it changes. */
struct AppContext : juce::ChangeBroadcaster
{
    explicit AppContext (Engine& e) : engine (e) {}

    Engine& engine;
    int selectedPad = 0; // absolute 0..127
    PadView padView = PadView::Play;
    ScreenMode screen = ScreenMode::Main;
    juce::File projectFolder;
    std::function<void (ScreenMode)> showScreen;
    std::function<void (const juce::String&)> flashMessage;
    /** Set by a screen that wants to know about pad hits (STEP EDIT inserts notes). */
    std::function<void (int pad, int velocity)> onPadHit;

    int currentTrack() const { return engine.getSettings().currentTrack; }
    int currentProgram()
    {
        const int t = currentTrack();
        return engine.withProject ([&] (Project& p) {
            return p.sequences[(size_t) engine.getCurrentSequence()].tracks[(size_t) t].programIndex;
        });
    }
    void setSelectedPad (int p)
    {
        p = juce::jlimit (0, kNumPads - 1, p);
        if (p != selectedPad)
        {
            selectedPad = p;
            sendChangeMessage();
        }
    }
    void changed() { sendChangeMessage(); }

    static juce::String padName (int pad)
    {
        return juce::String::charToString ((juce::juce_wchar) ('A' + pad / kPadsPerBank))
               + juce::String (pad % kPadsPerBank + 1).paddedLeft ('0', 2);
    }
};

} // namespace padlab
