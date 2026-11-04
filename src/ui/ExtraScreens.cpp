// SONG, STEP EDIT and OTHER screens.

#include "ui/Screens.h"
#include "core/Timing.h"

namespace padlab
{
namespace
{
juce::StringArray divisionNamesList()
{
    juce::StringArray a;
    for (auto& d : timing::divisions())
        a.add (d.name);
    return a;
}

juce::String formatTicks (int64_t tick, int beatsPerBar)
{
    const int64_t barTicks = (int64_t) beatsPerBar * kPPQN;
    const int bar = (int) (tick / barTicks) + 1;
    const int beat = (int) ((tick % barTicks) / kPPQN) + 1;
    const int t = (int) (tick % kPPQN);
    return juce::String (bar).paddedLeft ('0', 3) + "." + juce::String (beat).paddedLeft ('0', 2) + "."
           + juce::String (t).paddedLeft ('0', 3);
}

/** A dot-matrix list: header row, inverse selected row, '>' marker for the "now" row, auto-scroll. */
class LcdList : public juce::Component
{
public:
    std::function<int()> getNumRows;
    std::function<juce::StringArray (int row)> getRow;  // one string per column
    std::function<void (int row)> onSelect;
    juce::StringArray headers;
    juce::Array<float> columnWidths; // fractions of the width

    void setSelected (int r)
    {
        if (r != selected) { selected = r; ensureVisible (r); repaint(); }
    }
    int getSelected() const { return selected; }
    void setMarked (int r)
    {
        if (r != marked) { marked = r; if (r >= 0) ensureVisible (r); repaint(); }
    }
    void update() { repaint(); }

    void paint (juce::Graphics& g) override
    {
        using namespace theme::lcd;
        auto r = getLocalBounds();
        g.setColour (ink);
        g.drawRect (r);
        auto head = r.removeFromTop (rowH);
        g.fillRect (head);
        g.setFont (theme::screenFont (13.0f));
        drawColumns (g, head.reduced (4, 0), headers, backlight);

        const int n = getNumRows ? getNumRows() : 0;
        const int visible = juce::jmax (1, r.getHeight() / rowH);
        scroll = juce::jlimit (0, juce::jmax (0, n - visible), scroll);
        g.setFont (theme::screenFont (14.0f));
        for (int i = 0; i < visible && scroll + i < n; ++i)
        {
            const int row = scroll + i;
            auto rr = juce::Rectangle<int> (r.getX(), r.getY() + i * rowH, r.getWidth(), rowH);
            const bool sel = row == selected;
            if (sel)
            {
                g.setColour (ink);
                g.fillRect (rr.reduced (1, 0));
            }
            g.setColour (sel ? backlight : ink);
            if (row == marked)
                g.drawText (">", rr.withWidth (10).translated (2, 0), juce::Justification::centredLeft);
            drawColumns (g, rr.withTrimmedLeft (12).withTrimmedRight (4), getRow (row), sel ? backlight : ink);
        }
        if (n == 0)
        {
            g.setColour (textDim);
            g.drawText (emptyText, r, juce::Justification::centred);
        }
        if (n > visible) // scroll bar
        {
            auto bar = r.removeFromRight (4).reduced (0, 2);
            g.setColour (grid);
            g.fillRect (bar);
            const float h = bar.getHeight() * (float) visible / (float) n;
            const float y = bar.getY() + (bar.getHeight() - h) * (float) scroll / (float) juce::jmax (1, n - visible);
            g.setColour (ink);
            g.fillRect (juce::Rectangle<float> ((float) bar.getX(), y, 4.0f, h));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = (e.y - rowH) / rowH;
        if (e.y < rowH || ! getNumRows)
            return;
        const int row = scroll + i;
        if (row >= 0 && row < getNumRows())
        {
            setSelected (row);
            if (onSelect)
                onSelect (row);
        }
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        scroll -= (int) std::round (w.deltaY * 8.0f);
        repaint();
    }

    juce::String emptyText;
    int rowH = 18;

private:
    void drawColumns (juce::Graphics& g, juce::Rectangle<int> r, const juce::StringArray& cols, juce::Colour c)
    {
        g.setColour (c);
        const int w = r.getWidth();
        for (int i = 0; i < cols.size(); ++i)
        {
            const int cw = (int) (w * (i < columnWidths.size() ? columnWidths[i] : 0.1f));
            g.drawText (cols[i], r.removeFromLeft (cw), juce::Justification::centredLeft, true);
        }
    }
    void ensureVisible (int r)
    {
        const int visible = juce::jmax (1, (getHeight() - rowH) / rowH);
        if (r < scroll) scroll = r;
        if (r >= scroll + visible) scroll = r - visible + 1;
    }
    int selected = -1, marked = -1, scroll = 0;
};

void placeField (juce::Rectangle<int>& row, Caption& cap, juce::Component& c, int w, int h = 24)
{
    auto cell = row.removeFromLeft (w);
    row.removeFromLeft (8);
    cap.setBounds (cell.removeFromTop (14));
    c.setBounds (cell.withHeight (h));
}

// =================================================================== SONG

class SongScreen : public Screen
{
public:
    explicit SongScreen (AppContext& c) : Screen (c)
    {
        juce::StringArray songs;
        for (int i = 0; i < kNumSongs; ++i)
            songs.add (juce::String (i + 1).paddedLeft ('0', 2));
        setupCombo (songBox, songs);
        songBox.onChange = [this] {
            if (loading)
                return;
            ctx.engine.updateSettings ([v = songBox.getSelectedId() - 1] (PerformanceSettings& s) {
                s.currentSong = v;
                s.songStartStep = 0;
            });
            list.setSelected (0);
            contextChanged();
        };
        nameEdit.onTextChange = [this] {
            if (! loading)
                editSong ([t = nameEdit.getText()] (Song& s) { s.name = t; }, false);
        };
        nameEdit.setJustification (juce::Justification::centredLeft);

        loop.setButtonText ("Loop to step");
        loop.onClick = [this] { editSong ([v = loop.getToggleState()] (Song& s) { s.loop = v; }); };
        setupParam (loopTo, 1, kMaxSongSteps, 1);
        loopTo.onValueChange = [this] {
            if (! loading)
                editSong ([v = (int) loopTo.getValue() - 1] (Song& s) { s.loopToStep = juce::jlimit (0, juce::jmax (0, s.numSteps - 1), v); });
        };

        setupCombo (stepSeq, {});
        stepSeq.onChange = [this] {
            if (loading || list.getSelected() < 0)
                return;
            editSong ([i = list.getSelected(), v = stepSeq.getSelectedId() - 1] (Song& s) {
                if (i < s.numSteps) s.steps[(size_t) i].sequence = v;
            });
        };
        setupParam (stepReps, 1, 99, 1, "x");
        stepReps.onValueChange = [this] {
            if (loading || list.getSelected() < 0)
                return;
            editSong ([i = list.getSelected(), v = (int) stepReps.getValue()] (Song& s) {
                if (i < s.numSteps) s.steps[(size_t) i].repeats = v;
            });
        };

        insertBtn.onClick = [this] { addStep (juce::jmax (0, list.getSelected())); };
        addBtn.onClick = [this] { addStep (-1); };
        deleteBtn.onClick = [this] {
            const int i = list.getSelected();
            editSong ([i] (Song& s) { s.removeStep (i); });
            list.setSelected (juce::jmax (0, i - 1));
            selectionChanged();
        };
        toSeqBtn.setTooltip ("Write the whole song into an empty sequence, one after another");
        toSeqBtn.onClick = [this] { convertToSequence(); };

        list.headers = { "STEP", "SEQUENCE", "REPS", "BARS", "BPM" };
        list.columnWidths = { 0.12f, 0.46f, 0.14f, 0.14f, 0.14f };
        list.emptyText = "Empty song - press ADD STEP";
        list.getNumRows = [this] { return (int) rows.size(); };
        list.getRow = [this] (int r) { return rows[(size_t) r]; };
        list.onSelect = [this] (int) { selectionChanged(); };

        for (auto* comp : std::initializer_list<juce::Component*> { &capSong, &songBox, &capName, &nameEdit, &loop, &loopTo,
                                                                    &list, &capStepSeq, &stepSeq, &capStepReps, &stepReps,
                                                                    &insertBtn, &addBtn, &deleteBtn, &toSeqBtn, &info, &hint })
            addAndMakeVisible (comp);
        info.setFont (theme::screenFont (15.0f));
        info.setColour (juce::Label::textColourId, theme::lcd::ink);
        hint.setFont (theme::screenFont (12.0f));
        hint.setColour (juce::Label::textColourId, theme::lcd::textDim);
        hint.setText ("PLAY: from the selected step   PLAY START: from step 1", juce::dontSendNotification);
    }

