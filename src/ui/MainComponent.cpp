#include "ui/MainComponent.h"
#include "core/DrumSynth.h"
#include "core/ProjectIO.h"
#include "core/Timing.h"
#include "ui/Materials.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace padlab
{
namespace
{
// computer keyboard -> pads (bottom row = pads 1-4, like the hardware)
constexpr char kPadKeys[kPadsPerBank] = { 'z', 'x', 'c', 'v', 'a', 's', 'd', 'f', 'q', 'w', 'e', 'r', '1', '2', '3', '4' };

const char* kQLinkNames[4] = { "TUNE", "CUTOFF", "RESO", "DECAY" };

// soft keys F1..F6 select these screens; their labels sit on the bottom row of the LCD
constexpr ScreenMode kSoftKeyScreens[6] = { ScreenMode::Main, ScreenMode::StepEdit, ScreenMode::Song,
                                            ScreenMode::Program, ScreenMode::SampleEdit, ScreenMode::Mixer };
const char* kSoftKeyLabels[6] = { "MAIN", "STEP", "SONG", "PROGRAM", "TRIM", "MIXER" };

// keypad layout (top-left first) and what SHIFT + key opens, printed under each key
constexpr int kKeypadOrder[12] = { 7, 8, 9, 4, 5, 6, 1, 2, 3, 10, 0, 11 };
const char* kKeypadModes[10] = { "EDIT", "SONG", "STEP", "DISK", "SAMPLE", "TRIM", "PROGRAM", "MIXER", "OTHER", "MIDI" };

} // namespace

MainComponent::MainComponent (Engine& e) : engine (e), ctx (e), pads (ctx)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    ctx.addChangeListener (this);
    ctx.showScreen = [this] (ScreenMode m) { showScreen (m); };
    ctx.flashMessage = [this] (const juce::String& m) { flash (m); };

    screens[(size_t) ScreenMode::Main] = createMainScreen (ctx);
    screens[(size_t) ScreenMode::Program] = createProgramScreen (ctx);
    screens[(size_t) ScreenMode::SampleEdit] = createSampleScreen (ctx);
    screens[(size_t) ScreenMode::Sampler] = createSamplerScreen (ctx);
    screens[(size_t) ScreenMode::Mixer] = createMixerScreen (ctx);
    screens[(size_t) ScreenMode::Song] = createSongScreen (ctx);
    screens[(size_t) ScreenMode::StepEdit] = createStepEditScreen (ctx);
    screens[(size_t) ScreenMode::Other] = createOtherScreen (ctx);
    screens[(size_t) ScreenMode::Help] = createHelpScreen (ctx);
    screens[(size_t) ScreenMode::Edit] = createEditScreen (ctx);
    for (auto& s : screens)
    {
        s->setLookAndFeel (&lcdLnf);
        s->setCachedComponentImage (new theme::LcdInkCache (*s));
        addChildComponent (*s);
    }

    for (auto* l : { &modeTitle, &projectTitle, &positionLabel, &statusLabel, &messageLabel })
    {
        addAndMakeVisible (l);
        l->setInterceptsMouseClicks (false, false);
    }
    for (auto* l : { &positionLabel, &statusLabel, &messageLabel })
        l->setComponentEffect (&lcdInkShadow);
    modeTitle.setFont (theme::screenFont (14.0f));
    modeTitle.setColour (juce::Label::textColourId, theme::lcd::backlight);
    projectTitle.setFont (theme::screenFont (14.0f));
    projectTitle.setColour (juce::Label::textColourId, theme::lcd::backlight);
    projectTitle.setJustificationType (juce::Justification::centredRight);
    positionLabel.setFont (theme::screenFont (20.0f));
    positionLabel.setColour (juce::Label::textColourId, theme::lcd::ink);
    statusLabel.setFont (theme::screenFont (13.0f));
    statusLabel.setColour (juce::Label::textColourId, theme::lcd::textDim);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    messageLabel.setFont (theme::screenFont (14.0f));
    messageLabel.setColour (juce::Label::textColourId, theme::lcd::ink);
    messageLabel.setJustificationType (juce::Justification::centred);

    juce::StringArray tabLabels;
    for (auto* l : kSoftKeyLabels)
        tabLabels.add (l);
    softTabs.setLabels (tabLabels);
    addAndMakeVisible (softTabs);

    addAndMakeVisible (pads);

    // ---- level knobs
    for (auto* k : { &mainVolume, &recGain })
    {
        k->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (k);
    }
    mainVolume.setRange (0.0, 1.0, 0.001);
    mainVolume.setValue (engine.getMainVolume(), juce::dontSendNotification);
    mainVolume.onValueChange = [this] { engine.setMainVolume ((float) mainVolume.getValue()); };
    mainVolume.setTooltip ("MAIN VOLUME: output level");
    recGain.setRange (-12.0, 24.0, 0.1);
    recGain.setValue (0.0, juce::dontSendNotification);
    recGain.onValueChange = [this] {
        engine.setRecGainDb ((float) recGain.getValue());
        flash ("REC GAIN " + juce::String (recGain.getValue(), 1) + " dB");
    };
    recGain.setTooltip ("REC GAIN: audio input level for sampling");
    recGain.getProperties().set ("capColour", (juce::int64) 0xffb0463c);

    // ---- Q-Links
    for (int i = 0; i < 4; ++i)
    {
        auto& q = qlinks[(size_t) i];
        q.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        q.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
        q.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        q.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        q.onValueChange = [this, i] { qlinkChanged (i); };
        addAndMakeVisible (q);
        auto& l = qlinkLabels[(size_t) i];
        l.setText (kQLinkNames[i], juce::dontSendNotification);
        l.setFont (theme::labelFont (11.5f));
        l.setColour (juce::Label::textColourId, theme::colours::silk);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    }
    qlinks[0].setRange (-24.0, 24.0, 0.01);
    qlinks[0].setTextValueSuffix (" st");
    qlinks[1].setRange (20.0, 20000.0, 1.0);
    qlinks[1].setSkewFactorFromMidPoint (1000.0);
    qlinks[1].setTextValueSuffix (" Hz");
    qlinks[2].setRange (0.0, 100.0, 1.0);
    qlinks[2].setTextValueSuffix (" %");
    qlinks[3].setRange (5.0, 5000.0, 1.0);
    qlinks[3].setSkewFactorFromMidPoint (300.0);
    qlinks[3].setTextValueSuffix (" ms");
    engine.onMidiDeviceMessage = [this] (const juce::String& m) { flash (m); };
    engine.onQLinkMidi = [this] (int knob, int value) {
        if (knob == 4)
            return nvFader.setValue (value);
        auto& q = qlinks[(size_t) knob];
        q.setValue (q.proportionOfLengthToValue (value / 127.0));
    };

    // ---- note variation: fader + AFTER (on/off); SHIFT + AFTER = ASSIGN (which parameter)
    nvFader.setSliderStyle (juce::Slider::LinearVertical);
    nvFader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    nvFader.setRange (0.0, 127.0, 1.0);
    nvFader.setValue (64.0, juce::dontSendNotification);
    nvFader.setDoubleClickReturnValue (true, 64.0);
    nvFader.onValueChange = [this] {
        const int v = (int) nvFader.getValue();
        engine.updateSettings ([v] (PerformanceSettings& s) { s.noteVariationValue = v; });
        static const char* names[] = { "", "TUNE", "DECAY", "ATTACK", "FILTER" };
        const auto s = engine.getSettings();
        flash ("NOTE VARIATION " + juce::String (names[(int) s.noteVariationType]) + " " + juce::String (v - 64)
               + (s.noteVariationOn ? "" : "  (press AFTER to enable)"));
    };
    nvFader.setTooltip ("NOTE VARIATION: varies tune / decay / attack / filter of the selected pad as you play it");
    addAndMakeVisible (nvFader);
    afterBtn.setSubLabel ("ASSIGN");
    afterBtn.setLegendLines (1);
    afterBtn.onClick = [this] {
        if (shiftActive())
        {
            shiftLatched = false;
            static const char* names[] = { "", "TUNE", "DECAY", "ATTACK", "FILTER" };
            engine.updateSettings ([] (PerformanceSettings& s) {
                s.noteVariationType = (VariationType) ((int) s.noteVariationType % 4 + 1);
            });
            flash ("NOTE VARIATION ASSIGN: " + juce::String (names[(int) engine.getSettings().noteVariationType]));
            return;
        }
        const int pad = ctx.selectedPad;
        engine.updateSettings ([pad] (PerformanceSettings& s) {
            s.noteVariationOn = ! s.noteVariationOn;
            if (s.noteVariationPad >= 0)
                s.noteVariationPad = pad;
        });
        flash (engine.getSettings().noteVariationOn ? "NOTE VARIATION ON (" + AppContext::padName (pad) + ")" : "NOTE VARIATION OFF");
    };
    addAndMakeVisible (afterBtn);

    // ---- keypad
    for (int i = 0; i < 12; ++i)
    {
        const int key = kKeypadOrder[i];
        const juce::String text = key == 10 ? "SHIFT" : key == 11 ? "ENTER" : juce::String (key);
        auto& b = keypad[(size_t) i];
        b = std::make_unique<theme::HardButton> (text, theme::colours::ledRed,
                                                 key == 10 ? Cap::Dark : key == 11 ? Cap::Light : Cap::Blue);
        b->setLegendOnCap (true);
        b->setCapAspect (1.7f);
        b->setHasLed (key == 10);
        if (key < 10)
            b->setSubLabel (kKeypadModes[key]);
        b->onClick = [this, key] { keypadPressed (key); };
        addAndMakeVisible (*b);
    }

    // ---- DATA area
    mainScreenBtn.setHasLed (false);
    mainScreenBtn.onClick = [this] { cancelEntry(); showScreen (ScreenMode::Main); };
    openWindowBtn.setHasLed (false);
    openWindowBtn.onClick = [this] { openWindow(); };
    dataWheel.onStep = [this] (int steps) { dataStep (steps); };
    cursorPad.onMove = [this] (int dx, int dy) { moveCursor (dx, dy); };
    for (auto* c : std::initializer_list<juce::Component*> { &mainScreenBtn, &openWindowBtn, &dataWheel, &cursorPad })
        addAndMakeVisible (c);

    // TAP TEMPO / NOTE REPEAT: tap it for tempo, hold it while playing pads for note repeat
    tapBtn.setTooltip ("Tap for tempo. Hold while playing pads for note repeat (N latches it).");
    tapBtn.onStateChange = [this] {
        const bool down = tapBtn.isDown();
        if (down && tapPressedAt == 0)
        {
            tapPressedAt = juce::Time::getMillisecondCounter();
            repeatWasLatched = engine.getSettings().noteRepeat;
            padHitWhileTapHeld = false;
            engine.updateSettings ([] (PerformanceSettings& s) { s.noteRepeat = true; });
        }
        else if (! down && tapPressedAt != 0)
        {
            const auto held = juce::Time::getMillisecondCounter() - tapPressedAt;
            tapPressedAt = 0;
            engine.updateSettings ([l = repeatWasLatched] (PerformanceSettings& s) { s.noteRepeat = l; });
            if (held < 350 && ! padHitWhileTapHeld)
                tapTempo();
        }
    };
    undoBtn.setHasLed (false);
    undoBtn.onClick = [this] {
        const auto what = engine.getUndoDescription();
        flash (engine.undo() ? "Undo " + what : "Nothing to undo");
        ctx.changed();
    };
    eraseBtn.setTooltip ("Hold ERASE and hold pads while playing to delete their notes");
    eraseBtn.onStateChange = [this] {
        const bool down = eraseBtn.isDown();
        engine.updateSettings ([down] (PerformanceSettings& s) { s.eraseHeld = down; });
    };
    for (auto* b : { &tapBtn, &undoBtn, &eraseBtn })
    {
        b->setCapAspect (1.6f);
        addAndMakeVisible (b);
    }

    // ---- LOCATE
    stepBackBtn.onClick = [this] { locateStep (-1); };
    stepFwdBtn.onClick = [this] { locateStep (1); };
    barBackBtn.onClick = [this] { locateBar (-1); };
    barFwdBtn.onClick = [this] { locateBar (1); };
    gotoBtn.onClick = [this] {
        if (entry == Entry::GoTo && entryDigits.isNotEmpty())
            return commitEntry();
        entry = Entry::GoTo;
        entryDigits.clear();
        messageUntil = 0;
        messageLabel.setText ("GO TO BAR: _   (keypad, then ENTER)", juce::dontSendNotification);
    };
    for (auto* b : { &stepBackBtn, &stepFwdBtn, &gotoBtn, &barBackBtn, &barFwdBtn })
    {
        b->setHasLed (false);
        b->setCapAspect (1.6f);
        addAndMakeVisible (b);
    }

    // ---- transport
    recBtn.onClick = [this] { recordPressed (RecordMode::Record); };
    overdubBtn.onClick = [this] { recordPressed (RecordMode::Overdub); };
    stopBtn.setHasLed (false);
    stopBtn.onClick = [this] {
        const auto now = juce::Time::getMillisecondCounter();
        if (engine.getTransportState() == Transport::State::Stopped && now - lastStopMs < 400)
            engine.panic(); // double-stop: silence everything
        lastStopMs = now;
        engine.stop();
    };
    playBtn.onClick = [this] { engine.play (false); };
    playStartBtn.onClick = [this] { engine.play (true); };
    for (auto* b : { &recBtn, &overdubBtn, &stopBtn, &playBtn, &playStartBtn })
    {
        b->setCapAspect (1.35f); // big, nearly square transport keys
        addAndMakeVisible (b);
    }

    // ---- soft keys
    for (int i = 0; i < 6; ++i)
    {
        auto& k = softKeys[(size_t) i];
        k = std::make_unique<theme::HardButton> ("F" + juce::String (i + 1), theme::colours::ledRed, Cap::Light);
        k->setHasLed (false);
        k->setLegendLines (1);
        k->onClick = [this, i] { cancelEntry(); showScreen (kSoftKeyScreens[i]); };
        addAndMakeVisible (*k);
    }

    // ---- pad plate column
    bankLeds.onSelect = [this] (int b) { setBank (b); };
    addAndMakeVisible (bankLeds);
    padBankBtn.setHasLed (false);
    padBankBtn.setTooltip ("Next pad bank (SHIFT: previous). Or click a bank letter.");
    padBankBtn.onClick = [this] {
        const int b = engine.getCurrentBank();
        const bool back = shiftActive();
        shiftLatched = false;
        setBank ((b + (back ? kNumBanks - 1 : 1)) % kNumBanks);
    };
    fullLevel.onClick = [this] { engine.updateSettings ([] (PerformanceSettings& s) { s.fullLevel = ! s.fullLevel; }); };
    sixteenLevels.setTooltip ("Spread the selected pad over all 16 pads (by velocity, tune, decay, attack or filter: see OTHER)");
    sixteenLevels.onClick = [this] {
        const int src = ctx.selectedPad;
        engine.updateSettings ([&] (PerformanceSettings& s) {
            s.sixteenLevels = ! s.sixteenLevels;
            s.sixteenLevelsSource = src;
        });
        const auto s = engine.getSettings();
        static const char* by[] = { "velocity", "tune", "decay", "attack", "filter" };
        if (s.sixteenLevels)
            flash ("16 LEVELS: " + AppContext::padName (src) + " by " + by[juce::jlimit (0, 4, s.sixteenLevelsParam)]);
    };
    trackMute.onClick = [this] {
        ctx.padView = ctx.padView == PadView::TrackMute ? PadView::Play : PadView::TrackMute;
        ctx.changed();
    };
    padMute.onClick = [this] {
        ctx.padView = ctx.padView == PadView::PadMute ? PadView::Play : PadView::PadMute;
        ctx.changed();
    };
    for (auto* b : { &padBankBtn, &fullLevel, &sixteenLevels, &trackMute, &padMute })
        addAndMakeVisible (b);

    helpBtn.setHasLed (false);
    helpBtn.setLegendLines (1);
    helpBtn.setTooltip ("How to load pads, chop, record, make songs... (or press ?)");
    helpBtn.onClick = [this] { showHelp(); };
    addAndMakeVisible (helpBtn);

    floppy.onClick = [this] { showDiskMenu(); };
    floppy.setTooltip ("DISK: new, open, save, audio/MIDI setup");
    addAndMakeVisible (floppy);

    // worn from use: the keys you hit all day are polished smooth
    playBtn.setWear (0.7f);
    stopBtn.setWear (0.8f);
    recBtn.setWear (0.45f);
    overdubBtn.setWear (0.35f);
    mainScreenBtn.setWear (0.4f);
    softKeys[0]->setWear (0.3f);

    addMouseListener (&touchTracker, true);

    addAndMakeVisible (playhead);
    addAndMakeVisible (glass); // LCD glass sits on top of the screen contents

    // open at a comfortable size for the display it's on
    auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const auto area = display != nullptr ? display->userBounds.toNearestInt() : juce::Rectangle<int> (0, 0, 1440, 900);
    setSize (juce::jlimit (1280, 1640, (int) (area.getWidth() * 0.96f)), juce::jlimit (800, 1010, (int) (area.getHeight() * 0.93f)));
    if (auto* forced = std::getenv ("PADLAB_WINDOW_SIZE")) // e.g. "1280x800", for testing layouts
    {
        const auto wh = juce::StringArray::fromTokens (forced, "x", {});
        if (wh.size() == 2)
            setSize (juce::jmax (1280, wh[0].getIntValue()), juce::jmax (800, wh[1].getIntValue()));
    }
    showScreen (ScreenMode::Main);
    loadQLinksFromPad();

    // Keys belong to the pads: only text fields may take keyboard focus.
    std::function<void (juce::Component&)> noFocus = [&noFocus] (juce::Component& c) {
        for (auto* child : c.getChildren())
        {
            if (dynamic_cast<juce::TextEditor*> (child) == nullptr)
            {
                child->setWantsKeyboardFocus (false);
                child->setMouseClickGrabsKeyboardFocus (false);
            }
            noFocus (*child);
        }
    };
    noFocus (*this);

    startTimerHz (30);
}

