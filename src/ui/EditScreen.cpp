// EDIT: sequence editing - loop, bars (insert / delete / copy), copy sequence / track, and
// edits on a range of events (velocity, move, shift pads, length, delete).

#include "Screens.h"
#include "../core/SeqEdit.h"

namespace padlab
{
namespace
{
void place (juce::Rectangle<int>& row, Caption& cap, juce::Component& c, int w)
{
    auto cell = row.removeFromLeft (w);
    row.removeFromLeft (8);
    cap.setBounds (cell.removeFromTop (14));
    c.setBounds (cell.withHeight (24));
}

juce::StringArray sequenceNames (Project& p)
{
    juce::StringArray a;
    for (int i = 0; i < kNumSequences; ++i)
        a.add (juce::String (i + 1).paddedLeft ('0', 2) + "  " + p.sequences[(size_t) i].name);
    return a;
}

class EditScreen : public Screen
{
public:
    explicit EditScreen (AppContext& c) : Screen (c)
    {
        for (auto* l : { &secSeq, &secBars, &secCopy, &secEvents })
        {
            l->setFont (theme::screenFont (14.0f, true));
            l->setColour (juce::Label::textColourId, theme::lcd::ink);
            addAndMakeVisible (l);
        }

        // ---- loop
        loop.setButtonText ("Loop");
        loop.onClick = [this] { editSeq ([v = loop.getToggleState()] (Sequence& s) { s.loop = v; }, "Loop " + juce::String (loop.getToggleState() ? "on" : "off")); };
        setupParam (loopTo, 1, 2, 1);
        loopTo.textFromValueFunction = [] (double v) { return "bar " + juce::String ((int) v); };
        loopTo.onValueChange = [this] {
            if (! loading)
                editSeq ([v = (int) loopTo.getValue() - 1] (Sequence& s) { s.loopToBar = juce::jlimit (0, s.bars - 1, v); }, {});
        };

        // ---- bars
        setupCombo (barsOp, { "Insert blank bars", "Delete bars", "Copy bars" });
        barsOp.setSelectedId (1, juce::dontSendNotification);
        barsOp.onChange = [this] { updateEnables(); };
        for (auto* sl : { &barFrom, &barTo, &barBefore })
        {
            setupParam (*sl, 1, 2, 1);
            sl->textFromValueFunction = [] (double v) { return "bar " + juce::String ((int) v); };
        }
        setupParam (barCount, 1, 64, 1);
        barsDo.onClick = [this] { doBars(); };

        // ---- copy sequence / track
        copySeqDo.onClick = [this] { doCopySequence(); };
        setupParam (toTrack, 1, kNumTracks, 1);
        toTrack.textFromValueFunction = [] (double v) { return "track " + juce::String ((int) v); };
        copyTrackDo.onClick = [this] { doCopyTrack(); };

        // ---- events
        setupCombo (evTrack, { "Current track", "All tracks" });
        evTrack.setSelectedId (1, juce::dontSendNotification);
        for (auto* sl : { &evFrom, &evTo })
        {
            setupParam (*sl, 1, 2, 1);
            sl->textFromValueFunction = [] (double v) { return "bar " + juce::String ((int) v); };
        }
        for (auto* sl : { &padLo, &padHi })
        {
            setupParam (*sl, 0, kNumPads - 1, 1);
            sl->textFromValueFunction = [] (double v) { return AppContext::padName ((int) v); };
        }
        padHi.setValue (kNumPads - 1, juce::dontSendNotification);
        setupCombo (evOp, { "Velocity +/-", "Velocity %", "Set velocity", "Move (ticks)", "Shift pads / notes", "Set length (ticks)", "Delete" });
        evOp.setSelectedId (2, juce::dontSendNotification);
        evOp.onChange = [this] { updateAmountRange(); };
        setupParam (amount, -127, 127, 1);
        evDo.onClick = [this] { doEvents(); };
        updateAmountRange();

        for (auto* comp : std::initializer_list<juce::Component*> {
                 &capLoopTo, &loop, &loopTo, &info, &capBarsOp, &barsOp, &capFrom, &barFrom, &capTo, &barTo, &capCount, &barCount,
                 &capToSeq, &toSeq, &capBefore, &barBefore, &barsDo, &capCopyTo, &copySeqTo, &copySeqDo, &capTrackSeq,
                 &trackSeqTo, &capToTrack, &toTrack, &copyTrackDo, &capEvTrack, &evTrack, &capEvFrom, &evFrom, &capEvTo, &evTo,
                 &capPadLo, &padLo, &capPadHi, &padHi, &capOp, &evOp, &capAmount, &amount, &evDo, &hint })
            addAndMakeVisible (comp);
        info.setFont (theme::screenFont (14.0f));
        info.setColour (juce::Label::textColourId, theme::lcd::ink);
        hint.setFont (theme::screenFont (12.0f));
        hint.setColour (juce::Label::textColourId, theme::lcd::textDim);
        hint.setText ("UNDO SEQ takes back the last edit of the current sequence. 960 ticks = 1 beat.", juce::dontSendNotification);
        updateEnables();
    }