    juce::String getTitle() const override { return "SONG"; }
    juce::String getHelpTopic() const override { return "Songs"; }

    void visibilityChanged() override
    {
        // like the hardware: while the Song screen is up, PLAY plays the song
        ctx.engine.updateSettings ([v = isVisible()] (PerformanceSettings& s) { s.songMode = v; });
        Screen::visibilityChanged();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto row1 = r.removeFromTop (40);
        placeField (row1, capSong, songBox, 70);
        placeField (row1, capName, nameEdit, juce::jmin (220, row1.getWidth() / 3));
        auto lp = row1.removeFromLeft (230).withTrimmedTop (14).withHeight (24);
        loop.setBounds (lp.removeFromLeft (130));
        loopTo.setBounds (lp);
        info.setBounds (row1.withTrimmedTop (14));

        r.removeFromTop (6);
        hint.setBounds (r.removeFromBottom (16));
        auto bottom = r.removeFromBottom (44);
        placeField (bottom, capStepSeq, stepSeq, juce::jmin (240, bottom.getWidth() / 3));
        placeField (bottom, capStepReps, stepReps, 90);
        auto btns = bottom.withTrimmedTop (14).withHeight (24);
        const int bw = juce::jmin (110, (btns.getWidth() - 18) / 4);
        for (auto* b : { &insertBtn, &addBtn, &deleteBtn, &toSeqBtn })
        {
            b->setBounds (btns.removeFromLeft (b == &toSeqBtn ? bw + 20 : bw));
            btns.removeFromLeft (6);
        }
        r.removeFromBottom (6);
        list.setBounds (r);
    }

    void contextChanged() override
    {
        const juce::ScopedValueSetter<bool> svs (loading, true);
        const auto s = ctx.engine.getSettings();
        songBox.setSelectedId (s.currentSong + 1, juce::dontSendNotification);
        juce::StringArray seqNames;
        rows.clear();
        int totalBars = 0;
        double totalSeconds = 0.0;
        ctx.engine.withProject ([&] (Project& p) {
            for (int i = 0; i < kNumSequences; ++i)
                seqNames.add (juce::String (i + 1).paddedLeft ('0', 2) + "  " + p.sequences[(size_t) i].name);
            const auto& sg = p.songs[(size_t) s.currentSong];
            nameEdit.setText (sg.name, juce::dontSendNotification);
            loop.setToggleState (sg.loop, juce::dontSendNotification);
            loopTo.setRange (1, juce::jmax (2, sg.numSteps), 1); // a slider needs end > start
            loopTo.setValue (sg.loopToStep + 1, juce::dontSendNotification);
            loopTo.setEnabled (sg.loop && sg.numSteps > 1);
            for (int i = 0; i < sg.numSteps; ++i)
            {
                const auto& st = sg.steps[(size_t) i];
                const auto& seq = p.sequences[(size_t) st.sequence];
                rows.push_back ({ juce::String (i + 1).paddedLeft ('0', 2), seqNames[st.sequence], juce::String (st.repeats),
                                  juce::String (seq.bars * st.repeats), juce::String (seq.bpm, 1) });
                totalBars += seq.bars * st.repeats;
                totalSeconds += seq.bars * seq.beatsPerBar * 60.0 / seq.bpm * st.repeats;
            }
            numSteps = sg.numSteps;
        });
        setupCombo (stepSeq, seqNames);
        const int secs = (int) std::round (totalSeconds);
        info.setText ("TOTAL " + juce::String (totalBars) + " BARS  " + juce::String (secs / 60) + ":"
                          + juce::String (secs % 60).paddedLeft ('0', 2),
                      juce::dontSendNotification);
        if (list.getSelected() >= numSteps || list.getSelected() < 0)
            list.setSelected (juce::jmax (0, juce::jmin (s.songStartStep, numSteps - 1)));
        list.update();
        selectionChanged (false);
    }