void MainComponent::parentHierarchyChanged()
{
    juce::Component::SafePointer<MainComponent> safe (this);
    juce::MessageManager::callAsync ([safe] {
        if (safe != nullptr && safe->isShowing())
            safe->grabKeyboardFocus();
    });
}

MainComponent::~MainComponent()
{
    stopTimer();
    removeMouseListener (&touchTracker);
    ctx.removeChangeListener (this);
    engine.onQLinkMidi = nullptr;
    engine.onMidiDeviceMessage = nullptr;
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

// ---------------------------------------------------------------- layout

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (24, 20);
    rightPanel = r.removeFromRight (juce::jlimit (470, 640, (int) (r.getWidth() * 0.40f)));
    r.removeFromRight (24);

    // ---- left: brand, then [level knobs | LCD], soft keys, and the lower control block
    auto left = r;
    brandArea = left.removeFromTop (40);
    left.removeFromTop (4);
    lowerArea = left.removeFromBottom (juce::jlimit (250, 296, (int) (left.getHeight() * 0.33f)));
    left.removeFromBottom (6);
    softKeyArea = left.removeFromBottom (42);
    knobArea = left.removeFromLeft (84);
    left.removeFromLeft (10);
    screenArea = left;
    lcdArea = screenArea.reduced (16, 14);
    glass.setBounds (lcdArea);
    playhead.setBounds (lcdArea);

    auto inner = lcdArea.reduced (6);
    auto titleBar = inner.removeFromTop (22);
    modeTitle.setBounds (titleBar.removeFromLeft (titleBar.getWidth() / 2).withTrimmedLeft (6));
    projectTitle.setBounds (titleBar.withTrimmedRight (6));
    tabArea = inner.removeFromBottom (22);
    statusArea = inner.removeFromBottom (28);
    auto st = statusArea.reduced (6, 2);
    positionLabel.setBounds (st.removeFromLeft (200));
    statusLabel.setBounds (st.removeFromRight (juce::jmin (360, st.getWidth() / 2)));
    messageLabel.setBounds (st);
    // Screens are designed for an LCD about 790 x 360 px; on a smaller panel they're scaled down
    // as a whole (like a smaller display), never squeezed.
    {
        const float scale = juce::jmin (1.0f, inner.getWidth() / 790.0f, inner.getHeight() / 360.0f);
        const int w = (int) std::floor (inner.getWidth() / scale), h = (int) std::floor (inner.getHeight() / scale);
        for (auto& s : screens)
        {
            s->setTransform ({});
            s->setBounds (0, 0, w, h);
            s->setTransform (juce::AffineTransform::scale (scale).translated ((float) inner.getX(), (float) inner.getY()));
        }
    }
    softTabs.setBounds (tabArea);

    // soft keys line up under their LCD labels
    {
        std::vector<int> centres;
        const int cellW = tabArea.getWidth() / 6;
        const int kw = juce::jmin (84, cellW - 10);
        for (int i = 0; i < 6; ++i)
        {
            const int cx = tabArea.getX() + cellW * i + cellW / 2;
            centres.push_back (cx - tabArea.getX());
            softKeys[(size_t) i]->setBounds (juce::Rectangle<int> (kw, softKeyArea.getHeight()).withCentre ({ cx, softKeyArea.getCentreY() }));
        }
        softTabs.setKeyCentres (centres);
    }

    // level knobs beside the LCD
    {
        auto k = knobArea.withTrimmedTop (20);
        auto vol = k.removeFromTop (100);
        mainVolume.setBounds (vol.withTrimmedTop (16).withSizeKeepingCentre (70, 70));
        k.removeFromTop (14);
        auto gain = k.removeFromTop (100);
        recGain.setBounds (gain.withTrimmedTop (16).withSizeKeepingCentre (70, 70));
    }

    // ---- lower block: [note variation] [keypad] [data] [keys + cursor], then [locate | transport]
    {
        auto lo = lowerArea;
        nvArea = lo.removeFromLeft (78);
        lo.removeFromLeft (14);
        auto rowB = lo.removeFromBottom (78);
        lo.removeFromBottom (8);
        keypadArea = lo.removeFromLeft (juce::jmin (210, (int) (lo.getWidth() * 0.31f)));
        lo.removeFromLeft (14);
        wheelArea = lo.removeFromLeft (juce::jmin (176, lo.getHeight() + 6));
        lo.removeFromLeft (14);
        clusterArea = lo;
        cursorArea = clusterArea.removeFromRight (juce::jmin (128, clusterArea.getWidth() / 2));
        clusterArea.removeFromRight (8);

        locateArea = rowB.removeFromLeft (rowB.getWidth() / 2 - 8);
        rowB.removeFromLeft (16);
        transportArea = rowB;

        // note variation: AFTER key on top, fader below
        auto nv = nvArea.reduced (6, 4).withTrimmedTop (26);
        afterBtn.setBounds (nv.removeFromTop (58));
        nv.removeFromTop (4);
        nvFader.setBounds (nv.reduced (4, 2));

        // keypad 3 x 4
        auto kp = keypadArea;
        const int kw = kp.getWidth() / 3, kh = kp.getHeight() / 4;
        for (int i = 0; i < 12; ++i)
            keypad[(size_t) i]->setBounds (kp.getX() + (i % 3) * kw, kp.getY() + (i / 3) * kh, kw, kh);

        // MAIN SCREEN / OPEN WINDOW above the wheel
        auto w = wheelArea;
        auto top = w.removeFromTop (46);
        mainScreenBtn.setBounds (top.removeFromLeft (top.getWidth() / 2));
        openWindowBtn.setBounds (top);
        w.removeFromTop (14);
        dataWheel.setBounds (w);

        // TAP / UNDO / ERASE stacked
        auto c = clusterArea;
        const int bh = c.getHeight() / 3;
        tapBtn.setBounds (c.removeFromTop (bh));
        undoBtn.setBounds (c.removeFromTop (bh));
        eraseBtn.setBounds (c);
        cursorPad.setBounds (cursorArea.withTrimmedTop (18).withSizeKeepingCentre (
            juce::jmin (cursorArea.getWidth(), 120), juce::jmin (cursorArea.getHeight() - 18, 136)));

        auto placeRow = [] (juce::Rectangle<int> row, std::initializer_list<juce::Component*> keys) {
            row = row.withTrimmedTop (14);
            const int n = (int) keys.size();
            const int kwid = row.getWidth() / n;
            for (auto* k : keys)
                k->setBounds (row.removeFromLeft (kwid));
        };
        placeRow (locateArea, { &stepBackBtn, &stepFwdBtn, &gotoBtn, &barBackBtn, &barFwdBtn });
        placeRow (transportArea, { &recBtn, &overdubBtn, &stopBtn, &playBtn, &playStartBtn });
    }

    // ---- right: floppy + vents + badge, Q-Links, pad plate
    {
        auto rp = rightPanel;
        topRightArea = rp.removeFromTop (124);
        rp.removeFromTop (4);
        qlinkArea = rp.removeFromTop (106);
        rp.removeFromTop (8);
        plateArea = rp;

        auto tr = topRightArea;
        auto fl = tr.removeFromLeft ((int) (tr.getWidth() * 0.40f));
        floppy.setBounds (fl.withTrimmedTop (6).withHeight (58).reduced (2, 0));
        helpBtn.setBounds (fl.withTrimmedTop (70).withHeight (48).withSizeKeepingCentre (90, 48).withX (fl.getX()));
        tr.removeFromLeft (18);
        ventArea = tr.removeFromTop (66).withTrimmedTop (6);
        badgeArea = tr;

        auto qrow = qlinkArea.withTrimmedTop (18);
        const int qw = qrow.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto cell = qrow.removeFromLeft (qw).reduced (4, 0);
            qlinkLabels[(size_t) i].setBounds (cell.removeFromBottom (16));
            qlinks[(size_t) i].setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), cell.getHeight() + 16), cell.getHeight()));
        }

        auto plate = plateArea.reduced (10, 8);
        plate.removeFromTop (18); // printed DRUMS header
        stripArea = plate.removeFromLeft (84);
        plate.removeFromLeft (6);
        padsArea = plate;
        pads.setBounds (padsArea);

        auto s = stripArea.reduced (4, 6);
        bankLeds.setBounds (s.removeFromTop (78));
        s.removeFromTop (4);
        const int bh = juce::jmin (60, s.getHeight() / 5);
        for (auto* b : { &padBankBtn, &fullLevel, &sixteenLevels, &trackMute, &padMute })
            b->setBounds (s.removeFromTop (bh));
    }
    background = {};
}