    juce::String getTitle() const override { return "EDIT SEQUENCE"; }
    juce::String getHelpTopic() const override { return "Editing sequences"; }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        auto section = [&] (juce::Label& l) { l.setBounds (r.removeFromTop (18)); };

        section (secSeq);
        auto row = r.removeFromTop (40);
        loop.setBounds (row.removeFromLeft (90).withTrimmedTop (14).withHeight (24));
        place (row, capLoopTo, loopTo, 120);
        info.setBounds (row.withTrimmedTop (14).withHeight (24));
        r.removeFromTop (6);

        section (secBars);
        row = r.removeFromTop (40);
        place (row, capBarsOp, barsOp, 150);
        place (row, capFrom, barFrom, 84);
        place (row, capTo, barTo, 84);
        place (row, capCount, barCount, 70);
        place (row, capToSeq, toSeq, 150);
        place (row, capBefore, barBefore, 84);
        barsDo.setBounds (row.withTrimmedTop (14).withHeight (24));
        r.removeFromTop (6);

        section (secCopy);
        row = r.removeFromTop (40);
        place (row, capCopyTo, copySeqTo, 170);
        copySeqDo.setBounds (row.removeFromLeft (110).withTrimmedTop (14).withHeight (24));
        row.removeFromLeft (20);
        place (row, capTrackSeq, trackSeqTo, 170);
        place (row, capToTrack, toTrack, 90);
        copyTrackDo.setBounds (row.withTrimmedTop (14).withHeight (24));
        r.removeFromTop (6);

        section (secEvents);
        row = r.removeFromTop (40);
        place (row, capEvTrack, evTrack, 130);
        place (row, capEvFrom, evFrom, 84);
        place (row, capEvTo, evTo, 84);
        place (row, capPadLo, padLo, 70);
        place (row, capPadHi, padHi, 70);
        row = r.removeFromTop (40);
        place (row, capOp, evOp, 170);
        place (row, capAmount, amount, 120);
        evDo.setBounds (row.removeFromLeft (110).withTrimmedTop (14).withHeight (24));
        hint.setBounds (r.removeFromBottom (16));
    }

    void contextChanged() override
    {
        const juce::ScopedValueSetter<bool> svs (loading, true);
        const int seq = ctx.engine.getCurrentSequence();
        const int track = ctx.currentTrack(); // takes the lock itself: never call it inside withProject
        juce::StringArray names;
        int bars = 1;
        ctx.engine.withProject ([&] (Project& p) {
            names = sequenceNames (p);
            const auto& s = p.sequences[(size_t) seq];
            bars = s.bars;
            loop.setToggleState (s.loop, juce::dontSendNotification);
            loopTo.setRange (1, juce::jmax (2, bars), 1);
            loopTo.setValue (s.loopToBar + 1, juce::dontSendNotification);
            info.setText ("SEQ " + juce::String (seq + 1).paddedLeft ('0', 2) + "  " + s.name + "  " + juce::String (bars) + " bars  TRACK "
                              + juce::String (track + 1),
                          juce::dontSendNotification);
        });
        for (auto* cb : { &toSeq, &copySeqTo, &trackSeqTo })
        {
            const int keep = cb->getSelectedId();
            setupCombo (*cb, names);
            cb->setSelectedId (keep > 0 ? keep : seq + 1, juce::dontSendNotification);
        }
        for (auto* sl : { &barFrom, &barTo, &evFrom, &evTo })
            sl->setRange (1, juce::jmax (2, bars), 1);
        barBefore.setRange (1, bars + 1, 1);
        if (barTo.getValue() < barFrom.getValue())
            barTo.setValue (barFrom.getValue(), juce::dontSendNotification);
        if (evTo.getValue() <= 1.0 && bars > 1)
            evTo.setValue (bars, juce::dontSendNotification);
        if (barBefore.getValue() <= 1.0)
            barBefore.setValue (bars + 1, juce::dontSendNotification);
        toTrack.setValue (track + 1, juce::dontSendNotification);
        for (auto* sl : { &loopTo, &barFrom, &barTo, &barBefore, &evFrom, &evTo, &padLo, &padHi, &toTrack })
            sl->updateText();
    }

private:
    template <typename Fn>
    void editSeq (Fn&& fn, const juce::String& message)
    {
        if (loading)
            return;
        const int seq = ctx.engine.getCurrentSequence();
        ctx.engine.withProject ([&] (Project& p) { fn (p.sequences[(size_t) seq]); });
        if (message.isNotEmpty())
            flash (message);
        ctx.changed();
        contextChanged();
    }