    void refresh() override
    {
        if (ctx.engine.isPlayingSong())
        {
            list.setMarked (ctx.engine.getSongStep());
            const auto st = ctx.engine.getSongStep();
            if (st >= 0 && st < (int) rows.size())
                hint.setText ("PLAYING STEP " + juce::String (st + 1) + "  PASS " + juce::String (ctx.engine.getSongRepeat() + 1)
                                  + " OF " + rows[(size_t) st][2],
                              juce::dontSendNotification);
        }
        else if (list.getSelected() >= 0)
        {
            list.setMarked (-1);
            hint.setText ("PLAY: from the selected step   PLAY START: from step 1", juce::dontSendNotification);
        }
    }

private:
    template <typename Fn>
    void editSong (Fn&& fn, bool reload = true)
    {
        const int song = ctx.engine.getSettings().currentSong;
        ctx.engine.withProject ([&] (Project& p) { fn (p.songs[(size_t) song]); });
        if (reload)
            contextChanged();
    }

    void addStep (int at)
    {
        const int seq = ctx.engine.getCurrentSequence();
        int inserted = -1;
        editSong ([&] (Song& s) {
            const int idx = at < 0 ? s.numSteps : at;
            const int seqToUse = (at >= 0 && at < s.numSteps) ? s.steps[(size_t) at].sequence : seq;
            if (s.insertStep (idx, { seqToUse, 1 }))
                inserted = idx;
        });
        if (inserted < 0)
            return flashMsg ("Song is full (99 steps)");
        list.setSelected (inserted);
        selectionChanged();
    }

    void selectionChanged (bool storeStart = true)
    {
        const juce::ScopedValueSetter<bool> svs (loading, true);
        const int i = list.getSelected();
        const bool valid = i >= 0 && i < numSteps;
        stepSeq.setEnabled (valid);
        stepReps.setEnabled (valid);
        deleteBtn.setEnabled (valid);
        if (valid)
        {
            const int song = ctx.engine.getSettings().currentSong;
            SongStep st;
            ctx.engine.withProject ([&] (Project& p) { st = p.songs[(size_t) song].steps[(size_t) i]; });
            stepSeq.setSelectedId (st.sequence + 1, juce::dontSendNotification);
            stepReps.setValue (st.repeats, juce::dontSendNotification);
            if (storeStart)
                ctx.engine.updateSettings ([i] (PerformanceSettings& s) { s.songStartStep = i; });
        }
    }

    void convertToSequence()
    {
        const int song = ctx.engine.getSettings().currentSong;
        juce::String result;
        ctx.engine.pushUndo ("song to sequence");
        int target = -1;
        ctx.engine.withProject ([&] (Project& p) {
            const auto& sg = p.songs[(size_t) song];
            if (sg.numSteps == 0)
                return void (result = "Song is empty");
            for (int i = kNumSequences - 1; i >= 0 && target < 0; --i)
            {
                bool empty = true;
                for (auto& t : p.sequences[(size_t) i].tracks)
                    empty = empty && t.events.empty();
                if (empty)
                    target = i;
            }
            if (target < 0)
                return void (result = "No empty sequence to write into");
            Sequence out;
            out.name = sg.name;
            out.bpm = p.sequences[(size_t) sg.steps[0].sequence].bpm;
            out.beatsPerBar = p.sequences[(size_t) sg.steps[0].sequence].beatsPerBar;
            int bars = 0;
            int64_t offset = 0;
            for (int i = 0; i < sg.numSteps; ++i)
            {
                const auto& src = p.sequences[(size_t) sg.steps[(size_t) i].sequence];
                for (int r = 0; r < sg.steps[(size_t) i].repeats; ++r)
                {
                    if (bars + src.bars > 64)
                        break;
                    for (int t = 0; t < kNumTracks; ++t)
                    {
                        out.tracks[(size_t) t].programIndex = src.tracks[(size_t) t].programIndex;
                        for (auto e : src.tracks[(size_t) t].events)
                        {
                            e.tick = offset + e.tick * out.beatsPerBar / juce::jmax (1, src.beatsPerBar);
                            out.tracks[(size_t) t].insertSorted (e);
                        }
                    }
                    bars += src.bars;
                    offset += (int64_t) src.bars * out.beatsPerBar * kPPQN;
                }
            }
            out.bars = juce::jmax (1, bars);
            p.sequences[(size_t) target] = out;
            result = "Song written to sequence " + juce::String (target + 1) + " (" + juce::String (out.bars) + " bars)";
        });
        flashMsg (result);
        ctx.changed();
    }

    void flashMsg (const juce::String& m)
    {
        if (ctx.flashMessage)
            ctx.flashMessage (m);
    }