namespace
{
    // a printed section header with a rule either side, like silkscreened panel groupings
    void sectionHeader (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> row, bool light = false)
    {
        using namespace padlab::theme;
        auto r = row.removeFromTop (14);
        g.setFont (labelFont (11.0f));
        const int tw = (int) std::ceil (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text)) + 14;
        auto textR = r.withSizeKeepingCentre (tw, r.getHeight());
        g.setColour ((light ? colours::silkLight : colours::silk).withAlpha (0.55f));
        const float y = (float) r.getCentreY();
        g.drawLine ((float) r.getX(), y, (float) textR.getX(), y, 1.0f);
        g.drawLine ((float) textR.getRight(), y, (float) r.getRight(), y, 1.0f);
        drawSilk (g, text, textR, juce::Justification::centred, 11.0f, light);
    }

    /** Bevelled opening: four chamfer faces lit by a top-left light. */
    void chamfer (juce::Graphics& g, juce::Rectangle<float> outer, float depth, bool recessed)
    {
        auto inner = outer.reduced (depth);
        auto face = [&] (juce::Point<float> a, juce::Point<float> b, juce::Point<float> c, juce::Point<float> d, juce::Colour col) {
            juce::Path p;
            p.startNewSubPath (a);
            p.lineTo (b);
            p.lineTo (c);
            p.lineTo (d);
            p.closeSubPath();
            g.setColour (col);
            g.fillPath (p);
        };
        const auto litC = juce::Colours::white.withAlpha (0.20f), darkC = juce::Colours::black.withAlpha (0.45f);
        const auto top = recessed ? darkC : litC, bottom = recessed ? litC : darkC;
        const auto left = recessed ? juce::Colours::black.withAlpha (0.32f) : juce::Colours::white.withAlpha (0.12f);
        const auto right = recessed ? juce::Colours::white.withAlpha (0.08f) : juce::Colours::black.withAlpha (0.25f);
        face (outer.getTopLeft(), outer.getTopRight(), inner.getTopRight(), inner.getTopLeft(), top);
        face (outer.getBottomLeft(), outer.getBottomRight(), inner.getBottomRight(), inner.getBottomLeft(), bottom);
        face (outer.getTopLeft(), outer.getBottomLeft(), inner.getBottomLeft(), inner.getTopLeft(), left);
        face (outer.getTopRight(), outer.getBottomRight(), inner.getBottomRight(), inner.getTopRight(), right);
    }

    /** A thin printed outline box with a title, grouping a section of controls. */
    void groupBox (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
    {
        using namespace padlab::theme;
        g.setColour (colours::silk.withAlpha (0.5f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3.0f, 1.0f);
        g.setFont (labelFont (10.0f));
        drawSilk (g, title, r.removeFromTop (24).reduced (2, 2), juce::Justification::centred, 10.0f);
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    const float sc = materials::scaleOf (g);
    const int w = (int) std::ceil (getWidth() * sc), h = (int) std::ceil (getHeight() * sc);
    if (background.getWidth() != w || background.getHeight() != h)
    {
        background = juce::Image (juce::Image::ARGB, std::max (1, w), std::max (1, h), true);
        juce::Graphics bg (background);
        bg.addTransform (juce::AffineTransform::scale (sc));
        renderBackground (bg, sc);
    }
    g.drawImageTransformed (background, juce::AffineTransform::scale (1.0f / sc));
}

void MainComponent::renderBackground (juce::Graphics& g, float sc)
{
    using namespace theme;
    const auto bounds = getLocalBounds().toFloat();
    const auto tileFill = [sc] (juce::Colour c, uint32_t seed) {
        const int size = 240;
        const auto& tile = materials::cached ("paint:" + c.toString() + ":" + juce::String ((int) seed),
                                              [=] { return materials::paintTile (c, size, 1.0f, seed); });
        return juce::FillType (tile, juce::AffineTransform::scale (1.0f / sc));
    };

    // ---- chassis: one powder-coated top panel lit from the top-left
    g.setFillType (tileFill (colours::chassis, 7));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), 0, 0, juce::Colours::transparentWhite,
                                             bounds.getWidth() * 0.7f, bounds.getHeight() * 0.8f, true));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0, bounds.getHeight() * 0.35f,
                                             juce::Colours::black.withAlpha (0.10f), 0, bounds.getBottom(), false));
    g.fillAll();
    chamfer (g, bounds, 5.0f, false);
    // the front lip: the panel folds down at the bottom edge
    {
        auto lip = bounds.withTop (bounds.getBottom() - 9.0f).reduced (5.0f, 0.0f).withTrimmedBottom (4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillRect (lip);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine (lip.getX(), lip.getY(), lip.getRight(), lip.getY(), 1.0f);
    }

    const float sr = 6.0f;
    drawScrew (g, { 14.0f, 13.0f }, sr, 0.4f);
    drawScrew (g, { bounds.getRight() - 14.0f, 13.0f }, sr, 1.2f);
    drawScrew (g, { 14.0f, bounds.getBottom() - 15.0f }, sr, 2.1f);
    drawScrew (g, { bounds.getRight() - 14.0f, bounds.getBottom() - 15.0f }, sr, 0.9f);

    // ---- brand (top-left) and product descriptor
    {
        auto b = brandArea.reduced (2, 0);
        const float h = 34.0f;
        juce::GlyphArrangement ga;
        ga.addLineOfText (labelFont (h), "PADLAB", (float) b.getX(), (float) b.getCentreY() + h * 0.32f);
        juce::Path wordmark;
        ga.createPath (wordmark);
        wordmark.applyTransform (juce::AffineTransform::shear (-0.18f, 0.0f).translated (b.getCentreY() * 0.18f, 0.0f));
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillPath (wordmark, juce::AffineTransform::translation (0.0f, 1.0f));
        g.setColour (colours::keyRed.darker (0.15f));
        g.fillPath (wordmark);
        auto desc = b.withTrimmedLeft ((int) wordmark.getBounds().getRight() - b.getX() + 20);
        drawSilk (g, "DRUM SAMPLER  /  960 PPQN MIDI SEQUENCER\nSAMPLING GROOVE WORKSTATION", desc.withWidth (juce::jmin (desc.getWidth(), 360)),
                  juce::Justification::centredLeft, 10.5f);
    }

    // ---- level knobs: printed names and MIN / MAX
    {
        auto k = knobArea.withTrimmedTop (20);
        auto vol = k.removeFromTop (100);
        drawSilk (g, "MAIN VOLUME", vol.removeFromTop (14), juce::Justification::centred, 10.0f);
        drawSilk (g, "MIN          MAX", vol.withTrimmedTop (vol.getHeight() - 12), juce::Justification::centred, 8.0f);
        k.removeFromTop (14);
        auto gain = k.removeFromTop (100);
        drawSilk (g, "REC GAIN", gain.removeFromTop (14), juce::Justification::centred, 10.0f);
        drawSilk (g, "MIN          MAX", gain.withTrimmedTop (gain.getHeight() - 12), juce::Justification::centred, 8.0f);
    }

    // ---- LCD bezel: moulded dark plastic with a chamfered window
    auto bez = screenArea.toFloat();
    juce::DropShadow (juce::Colours::black.withAlpha (0.40f), 10, { 0, 4 }).drawForRectangle (g, screenArea.reduced (6));
    {
        juce::Path bezPath;
        bezPath.addRoundedRectangle (bez, 8.0f);
        g.saveState();
        g.reduceClipRegion (bezPath);
        g.setFillType (tileFill (colours::bezel, 13));
        g.fillRect (bez);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0, bez.getY(),
                                                 juce::Colours::black.withAlpha (0.25f), 0, bez.getBottom(), false));
        g.fillRect (bez);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), 0, bez.getY(),
                                                 juce::Colours::transparentWhite, 0, bez.getY() + 6.0f, false));
        g.fillRect (bez.withHeight (6.0f));
        g.restoreState();
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (bez, 8.0f, 1.0f);
    }
    chamfer (g, lcdArea.toFloat().expanded (6.0f), 6.0f, true);

    // backlight, title bar, status rule and soft-key row
    auto lcdR = lcdArea.toFloat();
    g.setGradientFill (juce::ColourGradient (lcd::backlight.brighter (0.08f), lcdR.getCentreX(), lcdR.getCentreY() - lcdR.getHeight() * 0.15f,
                                             lcd::backlightLo.darker (0.05f), lcdR.getX(), lcdR.getBottom(), true));
    g.fillRect (lcdR);
    {
        auto inner = lcdArea.reduced (6);
        g.setColour (lcd::ink);
        g.fillRect (inner.removeFromTop (22));
        g.fillRect (statusArea.getX(), statusArea.getY(), statusArea.getWidth(), 2);
        g.fillRect (tabArea.getX(), tabArea.getY(), tabArea.getWidth(), 1);
    }

    // ---- lower block print
    groupBox (g, nvArea, "NOTE VARIATION");
    sectionHeader (g, "DATA", wheelArea.withTrimmedTop (46 + 2).withHeight (14));
    sectionHeader (g, "CURSOR", cursorArea.withHeight (14));
    sectionHeader (g, "LOCATE", locateArea);
    sectionHeader (g, "TRANSPORT", transportArea);

    // ---- top right: vents and the model badge
    drawVentGrille (g, ventArea.toFloat(), 4, 6);
    {
        auto b = badgeArea.reduced (0, 4);
        auto model = b.removeFromTop (34);
        g.setFont (labelFont (36.0f));
        const juce::String name ("PL-96");
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), name);
        auto nameR = model.removeFromRight ((int) tw + 4);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawText (name, nameR.translated (0, 1), juce::Justification::centredRight);
        g.setColour (colours::silk.withAlpha (0.85f));
        g.drawText (name, nameR, juce::Justification::centredRight);
        // two printed stripes under the model name
        auto stripe = b.removeFromTop (6).withLeft (nameR.getX() - 60);
        g.setColour (colours::keyRed);
        g.fillRect (stripe.removeFromTop (2));
        stripe.removeFromTop (1);
        g.setColour (colours::keyBlue.darker (0.3f));
        g.fillRect (stripe.removeFromTop (2));
        drawSilk (g, "SAMPLING GROOVE WORKSTATION", b.withTrimmedTop (2).withHeight (14), juce::Justification::centredRight, 11.0f);
    }
    sectionHeader (g, "Q-LINK  /  SELECTED PAD", qlinkArea);

    // ---- pad plate: a flat blue-grey plate set into the panel, with a darker column for the bank keys
    {
        auto plate = plateArea.toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawRoundedRectangle (plate.translated (0, 1.2f).expanded (0.6f), 4.0f, 1.2f);
        g.setFillType (tileFill (colours::deck, 11));
        g.fillRoundedRectangle (plate, 4.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.18f), 0, plate.getY(),
                                                 juce::Colours::transparentBlack, 0, plate.getY() + 12.0f, false));
        g.fillRect (plate.withHeight (12.0f));
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (plate, 4.0f, 1.0f);

        auto strip = stripArea.toFloat();
        g.setColour (colours::deck.darker (0.18f));
        g.fillRoundedRectangle (strip, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawRoundedRectangle (strip, 3.0f, 1.0f);

        auto head = plateArea.reduced (10, 8).removeFromTop (16).withTrimmedLeft (stripArea.getWidth() + 6);
        sectionHeader (g, "DRUMS", head);
    }
}

