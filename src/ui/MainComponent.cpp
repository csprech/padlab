#include "ui/MainComponent.h"
#include "core/DrumSynth.h"
#include "core/ProjectIO.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace padlab
{
namespace
{
// computer keyboard -> pads (bottom row = pads 1-4, like the hardware)
constexpr char kPadKeys[kPadsPerBank] = { 'z', 'x', 'c', 'v', 'a', 's', 'd', 'f', 'q', 'w', 'e', 'r', '1', '2', '3', '4' };

const char* kQLinkNames[4] = { "TUNE", "CUTOFF", "RESO", "DECAY" };
} // namespace

MainComponent::MainComponent (Engine& e) : engine (e), ctx (e), pads (ctx)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    ctx.addChangeListener (this);
    ctx.showScreen = [this] (ScreenMode m) { showScreen (m); };
    ctx.flashMessage = [this] (const juce::String& m) { flash (m); };

    screens[0] = createMainScreen (ctx);
    screens[1] = createProgramScreen (ctx);
    screens[2] = createSampleScreen (ctx);
    screens[3] = createSamplerScreen (ctx);
    screens[4] = createMixerScreen (ctx);
    for (auto& s : screens)
    {
        s->setLookAndFeel (&lcdLnf);
        addChildComponent (*s);
    }
    for (auto* b : { &stopBtn, &undoBtn, &tapBtn })
        b->setHasLed (false);

    for (auto* l : { &modeTitle, &projectTitle, &positionLabel, &statusLabel, &messageLabel })
    {
        addAndMakeVisible (l);
        l->setInterceptsMouseClicks (false, false);
    }
    modeTitle.setFont (theme::screenFont (14.0f));
    modeTitle.setColour (juce::Label::textColourId, theme::lcd::backlight);
    projectTitle.setFont (theme::screenFont (14.0f));
    projectTitle.setColour (juce::Label::textColourId, theme::lcd::backlight);
    projectTitle.setJustificationType (juce::Justification::centredRight);
    positionLabel.setFont (theme::screenFont (21.0f));
    positionLabel.setColour (juce::Label::textColourId, theme::lcd::ink);
    statusLabel.setFont (theme::screenFont (13.0f));
    statusLabel.setColour (juce::Label::textColourId, theme::lcd::textDim);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    messageLabel.setFont (theme::screenFont (14.0f));
    messageLabel.setColour (juce::Label::textColourId, theme::lcd::ink);
    messageLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (pads);

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
        l.setFont (theme::labelFont (12.0f));
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
    engine.onQLinkMidi = [this] (int knob, int value) {
        auto& q = qlinks[(size_t) knob];
        q.setValue (q.proportionOfLengthToValue (value / 127.0));
    };

    // ---- pad modes
    fullLevel.onClick = [this] {
        engine.updateSettings ([] (PerformanceSettings& s) { s.fullLevel = ! s.fullLevel; });
    };
    sixteenLevels.setTooltip ("Spread the selected pad over all 16 pads by velocity (shift-click: by tune)");
    sixteenLevels.onClick = [this] {
        const bool tuneMode = juce::ModifierKeys::currentModifiers.isShiftDown();
        const int src = ctx.selectedPad;
        engine.updateSettings ([&] (PerformanceSettings& s) {
            s.sixteenLevels = ! s.sixteenLevels;
            s.sixteenLevelsSource = src;
            s.sixteenLevelsParam = tuneMode ? 1 : 0;
        });
        const auto s = engine.getSettings();
        if (s.sixteenLevels)
            flash ("16 LEVELS: " + AppContext::padName (src) + (s.sixteenLevelsParam == 1 ? " by tune" : " by velocity"));
    };
    noteRepeat.setTooltip ("Hold pads to repeat at the Note Repeat rate. Drag up/down on a held pad for pressure.");
    noteRepeat.onClick = [this] {
        engine.updateSettings ([] (PerformanceSettings& s) { s.noteRepeat = ! s.noteRepeat; });
    };
    erase.setTooltip ("Hold ERASE and hold pads while playing to delete their notes");
    erase.onStateChange = [this] {
        const bool down = erase.isDown();
        engine.updateSettings ([down] (PerformanceSettings& s) { s.eraseHeld = down; });
    };
    trackMute.onClick = [this] {
        ctx.padView = ctx.padView == PadView::TrackMute ? PadView::Play : PadView::TrackMute;
        ctx.changed();
    };
    padMute.onClick = [this] {
        ctx.padView = ctx.padView == PadView::PadMute ? PadView::Play : PadView::PadMute;
        ctx.changed();
    };
    for (auto* b : { &fullLevel, &sixteenLevels, &noteRepeat, &erase, &trackMute, &padMute })
        addAndMakeVisible (b);

    for (int i = 0; i < kNumBanks; ++i)
    {
        banks[(size_t) i] = std::make_unique<theme::HardButton> (juce::String::charToString ((juce::juce_wchar) ('A' + i)));
        banks[(size_t) i]->onClick = [this, i] { setBank (i); };
        addAndMakeVisible (*banks[(size_t) i]);
    }

    // ---- screen modes
    mainBtn.onClick = [this] { showScreen (ScreenMode::Main); };
    progBtn.onClick = [this] { showScreen (ScreenMode::Program); };
    sampleBtn.onClick = [this] { showScreen (ScreenMode::SampleEdit); };
    samplerBtn.onClick = [this] { showScreen (ScreenMode::Sampler); };
    mixerBtn.onClick = [this] { showScreen (ScreenMode::Mixer); };
    for (auto* b : { &mainBtn, &progBtn, &sampleBtn, &samplerBtn, &mixerBtn })
        addAndMakeVisible (b);

    // ---- transport
    recBtn.onClick = [this] { recordPressed (RecordMode::Record); };
    overdubBtn.onClick = [this] { recordPressed (RecordMode::Overdub); };
    stopBtn.onClick = [this] {
        const auto now = juce::Time::getMillisecondCounter();
        if (engine.getTransportState() == Transport::State::Stopped && now - lastStopMs < 400)
            engine.panic(); // double-stop: silence everything
        lastStopMs = now;
        engine.stop();
    };
    playBtn.onClick = [this] { engine.play (false); };
    playStartBtn.onClick = [this] { engine.play (true); };
    undoBtn.onClick = [this] {
        const auto what = engine.getUndoDescription();
        flash (engine.undo() ? "Undo " + what : "Nothing to undo");
        ctx.changed();
    };
    tapBtn.onClick = [this] { tapTempo(); };
    for (auto* b : { &recBtn, &overdubBtn, &stopBtn, &playBtn, &playStartBtn, &undoBtn, &tapBtn })
        addAndMakeVisible (b);

    newBtn.onClick = [this] { newProject(); };
    openBtn.onClick = [this] { openProject(); };
    saveBtn.onClick = [this] { saveProject (false); };
    settingsBtn.onClick = [this] { showAudioSettings(); };
    for (auto* b : { &newBtn, &openBtn, &saveBtn, &settingsBtn })
        addAndMakeVisible (b);

    addAndMakeVisible (glass); // LCD glass sits on top of the screen contents

    setSize (1440, 880);
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
    ctx.removeChangeListener (this);
    engine.onQLinkMidi = nullptr;
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

// ---------------------------------------------------------------- layout

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (26, 22);
    rightPanel = r.removeFromRight (juce::jlimit (420, 560, (int) (r.getWidth() * 0.345f)));
    r.removeFromRight (26);

    // ---- left: brand, LCD, mode keys, transport keys
    auto left = r;
    brandArea = left.removeFromTop (46);
    left.removeFromTop (6);
    transportArea = left.removeFromBottom (78);
    left.removeFromBottom (8);
    modeArea = left.removeFromBottom (74);
    left.removeFromBottom (10);
    screenArea = left;
    lcdArea = screenArea.reduced (18, 16);
    glass.setBounds (lcdArea);

    auto inner = lcdArea.reduced (6);
    auto titleBar = inner.removeFromTop (22);
    modeTitle.setBounds (titleBar.removeFromLeft (titleBar.getWidth() / 2).withTrimmedLeft (6));
    projectTitle.setBounds (titleBar.withTrimmedRight (6));
    statusArea = inner.removeFromBottom (30);
    auto st = statusArea.reduced (6, 2);
    positionLabel.setBounds (st.removeFromLeft (220));
    statusLabel.setBounds (st.removeFromRight (380));
    messageLabel.setBounds (st);
    for (auto& s : screens)
        s->setBounds (inner);

    {
        auto m = modeArea.withTrimmedTop (16);
        const int w = 96;
        for (auto* b : { &mainBtn, &progBtn, &sampleBtn, &samplerBtn, &mixerBtn })
        {
            b->setBounds (m.removeFromLeft (w));
            m.removeFromLeft (6);
        }
        fileArea = m.removeFromRight (340);
        auto files = fileArea.withTrimmedTop (4).withSizeKeepingCentre (340, 30);
        for (auto* b : { &newBtn, &openBtn, &saveBtn, &settingsBtn })
        {
            b->setBounds (files.removeFromLeft (b == &settingsBtn ? 100 : 74));
            files.removeFromLeft (6);
        }
    }
    {
        auto t = transportArea.withTrimmedTop (16);
        const int w = juce::jmin (104, (t.getWidth() - 6 * 8) / 7);
        for (auto* b : { &recBtn, &overdubBtn, &stopBtn, &playBtn, &playStartBtn })
        {
            b->setBounds (t.removeFromLeft (w));
            t.removeFromLeft (8);
        }
        tapBtn.setBounds (t.removeFromRight (w));
        t.removeFromRight (8);
        undoBtn.setBounds (t.removeFromRight (w));
    }

    // ---- right: brand, Q-links, pad modes, banks, pad deck
    auto rp = rightPanel;
    deckArea = rp.removeFromBottom (PadGrid::heightForWidth (rp.getWidth()));
    pads.setBounds (deckArea);
    rp.removeFromBottom (10);
    bankArea = rp.removeFromBottom (58);
    rp.removeFromBottom (4);
    padModeArea = rp.removeFromBottom (72);
    rp.removeFromBottom (4);
    qlinkArea = rp;

    auto qrow = qlinkArea.withTrimmedTop (18);
    const int qw = qrow.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto cell = qrow.removeFromLeft (qw).reduced (4, 0);
        qlinkLabels[(size_t) i].setBounds (cell.removeFromBottom (18));
        qlinks[(size_t) i].setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), cell.getHeight() + 16), cell.getHeight()));
    }
    auto modeRow = padModeArea.withTrimmedTop (16);
    const int mw = (modeRow.getWidth() - 5 * 6) / 6;
    for (auto* b : { &fullLevel, &sixteenLevels, &noteRepeat, &erase, &trackMute, &padMute })
    {
        b->setBounds (modeRow.removeFromLeft (mw));
        modeRow.removeFromLeft (6);
    }
    auto bankRow = bankArea.withTrimmedTop (16);
    const int bw = (bankRow.getWidth() - 7 * 6) / 8;
    for (auto& b : banks)
    {
        b->setBounds (bankRow.removeFromLeft (bw));
        bankRow.removeFromLeft (6);
    }
}