    bool loading = false;
    int numSteps = 0;
    std::vector<juce::StringArray> rows;
    Caption capSong { "Song" }, capName { "Name" }, capStepSeq { "Step sequence" }, capStepReps { "Repeats" };
    juce::ComboBox songBox, stepSeq;
    juce::TextEditor nameEdit;
    juce::ToggleButton loop;
    juce::Slider loopTo, stepReps;
    LcdList list;
    juce::TextButton insertBtn { "INSERT" }, addBtn { "ADD STEP" }, deleteBtn { "DELETE" }, toSeqBtn { "TO SEQUENCE" };
    juce::Label info, hint;
};

// =================================================================== STEP EDIT

class StepEditScreen : public Screen
{
public:
    explicit StepEditScreen (AppContext& c) : Screen (c)
    {
        setupCombo (stepSize, divisionNamesList());
        stepSize.onChange = [this] {
            if (! loading)
                ctx.engine.updateSettings ([v = stepSize.getSelectedId() - 1] (PerformanceSettings& s) { s.timingCorrect = v; });
        };
        list.headers = { "POSITION", "PAD", "SOUND", "VELO", "LENGTH", "TUNE", "VARIATION" };
        list.columnWidths = { 0.17f, 0.08f, 0.22f, 0.09f, 0.11f, 0.11f, 0.22f };
        list.emptyText = "No events on this track - hit a pad to insert one at the cursor";
        list.getNumRows = [this] { return (int) rows.size(); };
        list.getRow = [this] (int r) { return rows[(size_t) r]; };
        list.onSelect = [this] (int r) {
            if (r >= 0 && r < (int) events.size())
                ctx.engine.locate (events[(size_t) r].tick);
            selectionChanged();
        };

        setupParam (posField, 0, 1, 1);
        posField.textFromValueFunction = [this] (double v) { return formatTicks ((int64_t) v, beatsPerBar); };
        posField.onValueChange = [this] { editSelected ([this] (NoteEvent& e) { e.tick = (int64_t) posField.getValue(); }, true); };
        setupParam (padField, 0, kNumPads - 1, 1);
        padField.textFromValueFunction = [] (double v) { return AppContext::padName ((int) v); };
        padField.onValueChange = [this] { editSelected ([this] (NoteEvent& e) { e.pad = (int16_t) padField.getValue(); }); };
        setupParam (veloField, 1, 127, 1);
        veloField.onValueChange = [this] { editSelected ([this] (NoteEvent& e) { e.velocity = (uint8_t) veloField.getValue(); }); };
        setupParam (lenField, 1, kPPQN * 4, 1, " t");
        lenField.onValueChange = [this] { editSelected ([this] (NoteEvent& e) { e.length = (int32_t) lenField.getValue(); }); };
        setupParam (tuneField, -24, 24, 0.1, " st");
        tuneField.onValueChange = [this] { editSelected ([this] (NoteEvent& e) { e.tune = (float) tuneField.getValue(); }); };
        setupCombo (varType, { "None", "Decay", "Attack", "Filter" });
        varType.onChange = [this] {
            editSelected ([this] (NoteEvent& e) {
                const int id = varType.getSelectedId();
                e.variation.type = id <= 1 ? VariationType::None : (VariationType) (id); // 2,3,4 = Decay/Attack/Filter
            });
        };
        setupParam (varValue, 0, 127, 1);
        varValue.onValueChange = [this] { editSelected ([this] (NoteEvent& e) { e.variation.value = (uint8_t) varValue.getValue(); }); };

        prevBtn.onClick = [this] { stepTo (-1); };
        nextBtn.onClick = [this] { stepTo (1); };
        insertBtn.onClick = [this] { insertAtCursor (ctx.selectedPad, 100); };
        deleteBtn.onClick = [this] {
            const int i = list.getSelected();
            if (i < 0 || i >= (int) events.size())
                return;
            ctx.engine.pushUndo ("delete event");
            const auto victim = events[(size_t) i];
            editTrack ([&] (Track& t) {
                for (auto it = t.events.begin(); it != t.events.end(); ++it)
                    if (it->tick == victim.tick && it->pad == victim.pad)
                    {
                        t.events.erase (it);
                        break;
                    }
            });
            reload();
            list.setSelected (juce::jmin (i, (int) events.size() - 1));
            selectionChanged();
        };

        for (auto* comp : std::initializer_list<juce::Component*> { &info, &capStep, &stepSize, &prevBtn, &nextBtn, &insertBtn,
                                                                    &deleteBtn, &list, &capPos, &posField, &capPad, &padField,
                                                                    &capVelo, &veloField, &capLen, &lenField, &capTune, &tuneField,
                                                                    &capVar, &varType, &capVarVal, &varValue, &hint })
            addAndMakeVisible (comp);
        info.setFont (theme::screenFont (16.0f));
        info.setColour (juce::Label::textColourId, theme::lcd::ink);
        hint.setFont (theme::screenFont (12.0f));
        hint.setColour (juce::Label::textColourId, theme::lcd::textDim);
        hint.setText ("STEP < > on the panel move event to event. While stopped, hitting a pad inserts it at the cursor.",
                      juce::dontSendNotification);
    }

    juce::String getTitle() const override { return "STEP EDIT"; }
    juce::String getHelpTopic() const override { return "Step edit"; }

    void visibilityChanged() override
    {
        // the hardware inserts a note at the cursor when you hit a pad in step edit (while stopped)
        if (isVisible())
            ctx.onPadHit = [this] (int pad, int velocity) {
                if (ctx.engine.getTransportState() == Transport::State::Stopped)
                    insertAtCursor (pad, velocity);
            };
        else
            ctx.onPadHit = nullptr;
        Screen::visibilityChanged();
    }