// ---------------------------------------------------------------- screens / state

void MainComponent::showHelp()
{
    if (ctx.screen == ScreenMode::Help)
        return showScreen (ctx.previousScreen);
    const auto topic = activeScreen().getHelpTopic();
    showScreen (ScreenMode::Help);
    helpOpenTopic (activeScreen(), topic);
}

void MainComponent::showScreen (ScreenMode m)
{
    if (m == ScreenMode::Help && ctx.screen != ScreenMode::Help)
        ctx.previousScreen = ctx.screen;
    ctx.screen = m;
    for (size_t i = 0; i < screens.size(); ++i)
        screens[i]->setVisible ((int) i == (int) m);
    modeTitle.setText (screens[(size_t) m]->getTitle(), juce::dontSendNotification);
    int tab = -1;
    for (int i = 0; i < 6; ++i)
        if (kSoftKeyScreens[i] == m)
            tab = i;
    softTabs.setActive (tab);
    if (isShowing())
        grabKeyboardFocus();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    loadQLinksFromPad();
    trackMute.setLit (ctx.padView == PadView::TrackMute);
    padMute.setLit (ctx.padView == PadView::PadMute);
    pads.repaint();
}

void MainComponent::flash (const juce::String& message)
{
    if (entry != Entry::None)
        return; // keypad entry owns the message line until ENTER
    messageLabel.setText (message, juce::dontSendNotification);
    messageUntil = juce::Time::getMillisecondCounter() + 2500;
}