namespace
{
    // a printed section header with a rule either side, like silkscreened panel groupings
    void sectionHeader (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> row, bool light = false)
    {
        using namespace padlab::theme;
        auto r = row.removeFromTop (14);
        g.setFont (labelFont (11.5f));
        const int tw = (int) std::ceil (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text)) + 14;
        auto textR = r.withSizeKeepingCentre (tw, r.getHeight());
        g.setColour ((light ? colours::silkLight : colours::silk).withAlpha (0.55f));
        const float y = (float) r.getCentreY();
        g.drawLine ((float) r.getX(), y, (float) textR.getX(), y, 1.0f);
        g.drawLine ((float) textR.getRight(), y, (float) r.getRight(), y, 1.0f);
        drawSilk (g, text, textR, juce::Justification::centred, 11.5f, light);
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    using namespace theme;
    const auto bounds = getLocalBounds().toFloat();

    // ---- chassis: painted metal with speckle, lit from above
    g.setGradientFill (juce::ColourGradient (colours::chassis.brighter (0.06f), 0, 0, colours::chassisLow, 0, bounds.getBottom(), false));
    g.fillAll();
    g.setTiledImageFill (speckle (0), 0, 0, 1.0f);
    g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawHorizontalLine (1, 0.0f, bounds.getWidth());
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, bounds.getWidth());

    // screws
    const float sr = 6.0f;
    drawScrew (g, { 13.0f, 12.0f }, sr, 0.4f);
    drawScrew (g, { bounds.getRight() - 13.0f, 12.0f }, sr, 1.2f);
    drawScrew (g, { 13.0f, bounds.getBottom() - 12.0f }, sr, 2.1f);
    drawScrew (g, { bounds.getRight() - 13.0f, bounds.getBottom() - 12.0f }, sr, 0.9f);

    // ---- pad deck: a darker inset plate
    auto deck = deckArea.toFloat().expanded (8.0f, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (deck.translated (0, 1.0f), 10.0f, 1.2f);
    g.setColour (colours::deck);
    g.fillRoundedRectangle (deck, 10.0f);
    g.setTiledImageFill (speckle (0), 0, 0, 1.0f);
    g.fillRoundedRectangle (deck, 10.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.25f), 0, deck.getY(), juce::Colours::transparentBlack,
                                             0, deck.getY() + 12.0f, false));
    g.fillRoundedRectangle (deck.withHeight (14.0f), 10.0f);
    g.setColour (colours::seam);
    g.drawRoundedRectangle (deck, 10.0f, 1.0f);

    // ---- LCD bezel
    auto bez = screenArea.toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawRoundedRectangle (bez.expanded (1.5f).translated (0, 1.0f), 12.0f, 1.2f);
    g.setGradientFill (juce::ColourGradient (colours::bezel.brighter (0.12f), 0, bez.getY(), colours::bezel.darker (0.3f), 0,
                                             bez.getBottom(), false));
    g.fillRoundedRectangle (bez, 12.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (lcdArea.toFloat().expanded (2.0f), 4.0f, 2.0f);
    // backlight
    auto lcdR = lcdArea.toFloat();
    g.setGradientFill (juce::ColourGradient (lcd::backlight.brighter (0.06f), lcdR.getCentreX(), lcdR.getCentreY() - lcdR.getHeight() * 0.1f,
                                             lcd::backlightLo, lcdR.getX(), lcdR.getBottom(), true));
    g.fillRect (lcdR);
    // title bar (inverse video) and status rule
    auto inner = lcdArea.reduced (6);
    g.setColour (lcd::ink);
    g.fillRect (inner.removeFromTop (22));
    auto status = inner.removeFromBottom (30);
    g.fillRect (status.getX(), status.getY(), status.getWidth(), 2);
    // bezel print
    drawSilk (g, "BACKLIT DOT MATRIX LCD", screenArea.withTrimmedTop (screenArea.getHeight() - 15).withTrimmedRight (22),
              juce::Justification::centredRight, 9.5f, true);

    // ---- brand plate
    {
        auto b = brandArea.reduced (4, 0);
        auto name = b.removeFromLeft ((int) (b.getWidth() * 0.55f));
        const float h = 40.0f;
        // italic-ish wordmark via a shear transform
        juce::GlyphArrangement ga;
        ga.addLineOfText (labelFont (h), "PADLAB", (float) name.getX(), (float) name.getCentreY() + h * 0.32f);
        juce::Path wordmark;
        ga.createPath (wordmark);
        wordmark.applyTransform (juce::AffineTransform::shear (-0.18f, 0.0f).translated (name.getCentreY() * 0.18f, 0.0f));
        g.setColour (colours::silk);
        g.fillPath (wordmark);

        auto badge = juce::Rectangle<float> (wordmark.getBounds().getRight() + 10.0f, name.getCentreY() - 10.0f, 56.0f, 20.0f);
        g.setColour (colours::bezel);
        g.fillRoundedRectangle (badge, 3.0f);
        drawSilk (g, "PL-96", badge.toNearestInt(), juce::Justification::centred, 13.0f, true);

        drawSilk (g, "SAMPLING GROOVE\nWORKSTATION", b.reduced (0, 4), juce::Justification::centredRight, 12.0f);
    }

    // ---- silkscreen section headers
    sectionHeader (g, "Q-LINK  /  SELECTED PAD", qlinkArea);
    sectionHeader (g, "PAD PERFORMANCE", padModeArea);
    sectionHeader (g, "PAD BANK", bankArea);
    sectionHeader (g, "MODE", modeArea.withWidth (5 * 96 + 4 * 6));
    sectionHeader (g, "DISK", fileArea);
    sectionHeader (g, "TRANSPORT", transportArea);
}

