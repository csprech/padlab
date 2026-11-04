// HELP: how-to topics, shown on the LCD. Opened with the HELP key or "?", on the topic for the
// screen you came from. Up/Down pick a topic, PageUp/PageDown (or the wheel) scroll, Esc goes back.

#include "ui/Screens.h"

namespace padlab
{
namespace
{
struct Topic
{
    const char* title;
    const char* text;
};

// "# " = heading, "- " = bullet, "1. " = step; blank line = paragraph break.
const Topic kTopics[] = {
    { "Quick start", R"(# Quick start
Padlab works like a classic pad sampler: sounds live on pads, pads belong to a program, and you record what you play into sequences.

1. Press PLAY (or Space) to hear the factory beat. STOP (or Space) stops; press STOP twice to silence everything.
2. Hit the pads with the mouse (higher on the pad = harder) or the keyboard: Z X C V / A S D F / Q W E R / 1 2 3 4 are the 16 pads, bottom row first.
3. Press OVERDUB (or .) and PLAY to record on top of the beat, with timing correct and swing from the MAIN screen.
4. Load your own sounds onto pads (see "Loading sounds onto pads") and chop breaks (see "Chopping a sample").
5. Save with DISK (the floppy drive) or Cmd-S.

The soft keys under the screen (F1-F6) switch screens: MAIN, STEP, SONG, PROGRAM, TRIM (sample edit), MIXER. SHIFT + a keypad key opens the mode printed under it (4 SAMPLE, 8 OTHER...).)" },

    { "Loading sounds onto pads", R"(# Loading sounds onto pads
- Drag audio files from Finder onto a pad. Several files fill the following pads.
- Or select a pad (hit it), open PROGRAM (F4), and press LOAD on layer 1. Layers 2-4 let one pad play different sounds at different velocities.
- WAV, AIFF, FLAC and OGG load directly. MP3, M4A and AAC are converted to WAV on import (the screen says so), and saved as WAV with the project.
- To replace a kit sound, just load onto that pad: the old sound is swapped out.
- To record a new sound from your audio input, use SAMPLE (SHIFT + 4); see "Sampling".

Each pad has tune, level, pan, an envelope, a filter and a mute group in PROGRAM. Mute groups let one sound cut off another (open and closed hi-hat).)" },

    { "Chopping a sample", R"(# Chopping a sample
1. Put the sample on a pad (drag it on) and hit that pad to select it.
2. Open TRIM (F5, or SHIFT + 5). The waveform is the selected pad's sample.
3. Pick a CHOP MODE: Threshold finds the hits (raise SENSITIVITY for more slices), Regions cuts equal slices, BPM cuts on the beat, Manual starts empty.
4. Press CHOP (or the H key).
5. Fix the chops by hand:
- Click a slice to hear it.
- Double-click, or Shift-click, to add a chop there.
- Cmd-click or Alt-click deletes the nearest chop.
- Drag a chop line to move it, or nudge the selected one with SLICE START and the DATA wheel.
- DELETE removes the selected slice's chop; Cmd-DELETE clears them all.
- Tap chops in time: press P (PLAY ALL) and press M wherever you want a cut.
- J and K step to the previous / next slice.
6. Use the slices:
- CONVERT SLICES TO PADS puts every slice on its own pad in the chosen program, and can write a sequence that plays the loop back and set the tempo from it.
- PAD PLAYS THIS SLICE makes the current pad play only the selected slice.
- TRIM TO SLICE (T) makes a new sample from just that slice.
Chops aren't saved until you convert them, so you can try as many as you like.)" },

    { "Recording a beat", R"(# Recording a beat
1. On MAIN, pick the SEQUENCE, its BARS and TEMPO (or tap TAP TEMPO a few times).
2. Choose TIMING CORRECT (1/16 is a good start) and SWING (54-62% for a lazy feel).
3. REC (,) records and replaces the track on the first pass, then switches to overdub. OVERDUB (.) always adds. Press PLAY after arming; with COUNT-IN on you get one bar of clicks first.
4. Hold ERASE and hold a pad while playing to remove that pad's notes.
5. UNDO SEQ (Cmd-Z) takes back the last recording or edit.
6. For push or drag, set TIMING SHIFT in OTHER: notes land a few ticks early or late after timing correct.
Each sequence has 64 tracks; pick the track in the track strip at the bottom of MAIN (the TR arrows page through them 8 at a time) and give it a program, or set its OUT to play an external instrument over MIDI. Sequence changes you make while playing happen at the end of the loop.)" },

    { "Note repeat & 16 levels", R"(# Note repeat, 16 levels, full level
- NOTE REPEAT: hold the TAP TEMPO / NOTE REPEAT key and hold pads: they repeat at the NOTE REPEAT rate on MAIN, on the swung grid, louder the harder you press (drag up on a held pad). N latches it on. Tap the same key quickly for tap tempo.
- FULL LEVEL: every hit at full velocity.
- 16 LEVELS: hit the pad you want, press 16 LEVELS, and all 16 pads play that sound in 16 steps of velocity - or tune, decay, attack or filter (choose in OTHER).
- PAD BANK switches the pads between banks A-H (8 x 16 = 128 pads per program). Click a bank letter to jump straight there.)" },

    { "Note variation", R"(# Note variation
The fader on the left varies one thing about the selected pad (or every pad) as you play it:
- TUNE: up to an octave up or down
- DECAY: shorter or longer
- ATTACK: from snappy to a slow fade-in
- FILTER: darker or brighter
1. Press AFTER to turn it on (its light comes on).
2. SHIFT + AFTER (ASSIGN) picks the parameter; OTHER chooses the selected pad or all pads.
3. Move the fader while you play. The value is recorded with each note, and you can change it later in STEP EDIT.)" },

    { "Step edit", R"(# Step edit
STEP (F2) lists every note on the current track, in order.
- Click a note to select it and edit its position, pad, velocity, length, tune and variation.
- STEP < and STEP > on the panel jump from note to note; BAR << >> jump bars; GO TO + a bar number + ENTER jumps anywhere.
- While stopped, hitting a pad inserts that pad at the cursor position. INSERT adds the selected pad, DELETE removes the selected note.
- STEP SIZE sets how far STEP moves when there's no note to jump to.)" },

    { "Editing sequences", R"(# Editing sequences
Press EDIT on MAIN (or SHIFT + 0).
- LOOP: off plays the sequence once and stops. LOOP TO plays bar 1 once, then loops from that bar (an intro, then the groove).
- BARS: insert blank bars before a bar, delete a range of bars, or copy a range of bars (several times, into this or another sequence).
- COPY: copy the whole sequence to another one, or the current track to another track (in any sequence).
- EVENTS: pick the tracks, bars and pads, then change velocity (add, percent, set), move notes earlier/later by ticks (960 ticks = a beat), shift them to other pads (on MIDI tracks: transpose), set their length, or delete them.
UNDO SEQ takes back an edit of the current sequence.

# Tracks
Each sequence has 64 tracks. On MAIN, the TR column next to the tracks pages through them 8 at a time (a * marks pages with notes). In TRACK MUTE, pad banks A-D are tracks 1-16, 17-32, 33-48 and 49-64.)" },

    { "MIDI tracks", R"(# MIDI tracks
A track can play Padlab's sounds, an external instrument over MIDI, or both.
1. Pick the MIDI output in DISK > Audio / MIDI setup.
2. On MAIN, set OUT for the track: DRUM (Padlab's sounds), MIDI ch 1-16 (external only), or DRUM + MIDI (both).
3. Play and record as usual. Pads send notes: bank A pads 1-16 are notes 36-51 (C1 upward), bank B 52-67, and so on. On a MIDI track the note grid shows note names.
Notes go out timed to match the audio. Turn on SEND MIDI CLOCK in OTHER to keep the instrument's tempo in sync too.)" },

    { "Effects", R"(# Effects
Two send effects: a reverb and a delay that follows the tempo.
1. In PROGRAM, set REVERB SEND and DELAY SEND for each pad (a snare with some reverb, a clap into the delay...).
2. In OTHER, set the reverb size, damping and level, and the delay time (1/4 to 1/32T), feedback and level.
The sends are per pad, so one kit can have a dry kick and a wet snare.)" },

    { "Songs", R"(# Songs
SONG (F3) strings sequences together.
1. ADD STEP adds a step; choose its STEP SEQUENCE and REPEATS.
2. INSERT puts a step before the selected one; DELETE removes it.
3. LOOP TO STEP makes the song go round from a step instead of stopping.
4. While the SONG screen is showing, PLAY plays from the selected step and PLAY START from the top. The playing step is marked with >.
5. TO SEQUENCE writes the whole song into an empty sequence so you can edit it as one.)" },

    { "Sampling", R"(# Sampling
SAMPLE (SHIFT + 4) records new sounds.
1. Choose the SOURCE: your audio input, or RESAMPLE to record Padlab's own output.
2. Set REC GAIN (the red knob) so the meters peak below the top, and a THRESHOLD so recording starts when the sound does.
3. Press ARM, play the sound, and STOP.
4. KEEP puts it on the selected pad; KEEP + CHOP takes you straight to TRIM.
Pick the audio input device in DISK > Audio / MIDI setup.)" },

    { "Programs and mixing", R"(# Programs and mixing
- PROGRAM (F4): everything about the selected pad - up to 4 layers by velocity, tune, level, pan, attack/decay/sustain/release, filter, mute group, one-shot or note-on, poly or mono.
- The 4 Q-LINK knobs at the top right are quick tune / cutoff / resonance / decay for the selected pad.
- MIXER (F6): level, pan, mute and solo per track, a pad mixer for the current bank, and the master level.
- OTHER (SHIFT + 8): the converter character - clean, 12-bit / 40 kHz era, or 16-bit warm - metronome, count-in and timing options, and MIDI clock out.

# Syncing other gear
Turn on SEND MIDI CLOCK in OTHER, and pick the MIDI output in DISK > Audio / MIDI setup. Drum machines, synths and DAWs set to follow MIDI clock will then play in time with Padlab, starting and stopping with it.)" },

    { "Panel controls", R"(# Panel controls
- CURSOR (or the arrow keys) moves the brackets between the fields on the screen. You can also click a field.
- DATA wheel: drag around it, scroll over it, or press - and = to change the field under the cursor.
- Keypad: type a number and press ENTER to set the field exactly. SHIFT + key opens the mode printed under it: 1 SONG, 2 STEP, 3 DISK, 4 SAMPLE, 5 TRIM, 6 PROGRAM, 7 MIXER, 8 OTHER, 9 MIDI, 0 EDIT.
- OPEN WINDOW opens the field's full editor (a list or a value box).
- MAIN SCREEN goes back to MAIN.
- MAIN VOLUME sets the output level; REC GAIN the input level for sampling.
- The floppy drive is the DISK menu: new, load, save, save as, audio / MIDI setup.)" },

    { "Keyboard shortcuts", R"(# Keyboard shortcuts
- Pads: Z X C V (1-4), A S D F (5-8), Q W E R (9-12), 1 2 3 4 (13-16). Shift = full velocity.
- Space play / stop, Return play from start, . overdub, , record.
- [ and ] pad bank, N latch note repeat, L full level, G go to.
- Arrow keys cursor, - and = data wheel (Shift = x10), numeric keypad = panel keypad, Esc cancels an entry.
- F1-F6 soft keys. ? opens this help.
- Cmd-Z undo, Cmd-S save, Cmd-Shift-S save as, Cmd-O open, Cmd-N new.
- In TRIM: H chop, M mark a chop, DELETE delete chop, Cmd-DELETE clear chops, J / K previous / next slice, P play slice, T trim to slice.)" },

    { "Saving and loading", R"(# Saving and loading
- Click the floppy drive (or SHIFT + 3) for the DISK menu, or use Cmd-S / Cmd-O / Cmd-N.
- A project is a folder ending in .padlab: project.json plus a samples folder of WAV files, so it's easy to back up or share.
- The title bar on the screen shows the project name; a * means it hasn't been saved yet.)" },
};

constexpr int kNumTopics = (int) (sizeof (kTopics) / sizeof (kTopics[0]));

/** Word-wrapped help text in LCD ink: headings inverse, bullets and steps hang-indented. */
class HelpText : public juce::Component
{
public:
    void setText (const juce::String& t)
    {
        text = t;
        scroll = 0;
        repaint();
    }
    void scrollBy (int px)
    {
        scroll = juce::jlimit (0, juce::jmax (0, contentHeight - getHeight() + 10), scroll + px);
        repaint();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { scrollBy ((int) (-w.deltaY * 120.0f)); }

    void paint (juce::Graphics& g) override
    {
        using namespace theme::lcd;
        const auto font = theme::screenFont (14.5f);
        const float lineGap = 2.0f, width = (float) getWidth() - 14.0f;
        float y = 4.0f - (float) scroll;
        for (auto& raw : juce::StringArray::fromLines (text))
        {
            auto line = raw.trimEnd();
            if (line.isEmpty())
            {
                y += 8.0f;
                continue;
            }
            float indent = 0.0f;
            juce::String bullet;
            if (line.startsWith ("# "))
            {
                const auto head = line.substring (2);
                g.setColour (ink);
                g.fillRect (0.0f, y, width + 8.0f, 20.0f);
                g.setColour (backlight);
                g.setFont (theme::screenFont (16.0f));
                g.drawText (head.toUpperCase(), juce::Rectangle<float> (6.0f, y, width, 20.0f), juce::Justification::centredLeft);
                y += 26.0f;
                continue;
            }
            if (line.startsWith ("- "))
            {
                bullet = "-";
                line = line.substring (2);
                indent = 16.0f;
            }
            else if (line.length() > 2 && juce::CharacterFunctions::isDigit (line[0]) && line[1] == '.')
            {
                bullet = line.upToFirstOccurrenceOf (".", true, false);
                line = line.fromFirstOccurrenceOf (". ", false, false);
                indent = 22.0f;
            }
            juce::GlyphArrangement ga;
            ga.addJustifiedText (font, line, 6.0f + indent, y + font.getAscent(), width - indent, juce::Justification::left);
            const float h = ga.getBoundingBox (0, -1, true).getHeight();
            g.setColour (ink);
            if (bullet.isNotEmpty())
            {
                g.setFont (font);
                g.drawText (bullet, juce::Rectangle<float> (6.0f, y, indent, font.getHeight()), juce::Justification::centredLeft);
            }
            ga.draw (g);
            y += juce::jmax (h, font.getHeight()) + lineGap + 2.0f;
        }
        contentHeight = (int) (y + (float) scroll);
        if (contentHeight > getHeight()) // scroll indicator
        {
            const float frac = (float) getHeight() / (float) contentHeight;
            const float pos = (float) scroll / (float) juce::jmax (1, contentHeight - getHeight());
            g.setColour (grid);
            g.fillRect ((float) getWidth() - 4.0f, 0.0f, 4.0f, (float) getHeight());
            g.setColour (ink);
            g.fillRect ((float) getWidth() - 4.0f, pos * (getHeight() * (1.0f - frac)), 4.0f, getHeight() * frac);
        }
    }

private:
    juce::String text;
    int scroll = 0, contentHeight = 0;
};

class TopicList : public juce::Component
{
public:
    std::function<void (int)> onSelect;
    void setSelected (int i) { selected = juce::jlimit (0, kNumTopics - 1, i); repaint(); }
    int getSelected() const { return selected; }
    void paint (juce::Graphics& g) override
    {
        using namespace theme::lcd;
        g.setColour (ink);
        g.drawRect (getLocalBounds());
        g.setFont (theme::screenFont (14.0f));
        for (int i = 0; i < kNumTopics; ++i)
        {
            auto r = juce::Rectangle<int> (1, 1 + i * rowH, getWidth() - 2, rowH);
            if (i == selected)
            {
                g.setColour (ink);
                g.fillRect (r);
            }
            g.setColour (i == selected ? backlight : ink);
            g.drawText (juce::String (kTopics[i].title).toUpperCase(), r.reduced (6, 0), juce::Justification::centredLeft, true);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = (e.y - 1) / rowH;
        if (i >= 0 && i < kNumTopics && onSelect)
            onSelect (i);
    }
    int rowH = 20;

private:
    int selected = 0;
};

class HelpScreen : public Screen
{
public:
    explicit HelpScreen (AppContext& c) : Screen (c)
    {
        topics.onSelect = [this] (int i) { select (i); };
        backBtn.onClick = [this] { goBack(); };
        for (auto* comp : std::initializer_list<juce::Component*> { &topics, &body, &backBtn, &keysHint })
            addAndMakeVisible (comp);
        keysHint.setFont (theme::screenFont (12.0f));
        keysHint.setColour (juce::Label::textColourId, theme::lcd::textDim);
        keysHint.setText ("UP/DOWN topic   PAGE UP/DOWN or wheel scroll   ESC back", juce::dontSendNotification);
        select (0);
    }

    juce::String getTitle() const override { return "HELP"; }

    void openTopic (const juce::String& title)
    {
        for (int i = 0; i < kNumTopics; ++i)
            if (title.equalsIgnoreCase (kTopics[i].title))
                return select (i);
    }

    bool handleKey (const juce::KeyPress& k) override
    {
        const int code = k.getKeyCode();
        if (code == juce::KeyPress::upKey) { select (topics.getSelected() - 1); return true; }
        if (code == juce::KeyPress::downKey) { select (topics.getSelected() + 1); return true; }
        if (code == juce::KeyPress::pageDownKey) { body.scrollBy (body.getHeight() - 40); return true; }
        if (code == juce::KeyPress::pageUpKey) { body.scrollBy (-(body.getHeight() - 40)); return true; }
        if (code == juce::KeyPress::escapeKey) { goBack(); return true; }
        return false;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto bottom = r.removeFromBottom (24);
        backBtn.setBounds (bottom.removeFromRight (90));
        keysHint.setBounds (bottom);
        r.removeFromBottom (6);
        topics.setBounds (r.removeFromLeft (juce::jmin (250, r.getWidth() / 3)).withHeight (juce::jmin (r.getHeight(), kNumTopics * 20 + 2)));
        r.removeFromLeft (10);
        body.setBounds (r);
    }

private:
    void select (int i)
    {
        i = juce::jlimit (0, kNumTopics - 1, i);
        topics.setSelected (i);
        body.setText (kTopics[i].text);
    }
    void goBack()
    {
        if (ctx.showScreen)
            ctx.showScreen (ctx.previousScreen);
    }

    TopicList topics;
    HelpText body;
    juce::TextButton backBtn { "BACK" };
    juce::Label keysHint;
};
} // namespace

std::unique_ptr<Screen> createHelpScreen (AppContext& c) { return std::make_unique<HelpScreen> (c); }

void helpOpenTopic (Screen& s, const juce::String& topic)
{
    if (auto* h = dynamic_cast<HelpScreen*> (&s))
        h->openTopic (topic);
}

} // namespace padlab