void MainComponent::setBank (int b)
{
    engine.setCurrentBank (b);
    ctx.setSelectedPad (b * kPadsPerBank + ctx.selectedPad % kPadsPerBank);
    ctx.changed();
    flash ("PAD BANK " + juce::String::charToString ((juce::juce_wchar) ('A' + b)));
}

void MainComponent::timerCallback()
{
    const auto s = engine.getSettings();
    const auto state = engine.getTransportState();
    const auto rec = engine.getRecordMode();

    fullLevel.setLit (s.fullLevel);
    sixteenLevels.setLit (s.sixteenLevels);
    tapBtn.setLit (s.noteRepeat);
    eraseBtn.setLit (s.eraseHeld);
    afterBtn.setLit (s.noteVariationOn);
    keypad[9]->setLit (shiftActive());
    bankLeds.setBank (engine.getCurrentBank());
    if (tapPressedAt != 0)
    {
        // a pad hit while TAP is held means it's being used as NOTE REPEAT, not tapped
        static uint32_t lastHits = 0;
        uint32_t hits = 0;
        for (int i = 0; i < kNumPads; ++i)
            hits += engine.getPadHitCount (i);
        if (lastHits != 0 && hits != lastHits)
            padHitWhileTapHeld = true;
        lastHits = hits;
    }

    playBtn.setLit (state != Transport::State::Stopped);
    playStartBtn.setLit (state != Transport::State::Stopped && engine.isPlayingSong());
    recBtn.setLit (rec == RecordMode::Record);
    overdubBtn.setLit (rec == RecordMode::Overdub);
    const bool armedOnly = rec != RecordMode::None && state != Transport::State::Playing;
    recBtn.setBlink (armedOnly);
    overdubBtn.setBlink (armedOnly);
    if (armedOnly)
    {
        recBtn.repaint();
        overdubBtn.repaint();
    }

    // position: BAR.BEAT.TICK
    int bpb = 4;
    double bpm = 90.0;
    juce::String projName, seqName;
    const int seqIdx = engine.getCurrentSequence();
    engine.withProject ([&] (Project& p) {
        const auto& seq = p.sequences[(size_t) seqIdx];
        bpb = seq.beatsPerBar;
        bpm = seq.bpm;
        projName = p.name;
        seqName = seq.name;
    });
    juce::String pos;
    if (state == Transport::State::CountIn)
        pos = "COUNT " + juce::String ((int) std::ceil (engine.getCountInTicks() / kPPQN));
    else
    {
        const auto t = (int64_t) engine.getPositionTicks();
        const int bar = (int) (t / (bpb * kPPQN)) + 1, beat = (int) ((t / kPPQN) % bpb) + 1, tick = (int) (t % kPPQN);
        pos = juce::String (bar).paddedLeft ('0', 3) + "." + juce::String (beat).paddedLeft ('0', 2) + "."
              + juce::String (tick).paddedLeft ('0', 3);
    }
    const bool recording = rec != RecordMode::None && state != Transport::State::Stopped;
    positionLabel.setText ((recording ? "REC " : engine.isPlayingSong() ? "SONG " : "") + pos, juce::dontSendNotification);

    const int queued = engine.getQueuedSequence();
    if (++statusTick % 6 == 0)
        statusLabel.setText ((queued >= 0 ? "NEXT SEQ " + juce::String (queued + 1) + "   " : juce::String())
                                 + juce::String (bpm, 1) + " BPM   TRK " + juce::String (s.currentTrack + 1) + "   VOICES "
                                 + juce::String (engine.getActiveVoices()) + "   CPU " + juce::String ((int) (engine.getCpuLoad() * 100)) + "%",
                             juce::dontSendNotification);
    projectTitle.setText (projName + (ctx.projectFolder.exists() ? juce::String() : juce::String (" *")) + "  |  SEQ "
                              + juce::String (seqIdx + 1).paddedLeft ('0', 2) + " " + seqName,
                          juce::dontSendNotification);

    if (messageUntil != 0 && juce::Time::getMillisecondCounter() > messageUntil)
    {
        messageLabel.setText ({}, juce::dontSendNotification);
        messageUntil = 0;
    }

    auto& active = activeScreen();
    if (active.isVisible())
        active.refresh();

    Screen::LiveOverlay live;
    if (active.isVisible())
    {
        active.getLiveOverlay (engine.getPositionTicks(), state == Transport::State::Playing, live);
        // screen coordinates -> overlay coordinates (the screen may be scaled)
        if (! live.playhead.isEmpty())
            live.playhead = playhead.getLocalArea (&active, live.playhead);
        for (auto& m : live.meters)
            m.area = playhead.getLocalArea (&active, m.area);
    }
    playhead.update (std::move (live));

    juce::Rectangle<int> cursor;
    if (auto* t = dataTarget.getComponent(); t != nullptr && t->isShowing() && active.isParentOf (t))
        cursor = playhead.getLocalArea (t, t->getLocalBounds());
    playhead.setCursor (cursor);
}