    /** Panel STEP keys: jump to the previous/next event (or grid step when there's none). */
    void stepTo (int dir)
    {
        const int64_t now = (int64_t) std::llround (ctx.engine.getPositionTicks());
        int64_t target = -1;
        if (dir > 0)
        {
            for (auto& e : events)
                if (e.tick > now) { target = e.tick; break; }
        }
        else
        {
            for (auto it = events.rbegin(); it != events.rend(); ++it)
                if (it->tick < now) { target = it->tick; break; }
        }
        if (target < 0)
        {
            const auto s = ctx.engine.getSettings();
            const auto& div = timing::divisions()[(size_t) s.timingCorrect];
            const int64_t step = juce::jmax<int64_t> (1, div.ticks);
            target = juce::jlimit<int64_t> (0, juce::jmax<int64_t> (0, lengthTicks - 1), (now / step + dir) * step);
        }
        ctx.engine.locate (target);
        for (int i = 0; i < (int) events.size(); ++i)
            if (events[(size_t) i].tick == target) { list.setSelected (i); break; }
        selectionChanged();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto row1 = r.removeFromTop (40);
        info.setBounds (row1.removeFromLeft (juce::jmin (300, row1.getWidth() / 2)).withTrimmedTop (12));
        placeField (row1, capStep, stepSize, 80);
        auto btns = row1.withTrimmedTop (14).withHeight (24);
        for (auto* b : { &prevBtn, &nextBtn, &insertBtn, &deleteBtn })
        {
            b->setBounds (btns.removeFromLeft (juce::jmin (84, (btns.getWidth() - 18) / 4 + 1)));
            btns.removeFromLeft (6);
        }
        r.removeFromTop (6);
        hint.setBounds (r.removeFromBottom (16));
        auto edit = r.removeFromBottom (44);
        const int w = (edit.getWidth() - 6 * 8) / 7;
        placeField (edit, capPos, posField, w + 20);
        placeField (edit, capPad, padField, w - 10);
        placeField (edit, capVelo, veloField, w - 10);
        placeField (edit, capLen, lenField, w);
        placeField (edit, capTune, tuneField, w);
        placeField (edit, capVar, varType, w);
        placeField (edit, capVarVal, varValue, juce::jmax (40, edit.getWidth()));
        r.removeFromBottom (6);
        list.setBounds (r);
    }

    void contextChanged() override
    {
        const juce::ScopedValueSetter<bool> svs (loading, true);
        stepSize.setSelectedId (ctx.engine.getSettings().timingCorrect + 1, juce::dontSendNotification);
        reload();
        if ((list.getSelected() < 0 || list.getSelected() >= (int) events.size()) && ! events.empty())
        {
            // start on the event at (or after) the cursor
            const int64_t now = (int64_t) std::llround (ctx.engine.getPositionTicks());
            int pick = 0;
            while (pick < (int) events.size() - 1 && events[(size_t) pick].tick < now)
                ++pick;
            list.setSelected (pick);
        }
        selectionChanged();
    }

    void refresh() override
    {
        // follow external changes (recording, undo) cheaply
        const int seq = ctx.engine.getCurrentSequence(), track = ctx.currentTrack();
        size_t count = 0;
        ctx.engine.withProject ([&] (Project& p) { count = p.sequences[(size_t) seq].tracks[(size_t) track].events.size(); });
        if (seq != lastSeq || track != lastTrack || count != events.size())
            contextChanged();
        const int64_t now = (int64_t) std::llround (ctx.engine.getPositionTicks());
        int marked = -1;
        for (int i = 0; i < (int) events.size(); ++i)
            if (events[(size_t) i].tick <= now)
                marked = i;
        list.setMarked (marked);
        const auto text = "SEQ " + juce::String (seq + 1).paddedLeft ('0', 2) + "  TR " + juce::String (track + 1) + "   NOW "
                          + formatTicks (now, beatsPerBar);
        if (info.getText() != text)
            info.setText (text, juce::dontSendNotification);
    }

private:
    template <typename Fn>
    void editTrack (Fn&& fn)
    {
        const int seq = ctx.engine.getCurrentSequence(), track = ctx.currentTrack();
        ctx.engine.withProject ([&] (Project& p) { fn (p.sequences[(size_t) seq].tracks[(size_t) track]); });
    }

    template <typename Fn>
    void editSelected (Fn&& fn, bool resort = false)
    {
        if (loading)
            return;
        const int i = list.getSelected();
        if (i < 0 || i >= (int) events.size())
            return;
        const auto before = events[(size_t) i];
        NoteEvent after = before;
        fn (after);
        after.tick = juce::jlimit<int64_t> (0, juce::jmax<int64_t> (0, lengthTicks - 1), after.tick);
        editTrack ([&] (Track& t) {
            for (auto it = t.events.begin(); it != t.events.end(); ++it)
                if (it->tick == before.tick && it->pad == before.pad)
                {
                    if (resort)
                    {
                        t.events.erase (it);
                        t.insertSorted (after);
                    }
                    else
                        *it = after;
                    break;
                }
        });
        reload();
        for (int k = 0; k < (int) events.size(); ++k)
            if (events[(size_t) k].tick == after.tick && events[(size_t) k].pad == after.pad)
                list.setSelected (k);
        if (resort)
            ctx.engine.locate (after.tick);
    }

    void insertAtCursor (int pad, int velocity)
    {
        const int64_t now = juce::jlimit<int64_t> (0, juce::jmax<int64_t> (0, lengthTicks - 1),
                                                   (int64_t) std::llround (ctx.engine.getPositionTicks()));
        ctx.engine.pushUndo ("insert event");
        NoteEvent e;
        e.tick = now;
        e.pad = (int16_t) juce::jlimit (0, kNumPads - 1, pad);
        e.velocity = (uint8_t) juce::jlimit (1, 127, velocity);
        editTrack ([&] (Track& t) { t.insertSorted (e); });
        reload();
        for (int k = (int) events.size() - 1; k >= 0; --k)
            if (events[(size_t) k].tick == e.tick && events[(size_t) k].pad == e.pad)
            {
                list.setSelected (k);
                break;
            }
        selectionChanged();
    }