    void flash (const juce::String& m)
    {
        if (ctx.flashMessage)
            ctx.flashMessage (m);
    }

    void updateEnables()
    {
        const int op = barsOp.getSelectedId();
        barCount.setEnabled (op != 2);
        barTo.setEnabled (op != 1);
        toSeq.setEnabled (op == 3);
        barBefore.setEnabled (op != 2);
        capFrom.setText (op == 1 ? "BEFORE BAR" : "FROM BAR", juce::dontSendNotification);
        capCount.setText (op == 3 ? "COPIES" : "COUNT", juce::dontSendNotification);
        barBefore.setVisible (op == 3);
        capBefore.setVisible (op == 3);
        toSeq.setVisible (op == 3);
        capToSeq.setVisible (op == 3);
    }

    void updateAmountRange()
    {
        const juce::ScopedValueSetter<bool> svs (loading, true);
        switch (evOp.getSelectedId())
        {
            case 1: amount.setRange (-127, 127, 1); amount.setValue (-10); amount.setTextValueSuffix (""); break;
            case 2: amount.setRange (1, 200, 1); amount.setValue (80); amount.setTextValueSuffix (" %"); break;
            case 3: amount.setRange (1, 127, 1); amount.setValue (100); amount.setTextValueSuffix (""); break;
            case 4: amount.setRange (-3840, 3840, 1); amount.setValue (10); amount.setTextValueSuffix (" t"); break;
            case 5: amount.setRange (-64, 64, 1); amount.setValue (1); amount.setTextValueSuffix (""); break;
            case 6: amount.setRange (1, 3840, 1); amount.setValue (240); amount.setTextValueSuffix (" t"); break;
            default: amount.setRange (0, 1, 1); amount.setValue (0); amount.setTextValueSuffix (""); break;
        }
        amount.setEnabled (evOp.getSelectedId() != 7);
    }

    void doBars()
    {
        const int op = barsOp.getSelectedId();
        const int from = (int) barFrom.getValue() - 1, to = (int) barTo.getValue() - 1, n = (int) barCount.getValue();
        const int seq = ctx.engine.getCurrentSequence();
        ctx.engine.pushUndo (op == 1 ? "insert bars" : op == 2 ? "delete bars" : "copy bars");
        bool ok = false;
        juce::String msg;
        const int dstSeq = toSeq.getSelectedId() - 1, before = (int) barBefore.getValue() - 1;
        ctx.engine.withProject ([&] (Project& p) {
            auto& s = p.sequences[(size_t) seq];
            if (op == 1)
            {
                ok = seqedit::insertBars (s, from, n);
                msg = ok ? juce::String (n) + " bar(s) inserted before bar " + juce::String (from + 1) : "Max " + juce::String (kMaxBars) + " bars";
            }
            else if (op == 2)
            {
                ok = seqedit::deleteBars (s, from, to);
                msg = ok ? "Bars " + juce::String (from + 1) + "-" + juce::String (to + 1) + " deleted" : "A sequence keeps at least one bar";
            }
            else
            {
                ok = seqedit::copyBars (s, from, to, p.sequences[(size_t) dstSeq], before, n);
                msg = ok ? "Bars " + juce::String (from + 1) + "-" + juce::String (to + 1) + " copied to seq " + juce::String (dstSeq + 1)
                         : "Max " + juce::String (kMaxBars) + " bars";
            }
        });
        flash (msg);
        ctx.changed();
        contextChanged();
    }