// ---------------------------------------------------------------- transport helpers

void MainComponent::recordPressed (RecordMode mode)
{
    const auto current = engine.getRecordMode();
    if (current == mode)
    {
        engine.setRecord (RecordMode::None); // punch out / disarm
        return;
    }
    engine.pushUndo (mode == RecordMode::Record ? "record" : "overdub");
    engine.setRecord (mode);
}

void MainComponent::tapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! taps.empty() && now - taps.back() > 2000.0)
        taps.clear();
    taps.push_back (now);
    if (taps.size() > 5)
        taps.erase (taps.begin());
    if (taps.size() >= 3)
    {
        const double avg = (taps.back() - taps.front()) / (double) (taps.size() - 1);
        const double bpm = juce::jlimit (30.0, 300.0, std::round (60000.0 / avg * 10.0) / 10.0);
        const int seqIdx = engine.getCurrentSequence();
        engine.withProject ([&] (Project& p) { p.sequences[(size_t) seqIdx].bpm = bpm; });
        flash ("Tempo " + juce::String (bpm, 1));
        ctx.changed();
    }
}

// ---------------------------------------------------------------- Q-Links

void MainComponent::loadQLinksFromPad()
{
    const juce::ScopedValueSetter<bool> svs (loadingQ, true);
    const int prog = ctx.currentProgram();
    const int pad = ctx.selectedPad;
    float tune = 0, cutoff = 20000, reso = 0, decay = 300;
    engine.withProject ([&] (Project& p) {
        const auto& pd = p.programs[(size_t) prog].pads[(size_t) pad];
        tune = pd.tuneSemis;
        cutoff = pd.filter == FilterType::Off ? 20000.0f : pd.cutoffHz;
        reso = pd.resonance * 100.0f;
        decay = pd.decayMs;
    });
    qlinks[0].setValue (tune, juce::dontSendNotification);
    qlinks[1].setValue (cutoff, juce::dontSendNotification);
    qlinks[2].setValue (reso, juce::dontSendNotification);
    qlinks[3].setValue (decay, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
        qlinkLabels[(size_t) i].setText (juce::String (kQLinkNames[i]) + "  " + AppContext::padName (pad), juce::dontSendNotification);
}

void MainComponent::qlinkChanged (int knob)
{
    if (loadingQ)
        return;
    const int prog = ctx.currentProgram();
    const int pad = ctx.selectedPad;
    const double v = qlinks[(size_t) knob].getValue();
    engine.withProject ([&] (Project& p) {
        auto& pd = p.programs[(size_t) prog].pads[(size_t) pad];
        switch (knob)
        {
            case 0: pd.tuneSemis = (float) v; break;
            case 1:
                pd.cutoffHz = (float) v;
                if (pd.filter == FilterType::Off && v < 19999.0)
                    pd.filter = FilterType::LowPass;
                break;
            case 2: pd.resonance = (float) v / 100.0f; break;
            case 3:
                pd.decayMs = (float) v;
                pd.sustain = 0.0f; // decay is only audible without sustain
                break;
            default: break;
        }
    });
    if (ctx.screen == ScreenMode::Program)
        screens[(size_t) ScreenMode::Program]->contextChanged();
}


// ---------------------------------------------------------------- DATA wheel, CURSOR, keypad

bool MainComponent::shiftActive() const
{
    return shiftLatched || juce::ModifierKeys::currentModifiers.isShiftDown();
}

void MainComponent::noteTouched (juce::Component* c)
{
    for (; c != nullptr && c != this; c = c->getParentComponent())
    {
        if (c == &dataWheel || c == &cursorPad)
            return;
        for (auto& k : keypad)
            if (c == k.get())
                return;
        if (dynamic_cast<juce::Slider*> (c) != nullptr || dynamic_cast<juce::ComboBox*> (c) != nullptr
            || dynamic_cast<juce::ToggleButton*> (c) != nullptr)
        {
            if (c != dataTarget.getComponent())
                cancelEntry();
            dataTarget = c;
            return;
        }
    }
}

void MainComponent::moveCursor (int dx, int dy)
{
    // gather the editable fields of the current screen
    std::vector<juce::Component*> fields;
    std::function<void (juce::Component&)> collect = [&] (juce::Component& parent) {
        for (auto* c : parent.getChildren())
        {
            if (! c->isVisible())
                continue;
            if (c->isEnabled()
                && (dynamic_cast<juce::Slider*> (c) != nullptr || dynamic_cast<juce::ComboBox*> (c) != nullptr
                    || dynamic_cast<juce::ToggleButton*> (c) != nullptr))
                fields.push_back (c);
            else
                collect (*c);
        }
    };
    collect (activeScreen());
    if (fields.empty())
        return;
    cancelEntry();
    auto centre = [this] (juce::Component* c) { return getLocalArea (c, c->getLocalBounds()).getCentre().toFloat(); };

    auto* current = dataTarget.getComponent();
    if (current == nullptr || std::find (fields.begin(), fields.end(), current) == fields.end())
    {
        // no field yet: start at the top-left one
        dataTarget = *std::min_element (fields.begin(), fields.end(), [&] (auto* a, auto* b) {
            const auto pa = centre (a), pb = centre (b);
            return pa.y + pa.x * 0.25f < pb.y + pb.x * 0.25f;
        });
        return;
    }
    const auto from = centre (current);
    juce::Component* best = nullptr;
    float bestScore = 1.0e9f;
    for (auto* f : fields)
    {
        if (f == current)
            continue;
        const auto d = centre (f) - from;
        const float along = d.x * (float) dx + d.y * (float) dy;
        if (along <= 4.0f)
            continue;
        const float across = std::abs (d.x * (float) dy) + std::abs (d.y * (float) dx);
        const float score = along + across * 2.5f;
        if (score < bestScore)
        {
            bestScore = score;
            best = f;
        }
    }
    if (best != nullptr)
        dataTarget = best;
}

void MainComponent::dataStep (int steps)
{
    auto* target = dataTarget.getComponent();
    if (target == nullptr || ! target->isShowing())
    {
        flash ("Move the CURSOR to a field (or click one), then turn DATA");
        return;
    }
    cancelEntry();

    if (auto* s = dynamic_cast<juce::Slider*> (target))
    {
        const auto range = s->getRange();
        const double interval = s->getInterval();
        double v = s->getValue();
        if (interval > 0.0 && juce::approximatelyEqual (s->getSkewFactor(), 1.0))
        {
            // about 300 detents end to end, rounded to a tidy multiple of the slider's step
            const double raw = juce::jmax (1.0, std::ceil (range.getLength() / interval / 300.0));
            double mult = 1.0;
            for (double nice : { 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0 })
                if ((mult = nice) >= raw)
                    break;
            v = std::round (v / (interval * mult)) * interval * mult + steps * interval * mult;
        }
        else
        {
            v = s->proportionOfLengthToValue (juce::jlimit (0.0, 1.0, s->valueToProportionOfLength (v) + steps / 200.0));
        }
        s->setValue (range.clipValue (v), juce::sendNotificationSync);
        flash (s->getTextFromValue (s->getValue()));
    }
    else if (auto* cb = dynamic_cast<juce::ComboBox*> (target))
    {
        const int n = cb->getNumItems();
        int idx = cb->getSelectedItemIndex();
        const int dir = steps > 0 ? 1 : -1;
        for (int remaining = std::abs (steps); remaining > 0; --remaining)
        {
            int next = idx + dir;
            while (next >= 0 && next < n && ! cb->isItemEnabled (cb->getItemId (next)))
                next += dir;
            if (next < 0 || next >= n)
                break;
            idx = next;
        }
        if (idx != cb->getSelectedItemIndex() && idx >= 0)
        {
            cb->setSelectedItemIndex (idx, juce::sendNotificationSync);
            flash (cb->getItemText (idx));
        }
    }
    else if (auto* tb = dynamic_cast<juce::ToggleButton*> (target))
    {
        if ((steps > 0) != tb->getToggleState())
            tb->triggerClick();
        flash (tb->getButtonText() + (steps > 0 ? ": ON" : ": OFF"));
    }
}

void MainComponent::cancelEntry()
{
    if (entry == Entry::None)
        return;
    entry = Entry::None;
    entryDigits.clear();
    messageLabel.setText ({}, juce::dontSendNotification);
}

void MainComponent::keypadPressed (int key)
{
    if (key == 10) // SHIFT latches for the next key
    {
        shiftLatched = ! shiftLatched;
        return;
    }
    if (key == 11) // ENTER
        return commitEntry();

    auto showEntry = [this] {
        messageUntil = 0;
        messageLabel.setText ((entry == Entry::GoTo ? "GO TO BAR: " : "ENTER VALUE: ") + entryDigits + "_",
                              juce::dontSendNotification);
    };

    // SHIFT + key (or a key with no field to type into) opens the mode printed under it
    auto* target = dataTarget.getComponent();
    const bool numericTarget = target != nullptr && target->isShowing()
                               && (dynamic_cast<juce::Slider*> (target) != nullptr || dynamic_cast<juce::ComboBox*> (target) != nullptr);
    if (shiftActive() || (entry != Entry::GoTo && ! numericTarget))
    {
        shiftLatched = false;
        cancelEntry();
        switch (key)
        {
            case 0: showScreen (ScreenMode::Edit); break;
            case 1: showScreen (ScreenMode::Song); break;
            case 2: showScreen (ScreenMode::StepEdit); break;
            case 3: showDiskMenu(); break;
            case 4: showScreen (ScreenMode::Sampler); break;
            case 5: showScreen (ScreenMode::SampleEdit); break;
            case 6: showScreen (ScreenMode::Program); break;
            case 7: showScreen (ScreenMode::Mixer); break;
            case 8: showScreen (ScreenMode::Other); break;
            case 9: showAudioSettings(); break;
            default: break;
        }
        return;
    }

    if (entry == Entry::None)
        entry = Entry::Value;
    if (entryDigits.length() < 6)
        entryDigits << key;
    showEntry();
}

void MainComponent::commitEntry()
{
    if (entry == Entry::None || entryDigits.isEmpty())
        return cancelEntry();
    const int n = entryDigits.getIntValue();
    const auto mode = entry;
    entry = Entry::None;
    entryDigits.clear();

    if (mode == Entry::GoTo)
    {
        int64_t len = 0, barTicks = 4 * kPPQN;
        engine.withProject ([&] (Project& p) {
            const auto& seq = p.sequences[(size_t) engine.getCurrentSequence()];
            len = seq.getLengthTicks();
            barTicks = (int64_t) seq.beatsPerBar * kPPQN;
        });
        const int64_t target = juce::jlimit<int64_t> (0, juce::jmax<int64_t> (0, len - barTicks), (int64_t) (juce::jmax (1, n) - 1) * barTicks);
        engine.locate (target);
        flash ("GO TO BAR " + juce::String (target / barTicks + 1));
        return;
    }

    auto* target = dataTarget.getComponent();
    if (auto* s = dynamic_cast<juce::Slider*> (target))
    {
        s->setValue (s->getRange().clipValue (n), juce::sendNotificationSync);
        flash (s->getTextFromValue (s->getValue()));
    }
    else if (auto* cb = dynamic_cast<juce::ComboBox*> (target))
    {
        // pick the item numbered n ("05 Verse", "16"...), else the n-th item
        int pick = -1;
        for (int i = 0; i < cb->getNumItems() && pick < 0; ++i)
        {
            const auto lead = cb->getItemText (i).trimStart().initialSectionContainingOnly ("0123456789");
            if (lead.isNotEmpty() && lead.getIntValue() == n)
                pick = i;
        }
        if (pick < 0 && n >= 1 && n <= cb->getNumItems())
            pick = n - 1;
        if (pick >= 0)
        {
            cb->setSelectedItemIndex (pick, juce::sendNotificationSync);
            flash (cb->getItemText (pick));
        }
        else
            flash ("No item " + juce::String (n));
    }
    else
        messageLabel.setText ({}, juce::dontSendNotification);
}

void MainComponent::locateStep (int dir)
{
    cancelEntry();
    if (ctx.screen == ScreenMode::StepEdit)
        return stepEditStep (activeScreen(), dir);
    const auto s = engine.getSettings();
    const auto& div = timing::divisions()[(size_t) s.timingCorrect];
    const int64_t step = div.ticks > 1 ? div.ticks : kPPQN / 4;
    int64_t len = 0;
    engine.withProject ([&] (Project& p) { len = p.sequences[(size_t) engine.getCurrentSequence()].getLengthTicks(); });
    const auto pos = (int64_t) std::llround (engine.getPositionTicks());
    const int64_t target = dir > 0 ? (pos / step + 1) * step : (pos > 0 ? ((pos - 1) / step) * step : 0);
    engine.locate (juce::jlimit<int64_t> (0, juce::jmax<int64_t> (0, len - 1), target));
}

void MainComponent::locateBar (int dir)
{
    cancelEntry();
    int64_t len = 0, barTicks = 4 * kPPQN;
    engine.withProject ([&] (Project& p) {
        const auto& seq = p.sequences[(size_t) engine.getCurrentSequence()];
        len = seq.getLengthTicks();
        barTicks = (int64_t) seq.beatsPerBar * kPPQN;
    });
    const auto pos = (int64_t) std::llround (engine.getPositionTicks());
    int64_t target = dir > 0 ? (pos / barTicks + 1) * barTicks : (pos > 0 ? ((pos - 1) / barTicks) * barTicks : 0);
    if (target >= len)
        target = juce::jmax<int64_t> (0, len - barTicks);
    engine.locate (target);
}

void MainComponent::openWindow()
{
    // OPEN WINDOW: the full editor for the field under the cursor
    auto* target = dataTarget.getComponent();
    if (auto* cb = dynamic_cast<juce::ComboBox*> (target); cb != nullptr && cb->isShowing())
        return cb->showPopup();
    if (auto* s = dynamic_cast<juce::Slider*> (target); s != nullptr && s->isShowing())
        return s->showTextBox();
    flash ("OPEN WINDOW: move the CURSOR to a field first");
}

void MainComponent::showDiskMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lcdLnf);
    m.addSectionHeader ("DISK");
    m.addItem (1, "New project");
    m.addItem (2, "Load project...");
    m.addItem (3, "Save project", true);
    m.addItem (4, "Save project as...");
    m.addSeparator();
    m.addItem (5, "Audio / MIDI setup...");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&floppy), [this] (int r) {
        switch (r)
        {
            case 1: newProject(); break;
            case 2: openProject(); break;
            case 3: saveProject (false); break;
            case 4: saveProject (true); break;
            case 5: showAudioSettings(); break;
            default: break;
        }
    });
}