    void reload()
    {
        const int seq = ctx.engine.getCurrentSequence(), track = ctx.currentTrack();
        lastSeq = seq;
        lastTrack = track;
        const int prog = ctx.currentProgram();
        std::array<juce::String, kNumPads> names;
        ctx.engine.withProject ([&] (Project& p) {
            const auto& sq = p.sequences[(size_t) seq];
            events = sq.tracks[(size_t) track].events;
            beatsPerBar = sq.beatsPerBar;
            lengthTicks = sq.getLengthTicks();
            for (int i = 0; i < kNumPads; ++i)
            {
                const auto& l = p.programs[(size_t) prog].pads[(size_t) i].layers[0];
                names[(size_t) i] = l.sample != nullptr ? l.sample->name : juce::String ("-");
            }
        });
        rows.clear();
        static const char* varNames[] = { "", "TUNE", "DECAY", "ATTACK", "FILTER" };
        for (auto& e : events)
            rows.push_back ({ formatTicks (e.tick, beatsPerBar), AppContext::padName (e.pad), names[(size_t) e.pad],
                              juce::String ((int) e.velocity), juce::String (e.length),
                              e.tune == 0.0f ? juce::String ("0") : juce::String (e.tune, 1),
                              e.variation.type == VariationType::None
                                  ? juce::String ("-")
                                  : juce::String (varNames[(int) e.variation.type]) + " " + juce::String ((int) e.variation.value) });
        posField.setRange (0, (double) juce::jmax<int64_t> (1, lengthTicks - 1), 1);
        list.update();
    }

    void selectionChanged()
    {
        const juce::ScopedValueSetter<bool> svs (loading, true);
        const int i = list.getSelected();
        const bool valid = i >= 0 && i < (int) events.size();
        for (auto* comp : std::initializer_list<juce::Component*> { &posField, &padField, &veloField, &lenField, &tuneField,
                                                                    &varType, &varValue, &deleteBtn })
            comp->setEnabled (valid);
        if (! valid)
            return;
        const auto& e = events[(size_t) i];
        posField.setValue ((double) e.tick, juce::dontSendNotification);
        posField.updateText();
        padField.setValue (e.pad, juce::dontSendNotification);
        veloField.setValue (e.velocity, juce::dontSendNotification);
        lenField.setValue (e.length, juce::dontSendNotification);
        tuneField.setValue (e.tune, juce::dontSendNotification);
        varType.setSelectedId (e.variation.type == VariationType::None || e.variation.type == VariationType::Tune
                                   ? 1
                                   : (int) e.variation.type,
                               juce::dontSendNotification);
        varValue.setValue (e.variation.value, juce::dontSendNotification);
    }

