#include "engine/Engine.h"
#include "ui/MainComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace padlab
{
class PadlabApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String&) override
    {
        engine = std::make_unique<Engine>();
        window = std::make_unique<MainWindow> (getApplicationName(), *engine);
    }

    void shutdown() override
    {
        window.reset();
        engine.reset();
    }

    void systemRequestedQuit() override { quit(); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, Engine& eng)
            : DocumentWindow (name, juce::Colour (0xff1b1d21), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (eng), true);
            setResizable (true, true);
            setResizeLimits (1180, 760, 2560, 1600);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
            if (auto* c = getContentComponent())
                c->grabKeyboardFocus();
        }
        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

private:
    std::unique_ptr<Engine> engine;
    std::unique_ptr<MainWindow> window;
};
} // namespace padlab

START_JUCE_APPLICATION (padlab::PadlabApplication)