// ---------------------------------------------------------------- keyboard

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    // don't steal typing from text fields
    if (dynamic_cast<juce::TextEditor*> (getCurrentlyFocusedComponent()) != nullptr)
        return false;
    // the screen's own hotkeys first (e.g. chop editing in TRIM)
    if (activeScreen().isVisible() && activeScreen().handleKey (k))
        return true;
    if (k.getTextCharacter() == '?' || (mods.isCommandDown() && k.getKeyCode() == '/'))
    {
        showHelp();
        return true;
    }
    if (mods.isCommandDown())
    {
        const auto c = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
        if (k.getKeyCode() == 'Z' || c == 'z') { undoBtn.triggerClick(); return true; }
        if (k.getKeyCode() == 'S' || c == 's') { saveProject (mods.isShiftDown()); return true; }
        if (k.getKeyCode() == 'O' || c == 'o') { openProject(); return true; }
        if (k.getKeyCode() == 'N' || c == 'n') { newProject(); return true; }
        return false;
    }

    // don't steal typing from text fields
    if (dynamic_cast<juce::TextEditor*> (getCurrentlyFocusedComponent()) != nullptr)
        return false;

    const int code = k.getKeyCode();
    if (k == juce::KeyPress::spaceKey)
    {
        if (engine.getTransportState() == Transport::State::Stopped)
            engine.play (false);
        else
            engine.stop();
        return true;
    }
    if (k == juce::KeyPress::returnKey)
    {
        if (entry != Entry::None)
            commitEntry();
        else
            engine.play (true);
        return true;
    }
    if (k == juce::KeyPress::escapeKey) { cancelEntry(); return true; }
    if (code == juce::KeyPress::upKey) { moveCursor (0, -1); return true; }
    if (code == juce::KeyPress::downKey) { moveCursor (0, 1); return true; }
    if (code == juce::KeyPress::leftKey) { moveCursor (-1, 0); return true; }
    if (code == juce::KeyPress::rightKey) { moveCursor (1, 0); return true; }
    const int fkeys[] = { juce::KeyPress::F1Key, juce::KeyPress::F2Key, juce::KeyPress::F3Key,
                          juce::KeyPress::F4Key, juce::KeyPress::F5Key, juce::KeyPress::F6Key };
    for (int i = 0; i < 6; ++i)
        if (code == fkeys[i]) { softKeys[(size_t) i]->triggerClick(); return true; }
    // the numeric keypad of a full-size keyboard works like the panel keypad
    for (int d = 0; d <= 9; ++d)
        if (code == juce::KeyPress::numberPad0 + d) { keypadPressed (d); return true; }

    const auto ch = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (ch == '-' || ch == '_') { dataWheel.nudge (mods.isShiftDown() ? -10 : -1); return true; }
    if (ch == '=' || ch == '+') { dataWheel.nudge (mods.isShiftDown() ? 10 : 1); return true; }
    if (ch == '[') { setBank (juce::jmax (0, engine.getCurrentBank() - 1)); return true; }
    if (ch == ']') { setBank (juce::jmin (kNumBanks - 1, engine.getCurrentBank() + 1)); return true; }
    if (ch == '.') { recordPressed (RecordMode::Overdub); return true; }
    if (ch == ',') { recordPressed (RecordMode::Record); return true; }
    if (ch == 'n')
    {
        engine.updateSettings ([] (PerformanceSettings& s) { s.noteRepeat = ! s.noteRepeat; });
        flash (engine.getSettings().noteRepeat ? "NOTE REPEAT latched" : "NOTE REPEAT off");
        return true;
    }
    if (ch == 'l') { fullLevel.triggerClick(); return true; }
    if (ch == 'g') { gotoBtn.triggerClick(); return true; }

    for (auto key : kPadKeys)
        if (ch == (juce::juce_wchar) key)
            return true; // handled in keyStateChanged
    return false;
}