    bool loading = false;
    int lastSeq = -1, lastTrack = -1, beatsPerBar = 4;
    int64_t lengthTicks = kPPQN * 8;
    std::vector<NoteEvent> events;
    std::vector<juce::StringArray> rows;
    juce::Label info, hint;
    Caption capStep { "Step size" }, capPos { "Position" }, capPad { "Pad" }, capVelo { "Velocity" }, capLen { "Length" },
        capTune { "Tune" }, capVar { "Variation" }, capVarVal { "Var value" };
    juce::ComboBox stepSize, varType;
    juce::Slider posField, padField, veloField, lenField, tuneField, varValue;
    juce::TextButton prevBtn { "< STEP" }, nextBtn { "STEP >" }, insertBtn { "INSERT" }, deleteBtn { "DELETE" };
    LcdList list;
};

// =================================================================== OTHER

class OtherScreen : public Screen
{
public:
    explicit OtherScreen (AppContext& c) : Screen (c)
    {
        setupCombo (vintage, { "Clean (modern)", "12-bit / 40 kHz era", "16-bit warm" });
        vintage.onChange = [this] {
            if (! loading)
                ctx.engine.withProject ([v = vintage.getSelectedId() - 1] (Project& p) { p.vintageMode = v; });
        };
        // send effects
        setupParam (revSize, 0, 100, 1, " %");
        revSize.onValueChange = [this] { fx ([v = (float) revSize.getValue() / 100.0f] (FxSettings& f) { f.reverbSize = v; }); };
        setupParam (revDamp, 0, 100, 1, " %");
        revDamp.onValueChange = [this] { fx ([v = (float) revDamp.getValue() / 100.0f] (FxSettings& f) { f.reverbDamping = v; }); };
        setupParam (revReturn, -40, 6, 0.5, " dB");
        revReturn.onValueChange = [this] { fx ([v = (float) revReturn.getValue()] (FxSettings& f) { f.reverbReturnDb = v; }); };
        {
            juce::StringArray divs;
            for (int i = 0; i < timing::kNumDivisions - 1; ++i)
                divs.add (timing::divisions()[(size_t) i].name);
            setupCombo (dlyTime, divs);
        }
        dlyTime.onChange = [this] { fx ([v = dlyTime.getSelectedId() - 1] (FxSettings& f) { f.delayDivision = v; }); };
        setupParam (dlyFeedback, 0, 90, 1, " %");
        dlyFeedback.onValueChange = [this] { fx ([v = (float) dlyFeedback.getValue() / 100.0f] (FxSettings& f) { f.delayFeedback = v; }); };
        setupParam (dlyReturn, -40, 6, 0.5, " dB");
        dlyReturn.onValueChange = [this] { fx ([v = (float) dlyReturn.getValue()] (FxSettings& f) { f.delayReturnDb = v; }); };

        setupParam (masterLevel, -24, 6, 0.1, " dB");
        masterLevel.onValueChange = [this] {
            if (! loading)
                ctx.engine.withProject ([v = (float) masterLevel.getValue()] (Project& p) { p.masterLevelDb = v; });
        };

        setupCombo (nvType, { "Tune", "Decay", "Attack", "Filter" });
        nvType.onChange = [this] {
            if (! loading)
                ctx.engine.updateSettings ([v = nvType.getSelectedId()] (PerformanceSettings& s) { s.noteVariationType = (VariationType) v; });
        };
        setupCombo (nvTarget, { "Selected pad", "All pads" });
        nvTarget.onChange = [this] { applyTarget(); };
        nvOn.setButtonText ("Note variation on");
        nvOn.onClick = [this] {
            ctx.engine.updateSettings ([v = nvOn.getToggleState()] (PerformanceSettings& s) { s.noteVariationOn = v; });
        };
        setupCombo (sixteenParam, { "Velocity", "Tune", "Decay", "Attack", "Filter" });
        sixteenParam.onChange = [this] {
            if (! loading)
                ctx.engine.updateSettings ([v = sixteenParam.getSelectedId() - 1] (PerformanceSettings& s) { s.sixteenLevelsParam = v; });
        };

        metronome.setButtonText ("Metronome");
        metronome.onClick = [this] { ctx.engine.updateSettings ([v = metronome.getToggleState()] (PerformanceSettings& s) { s.metronome = v; }); };
        clickRecOnly.setButtonText ("Only while recording");
        clickRecOnly.onClick = [this] {
            ctx.engine.updateSettings ([v = clickRecOnly.getToggleState()] (PerformanceSettings& s) { s.metronomeOnlyWhenRecording = v; });
        };
        countIn.setButtonText ("Count-in");
        countIn.onClick = [this] { ctx.engine.updateSettings ([v = countIn.getToggleState()] (PerformanceSettings& s) { s.countIn = v; }); };
        autoOverdub.setButtonText ("REC becomes overdub after one pass");
        autoOverdub.onClick = [this] {
            ctx.engine.updateSettings ([v = autoOverdub.getToggleState()] (PerformanceSettings& s) { s.autoOverdub = v; });
        };
        setupParam (shift, -48, 48, 1, " ticks");
        shift.setTooltip ("TIMING SHIFT: after timing correct, notes land this many ticks late (+) or early (-). 960 ticks per beat.");
        shift.onValueChange = [this] {
            if (! loading)
                ctx.engine.updateSettings ([v = (int) shift.getValue()] (PerformanceSettings& s) { s.shiftTicks = v; });
        };
        midiClock.setButtonText ("Send MIDI clock");
        midiClock.setTooltip ("Clock, start/stop and song position to the MIDI output chosen in DISK > Audio / MIDI setup");
        midiClock.onClick = [this] {
            ctx.engine.setSendMidiClock (midiClock.getToggleState());
            const auto name = ctx.engine.getMidiClockOutputName();
            if (ctx.flashMessage)
                ctx.flashMessage (midiClock.getToggleState() ? (name.isEmpty() ? juce::String ("MIDI clock on - pick a MIDI output in DISK > Audio/MIDI setup")
                                                                                : "MIDI clock to " + name)
                                                             : juce::String ("MIDI clock off"));
        };
        setupParam (tcStrength, 0, 100, 1, "%");
        tcStrength.onValueChange = [this] {
            if (! loading)
                ctx.engine.updateSettings ([v = tcStrength.getValue() / 100.0] (PerformanceSettings& s) { s.tcStrength = v; });
        };

        for (auto* comp : std::initializer_list<juce::Component*> { &secSound, &capVintage, &vintage, &capMaster, &masterLevel,
                                                                    &secVar, &capNvType, &nvType, &capNvTarget, &nvTarget, &nvOn,
                                                                    &capSixteen, &sixteenParam, &secTiming, &metronome,
                                                                    &clickRecOnly, &countIn, &autoOverdub, &capTcStrength,
                                                                    &tcStrength, &capShift, &shift, &midiClock, &midiInfo,
                                                                    &secFx, &capRevSize, &revSize, &capRevDamp, &revDamp, &capRevReturn,
                                                                    &revReturn, &capDlyTime, &dlyTime, &capDlyFeedback, &dlyFeedback,
                                                                    &capDlyReturn, &dlyReturn })
            addAndMakeVisible (comp);
        for (auto* l : { &secSound, &secVar, &secTiming, &secFx })
        {
            l->setFont (theme::screenFont (14.0f, true));
            l->setColour (juce::Label::textColourId, theme::lcd::ink);
        }
        midiInfo.setFont (theme::screenFont (12.0f));
        midiInfo.setColour (juce::Label::textColourId, theme::lcd::textDim);
        midiInfo.setJustificationType (juce::Justification::topLeft);
        midiInfo.setMinimumHorizontalScale (1.0f); // wrap instead of squashing
        updateMidiInfo();
    }

    juce::String getTitle() const override { return "OTHER"; }
    juce::String getHelpTopic() const override { return "Note variation"; }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto section = [&] (juce::Label& title) {
            title.setBounds (r.removeFromTop (20));
            r.removeFromTop (2);
        };
        section (secSound);
        auto row = r.removeFromTop (40);
        placeField (row, capVintage, vintage, 220);
        placeField (row, capMaster, masterLevel, 140);
        r.removeFromTop (6);
        section (secFx);
        row = r.removeFromTop (40);
        placeField (row, capRevSize, revSize, 96);
        placeField (row, capRevDamp, revDamp, 96);
        placeField (row, capRevReturn, revReturn, 104);
        row.removeFromLeft (14);
        placeField (row, capDlyTime, dlyTime, 84);
        placeField (row, capDlyFeedback, dlyFeedback, 96);
        placeField (row, capDlyReturn, dlyReturn, 104);
        r.removeFromTop (6);
        section (secVar);
        row = r.removeFromTop (40);
        placeField (row, capNvType, nvType, 120);
        placeField (row, capNvTarget, nvTarget, 150);
        nvOn.setBounds (row.removeFromLeft (200).withTrimmedTop (14).withHeight (24));
        placeField (row, capSixteen, sixteenParam, 130);
        r.removeFromTop (10);
        section (secTiming);
        row = r.removeFromTop (26);
        metronome.setBounds (row.removeFromLeft (130));
        clickRecOnly.setBounds (row.removeFromLeft (200));
        countIn.setBounds (row.removeFromLeft (110));
        row = r.removeFromTop (40);
        autoOverdub.setBounds (row.removeFromLeft (310).withTrimmedTop (14).withHeight (24));
        placeField (row, capTcStrength, tcStrength, 140);
        placeField (row, capShift, shift, 120);
        r.removeFromTop (10);
        midiClock.setBounds (r.removeFromTop (26).removeFromLeft (220));
        r.removeFromTop (4);
        midiInfo.setBounds (r.removeFromTop (40));
    }