// ---------------------------------------------------------------- screens / state

void MainComponent::showScreen (ScreenMode m)
{
    ctx.screen = m;
    for (size_t i = 0; i < screens.size(); ++i)
        screens[i]->setVisible ((int) i == (int) m);
    modeTitle.setText (screens[(size_t) m]->getTitle(), juce::dontSendNotification);
    mainBtn.setLit (m == ScreenMode::Main);
    progBtn.setLit (m == ScreenMode::Program);
    sampleBtn.setLit (m == ScreenMode::SampleEdit);
    samplerBtn.setLit (m == ScreenMode::Sampler);
    mixerBtn.setLit (m == ScreenMode::Mixer);
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
    messageLabel.setText (message, juce::dontSendNotification);
    messageUntil = juce::Time::getMillisecondCounter() + 2500;
}

void MainComponent::setBank (int b)
{
    engine.setCurrentBank (b);
    ctx.setSelectedPad (b * kPadsPerBank + ctx.selectedPad % kPadsPerBank);
    ctx.changed();
}

void MainComponent::timerCallback()
{
    const auto s = engine.getSettings();
    const auto state = engine.getTransportState();
    const auto rec = engine.getRecordMode();

    fullLevel.setLit (s.fullLevel);
    sixteenLevels.setLit (s.sixteenLevels);
    noteRepeat.setLit (s.noteRepeat);
    erase.setLit (s.eraseHeld);
    for (int i = 0; i < kNumBanks; ++i)
        banks[(size_t) i]->setLit (engine.getCurrentBank() == i);

    playBtn.setLit (state != Transport::State::Stopped);
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
    int seqBars = 2, bpb = 4;
    double bpm = 90.0;
    juce::String projName, seqName;
    const int seqIdx = engine.getCurrentSequence();
    engine.withProject ([&] (Project& p) {
        const auto& seq = p.sequences[(size_t) seqIdx];
        seqBars = seq.bars;
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
    positionLabel.setText (pos, juce::dontSendNotification);
    const bool recording = engine.getRecordMode() != RecordMode::None && state != Transport::State::Stopped;
    positionLabel.setText ((recording ? "REC " : "") + positionLabel.getText(), juce::dontSendNotification);

    const int queued = engine.getQueuedSequence();
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

    auto& active = screens[(size_t) ctx.screen];
    if (active->isVisible())
        active->refresh();
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

// ---------------------------------------------------------------- keyboard

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
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

    if (k == juce::KeyPress::spaceKey)
    {
        if (engine.getTransportState() == Transport::State::Stopped)
            engine.play (false);
        else
            engine.stop();
        return true;
    }
    if (k == juce::KeyPress::returnKey) { engine.play (true); return true; }
    if (k == juce::KeyPress::F1Key) { showScreen (ScreenMode::Main); return true; }
    if (k == juce::KeyPress::F2Key) { showScreen (ScreenMode::Program); return true; }
    if (k == juce::KeyPress::F3Key) { showScreen (ScreenMode::SampleEdit); return true; }
    if (k == juce::KeyPress::F4Key) { showScreen (ScreenMode::Sampler); return true; }
    if (k == juce::KeyPress::F5Key) { showScreen (ScreenMode::Mixer); return true; }

    const auto ch = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (ch == '[') { setBank (juce::jmax (0, engine.getCurrentBank() - 1)); return true; }
    if (ch == ']') { setBank (juce::jmin (kNumBanks - 1, engine.getCurrentBank() + 1)); return true; }
    if (ch == '.') { recordPressed (RecordMode::Overdub); return true; }
    if (ch == ',') { recordPressed (RecordMode::Record); return true; }
    if (ch == 'n') { noteRepeat.triggerClick(); return true; }
    if (ch == 'l') { fullLevel.triggerClick(); return true; }

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
    auto* selector = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager(), 0, 2, 2, 2, true, false, true, false);
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