bool MainComponent::keyStateChanged (bool)
{
    if (juce::ModifierKeys::currentModifiers.isCommandDown())
        return false;
    if (dynamic_cast<juce::TextEditor*> (getCurrentlyFocusedComponent()) != nullptr)
        return false;

    bool handled = false;
    for (int i = 0; i < kPadsPerBank; ++i)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown (kPadKeys[i]);
        if (down == keyDown[(size_t) i])
            continue;
        keyDown[(size_t) i] = down;
        handled = true;
        if (down)
            pads.press (i, juce::ModifierKeys::currentModifiers.isShiftDown() ? 127 : 100);
        else
            pads.release (i);
    }
    return handled;
}

// ---------------------------------------------------------------- files

void MainComponent::newProject()
{
    auto p = std::make_unique<Project>();
    drumsynth::loadFactoryProject (*p);
    p->name = "Untitled";
    engine.replaceProject (std::move (p));
    ctx.projectFolder = juce::File();
    ctx.setSelectedPad (0);
    engine.setCurrentBank (0);
    engine.updateSettings ([] (PerformanceSettings& s) { s.currentTrack = 0; });
    ctx.changed();
    flash ("New project");
}

void MainComponent::openProject()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a .padlab project folder",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc) {
                              auto dir = fc.getResult();
                              if (! dir.isDirectory())
                                  return;
                              auto p = std::make_unique<Project>();
                              juce::String err;
                              if (! projectio::load (dir, *p, err))
                              {
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open project", err);
                                  return;
                              }
                              engine.replaceProject (std::move (p));
                              ctx.projectFolder = dir;
                              ctx.setSelectedPad (0);
                              engine.setCurrentBank (0);
                              ctx.changed();
                              flash ("Opened " + dir.getFileName());
                          });
}

void MainComponent::saveProject (bool saveAs)
{
    auto doSave = [this] (const juce::File& dir) {
        juce::String err;
        // capture under the lock (cheap), write files without holding it
        const auto job = engine.withProject ([&] (Project& p) {
            p.name = dir.getFileNameWithoutExtension();
            return projectio::prepareSave (p);
        });
        if (! projectio::writeSave (job, dir, err))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save project", err);
            return;
        }
        ctx.projectFolder = dir;
        flash ("Saved " + dir.getFileName());
    };

    if (! saveAs && ctx.projectFolder.isDirectory())
        return doSave (ctx.projectFolder);

    chooser = std::make_unique<juce::FileChooser> ("Save project as",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                                       .getChildFile ("My Beat.padlab"));
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [doSave] (const juce::FileChooser& fc) {
                              auto f = fc.getResult();
                              if (f == juce::File())
                                  return;
                              doSave (f.withFileExtension ("padlab"));
                          });
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager(), 0, 2, 2, 2, true, true, true, false);
    selector->setSize (520, 460);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (selector);
    o.dialogTitle = "Audio / MIDI settings";
    o.dialogBackgroundColour = theme::colours::chassis;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

} // namespace padlab