    void doCopySequence()
    {
        const int seq = ctx.engine.getCurrentSequence(), dst = copySeqTo.getSelectedId() - 1;
        if (dst == seq)
            return flash ("Pick another sequence to copy to");
        ctx.engine.withProject ([&] (Project& p) { seqedit::copySequence (p.sequences[(size_t) seq], p.sequences[(size_t) dst]); });
        flash ("Sequence " + juce::String (seq + 1) + " copied to " + juce::String (dst + 1));
        ctx.changed();
        contextChanged();
    }

    void doCopyTrack()
    {
        const int seq = ctx.engine.getCurrentSequence(), track = ctx.currentTrack();
        const int dstSeq = trackSeqTo.getSelectedId() - 1, dstTrack = (int) toTrack.getValue() - 1;
        if (dstSeq == seq && dstTrack == track)
            return flash ("Pick another track to copy to");
        if (dstSeq == seq)
            ctx.engine.pushUndo ("copy track");
        ctx.engine.withProject ([&] (Project& p) {
            seqedit::copyTrack (p.sequences[(size_t) seq].tracks[(size_t) track], p.sequences[(size_t) dstSeq].tracks[(size_t) dstTrack]);
        });
        flash ("Track " + juce::String (track + 1) + " copied to seq " + juce::String (dstSeq + 1) + " track " + juce::String (dstTrack + 1));
        ctx.changed();
    }

    void doEvents()
    {
        static const seqedit::Op ops[] = { seqedit::Op::VelocityAdd, seqedit::Op::VelocityPercent, seqedit::Op::VelocitySet, seqedit::Op::Move,
                                           seqedit::Op::ShiftPads,   seqedit::Op::LengthSet,       seqedit::Op::Delete };
        const auto op = ops[juce::jlimit (0, 6, evOp.getSelectedId() - 1)];
        const int seq = ctx.engine.getCurrentSequence(), track = ctx.currentTrack();
        const bool all = evTrack.getSelectedId() == 2;
        ctx.engine.pushUndo ("edit events");
        int count = 0;
        ctx.engine.withProject ([&] (Project& p) {
            auto& s = p.sequences[(size_t) seq];
            seqedit::Range range;
            range.fromTick = (int64_t) ((int) evFrom.getValue() - 1) * s.getBarTicks();
            range.toTick = (int64_t) ((int) evTo.getValue()) * s.getBarTicks();
            range.padLo = (int) juce::jmin (padLo.getValue(), padHi.getValue());
            range.padHi = (int) juce::jmax (padLo.getValue(), padHi.getValue());
            for (int t = 0; t < kNumTracks; ++t)
                if (all || t == track)
                    count += seqedit::editEvents (s.tracks[(size_t) t], range, op, (int) amount.getValue(), s.getLengthTicks());
        });
        flash (juce::String (count) + (count == 1 ? " note " : " notes ") + (op == seqedit::Op::Delete ? "deleted" : "edited"));
        ctx.changed();
    }

    bool loading = false;
    juce::Label secSeq { {}, "SEQUENCE" }, secBars { {}, "BARS" }, secCopy { {}, "COPY" }, secEvents { {}, "EVENTS" }, info, hint;
    Caption capLoopTo { "Loop to" }, capBarsOp { "Operation" }, capFrom { "From bar" }, capTo { "To bar" }, capCount { "Count" },
        capToSeq { "To sequence" }, capBefore { "Before bar" }, capCopyTo { "Copy sequence to" }, capTrackSeq { "Copy track to seq" },
        capToTrack { "Track" }, capEvTrack { "Tracks" }, capEvFrom { "From bar" }, capEvTo { "To bar" }, capPadLo { "Pads from" },
        capPadHi { "To" }, capOp { "Operation" }, capAmount { "Amount" };
    juce::ToggleButton loop;
    juce::ComboBox barsOp, toSeq, copySeqTo, trackSeqTo, evTrack, evOp;
    juce::Slider loopTo, barFrom, barTo, barCount, barBefore, toTrack, evFrom, evTo, padLo, padHi, amount;
    juce::TextButton barsDo { "DO IT" }, copySeqDo { "COPY SEQ" }, copyTrackDo { "COPY TRACK" }, evDo { "DO IT" };
};
} // namespace

std::unique_ptr<Screen> createEditScreen (AppContext& c) { return std::make_unique<EditScreen> (c); }

} // namespace padlab