    void contextChanged() override
    {
        const juce::ScopedValueSetter<bool> svs (loading, true);
        const auto s = ctx.engine.getSettings();
        ctx.engine.withProject ([&] (Project& p) {
            vintage.setSelectedId (p.vintageMode + 1, juce::dontSendNotification);
            revSize.setValue (p.fx.reverbSize * 100.0, juce::dontSendNotification);
            revDamp.setValue (p.fx.reverbDamping * 100.0, juce::dontSendNotification);
            revReturn.setValue (p.fx.reverbReturnDb, juce::dontSendNotification);
            dlyTime.setSelectedId (p.fx.delayDivision + 1, juce::dontSendNotification);
            dlyFeedback.setValue (p.fx.delayFeedback * 100.0, juce::dontSendNotification);
            dlyReturn.setValue (p.fx.delayReturnDb, juce::dontSendNotification);
            masterLevel.setValue (p.masterLevelDb, juce::dontSendNotification);
        });
        nvType.setSelectedId (juce::jlimit (1, 4, (int) s.noteVariationType), juce::dontSendNotification);
        nvTarget.setSelectedId (s.noteVariationPad < 0 ? 2 : 1, juce::dontSendNotification);
        nvOn.setToggleState (s.noteVariationOn, juce::dontSendNotification);
        sixteenParam.setSelectedId (s.sixteenLevelsParam + 1, juce::dontSendNotification);
        metronome.setToggleState (s.metronome, juce::dontSendNotification);
        clickRecOnly.setToggleState (s.metronomeOnlyWhenRecording, juce::dontSendNotification);
        countIn.setToggleState (s.countIn, juce::dontSendNotification);
        autoOverdub.setToggleState (s.autoOverdub, juce::dontSendNotification);
        tcStrength.setValue (s.tcStrength * 100.0, juce::dontSendNotification);
        midiClock.setToggleState (ctx.engine.getSendMidiClock(), juce::dontSendNotification);
        updateMidiInfo();
        shift.setValue (s.shiftTicks, juce::dontSendNotification);
        if (nvTarget.getSelectedId() == 1)
            applyTarget(); // the selected pad changed
    }

    void refresh() override
    {
        // controllers can be plugged in while this screen is open
        if (++refreshTick % 20 == 0)
            updateMidiInfo();
    }

private:
    void updateMidiInfo()
    {
        const auto inputs = ctx.engine.getActiveMidiInputNames();
        const auto text = "MIDI IN: " + (inputs.isEmpty() ? juce::String ("no controllers connected") : inputs.joinIntoString (", "))
                          + ".  Notes 36-51 play pads 1-16 of the current bank, aftertouch = pressure, CC 70-73 = Q-Links, "
                            "CC 74 = note variation. Plug controllers in any time; DISK > Audio/MIDI setup turns them on/off.";
        if (midiInfo.getText() != text)
            midiInfo.setText (text, juce::dontSendNotification);
    }

    int refreshTick = 0;
    template <typename Fn>
    void fx (Fn&& fn)
    {
        if (! loading)
            ctx.engine.withProject ([&] (Project& p) { fn (p.fx); });
    }

    void applyTarget()
    {
        const int pad = nvTarget.getSelectedId() == 2 ? -1 : ctx.selectedPad;
        ctx.engine.updateSettings ([pad] (PerformanceSettings& s) { s.noteVariationPad = pad; });
    }

    bool loading = false;
    juce::Label secSound { {}, "SOUND" }, secVar { {}, "NOTE VARIATION / 16 LEVELS" }, secTiming { {}, "TIMING / METRONOME" }, midiInfo;
    Caption capVintage { "Converter character" }, capMaster { "Master level" }, capNvType { "Slider controls" },
        capNvTarget { "Applies to" }, capSixteen { "16 levels by" }, capTcStrength { "Timing correct strength" };
    juce::ComboBox vintage, nvType, nvTarget, sixteenParam;
    juce::Slider masterLevel, tcStrength, shift, revSize, revDamp, revReturn, dlyFeedback, dlyReturn;
    juce::ComboBox dlyTime;
    juce::Label secFx { {}, "EFFECTS (pad sends in PROGRAM)" };
    Caption capRevSize { "Reverb size" }, capRevDamp { "Damping" }, capRevReturn { "Reverb level" }, capDlyTime { "Delay time" },
        capDlyFeedback { "Feedback" }, capDlyReturn { "Delay level" };
    Caption capShift { "Timing shift" };
    juce::ToggleButton nvOn, metronome, clickRecOnly, countIn, autoOverdub, midiClock;
};
} // namespace

std::unique_ptr<Screen> createSongScreen (AppContext& c) { return std::make_unique<SongScreen> (c); }
std::unique_ptr<Screen> createStepEditScreen (AppContext& c) { return std::make_unique<StepEditScreen> (c); }
std::unique_ptr<Screen> createOtherScreen (AppContext& c) { return std::make_unique<OtherScreen> (c); }

void stepEditStep (Screen& s, int dir)
{
    if (auto* se = dynamic_cast<StepEditScreen*> (&s))
        se->stepTo (dir);
}

} // namespace padlab
