#include "ui/PadGrid.h"
#include "core/SampleOps.h"
#include "ui/Screens.h"

namespace padlab
{
PadGrid::PadGrid (AppContext& c) : ctx (c) { startTimerHz (40); }

juce::Rectangle<float> PadGrid::padBounds (int i) const
{
    const float gap = 10.0f;
    const float size = juce::jmin ((getWidth() - gap * 3) / 4.0f, (getHeight() - gap * 5) / 4.0f);
    const float ox = (getWidth() - (size * 4 + gap * 3)) * 0.5f;
    const float oy = (getHeight() - (size * 4 + gap * 3)) * 0.5f;
    const int col = i % 4, row = 3 - i / 4; // pad 1 bottom-left
    return { ox + col * (size + gap), oy + row * (size + gap), size, size };
}

int PadGrid::padAt (juce::Point<int> p) const
{
    for (int i = 0; i < kPadsPerBank; ++i)
        if (padBounds (i).contains (p.toFloat()))
            return i;
    return -1;
}

int PadGrid::velocityFor (int i, juce::Point<int> p) const
{
    // vertical position inside the pad = how hard you hit (top = hardest)
    auto b = padBounds (i);
    const float y = juce::jlimit (0.0f, 1.0f, (p.y - b.getY()) / b.getHeight());
    return juce::jlimit (1, 127, (int) std::round (127.0f - y * 100.0f));
}

void PadGrid::press (int i, int velocity)
{
    const int bank = ctx.engine.getCurrentBank();
    const int pad = bank * kPadsPerBank + i;
    switch (ctx.padView)
    {
        case PadView::TrackMute:
            if (i < kNumTracks)
            {
                ctx.engine.withProject ([&] (Project& p) {
                    auto& t = p.sequences[(size_t) ctx.engine.getCurrentSequence()].tracks[(size_t) i];
                    t.mute = ! t.mute;
                });
                ctx.changed();
            }
            return;
        case PadView::PadMute:
        {
            const int prog = ctx.currentProgram();
            ctx.engine.withProject ([&] (Project& p) {
                auto& pd = p.programs[(size_t) prog].pads[(size_t) pad];
                pd.mute = ! pd.mute;
            });
            ctx.changed();
            return;
        }
        case PadView::Play:
            break;
    }
    ctx.engine.padDown (pad, velocity);
    if (! ctx.engine.getSettings().sixteenLevels)
        ctx.setSelectedPad (pad);
    flashPad (i, velocity);
}

void PadGrid::release (int i)
{
    if (ctx.padView == PadView::Play)
        ctx.engine.padUp (ctx.engine.getCurrentBank() * kPadsPerBank + i);
}

void PadGrid::flashPad (int i, int velocity)
{
    glow[(size_t) i] = juce::jmax (glow[(size_t) i], velocity / 127.0f);
    repaint();
}

void PadGrid::mouseDown (const juce::MouseEvent& e)
{
    mousePad = padAt (e.getPosition());
    if (mousePad >= 0)
        press (mousePad, e.mods.isShiftDown() ? 127 : velocityFor (mousePad, e.getPosition()));
}

void PadGrid::mouseDrag (const juce::MouseEvent& e)
{
    // dragging while held = aftertouch / pressure (drives note repeat velocity)
    if (mousePad >= 0 && ctx.padView == PadView::Play)
        ctx.engine.padPressure (ctx.engine.getCurrentBank() * kPadsPerBank + mousePad, velocityFor (mousePad, e.getPosition()));
}

void PadGrid::mouseUp (const juce::MouseEvent&)
{
    if (mousePad >= 0)
        release (mousePad);
    mousePad = -1;
}

void PadGrid::timerCallback()
{
    bool dirty = false;
    const int bank = ctx.engine.getCurrentBank();
    const int prog = ctx.currentProgram();

    for (int i = 0; i < kPadsPerBank; ++i)
    {
        const int pad = bank * kPadsPerBank + i;
        const auto hits = ctx.engine.getPadHitCount (pad);
        if (hits != lastHits[(size_t) pad])
        {
            lastHits[(size_t) pad] = hits;
            glow[(size_t) i] = juce::jmax (glow[(size_t) i], ctx.engine.getPadHitVelocity (pad) / 127.0f);
        }
        if (glow[(size_t) i] > 0.0f)
        {
            glow[(size_t) i] = juce::jmax (0.0f, glow[(size_t) i] - 0.08f);
            dirty = true;
        }
    }

    ctx.engine.withProject ([&] (Project& p) {
        const auto& seq = p.sequences[(size_t) ctx.engine.getCurrentSequence()];
        for (int i = 0; i < kPadsPerBank; ++i)
        {
            const auto& pd = p.programs[(size_t) prog].pads[(size_t) (bank * kPadsPerBank + i)];
            juce::String n;
            for (auto& l : pd.layers)
                if (l.sample != nullptr)
                {
                    n = describeLayer (l);
                    break;
                }
            if (n != names[(size_t) i] || pd.hasSample() != hasSample[(size_t) i] || pd.mute != padMuted[(size_t) i])
                dirty = true;
            names[(size_t) i] = n;
            hasSample[(size_t) i] = pd.hasSample();
            padMuted[(size_t) i] = pd.mute;
            const bool tm = i < kNumTracks && seq.tracks[(size_t) i].mute;
            if (tm != trackMuted[(size_t) i])
                dirty = true;
            trackMuted[(size_t) i] = tm;
        }
    });
    if (dirty)
        repaint();
}

void PadGrid::resized() {}

void PadGrid::paint (juce::Graphics& g)
{
    using namespace theme;
    const int bank = ctx.engine.getCurrentBank();
    const int selected = ctx.selectedPad;
    const auto view = ctx.padView;

    for (int i = 0; i < kPadsPerBank; ++i)
    {
        auto r = padBounds (i);
        const int pad = bank * kPadsPerBank + i;
        const float gl = glow[(size_t) i];

        // well + rubber pad
        g.setColour (colours::bodyEdge);
        g.fillRoundedRectangle (r.expanded (3.0f), 9.0f);

        juce::Colour base = colours::pad;
        juce::String label = AppContext::padName (pad);
        juce::String sub = names[(size_t) i];
        juce::Colour ring = juce::Colours::transparentBlack;

        if (view == PadView::TrackMute)
        {
            if (i < kNumTracks)
            {
                label = "TRACK " + juce::String (i + 1);
                sub = trackMuted[(size_t) i] ? "MUTED" : "ON";
                ring = trackMuted[(size_t) i] ? colours::textDim.withAlpha (0.4f) : colours::play;
            }
            else
            {
                label = {};
                sub = {};
            }
        }
        else if (view == PadView::PadMute)
        {
            sub = padMuted[(size_t) i] ? "MUTED" : sub;
            ring = hasSample[(size_t) i] ? (padMuted[(size_t) i] ? colours::textDim.withAlpha (0.4f) : colours::accent2) : ring;
        }
        else if (hasSample[(size_t) i])
        {
            ring = colours::accent.withAlpha (0.25f);
        }

        if (gl > 0.0f)
            base = base.interpolatedWith (padGlow (gl), juce::jmin (1.0f, gl * 1.1f));

        g.setGradientFill (juce::ColourGradient (base.brighter (0.12f), r.getX(), r.getY(), base.darker (0.2f),
                                                 r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (colours::padEdge.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);

        if (! ring.isTransparent())
        {
            g.setColour (ring);
            g.drawRoundedRectangle (r.reduced (3.0f), 5.0f, 2.0f);
        }
        if (pad == selected && view == PadView::Play)
        {
            g.setColour (colours::text.withAlpha (0.85f));
            g.drawRoundedRectangle (r.reduced (1.5f), 7.0f, 2.0f);
        }
        if (dragHover == i)
        {
            g.setColour (colours::accent2);
            g.drawRoundedRectangle (r.reduced (1.0f), 7.0f, 3.0f);
        }

        g.setColour (gl > 0.3f ? juce::Colours::black.withAlpha (0.8f) : colours::text.withAlpha (0.75f));
        g.setFont (labelFont (12.0f, true));
        g.drawText (label, r.reduced (8.0f, 6.0f).toNearestInt(), juce::Justification::topLeft);
        g.setFont (labelFont (11.0f));
        g.setColour (gl > 0.3f ? juce::Colours::black.withAlpha (0.7f) : colours::textDim);
        g.drawFittedText (sub, r.reduced (8.0f, 6.0f).toNearestInt(), juce::Justification::bottomLeft, 2);
    }
}

// ---------------------------------------------------------------- drag & drop samples onto pads

bool PadGrid::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (sampleops::isSupportedAudioFile (juce::File (f)))
            return true;
    return false;
}

void PadGrid::fileDragEnter (const juce::StringArray&, int x, int y) { fileDragMove ({}, x, y); }

void PadGrid::fileDragMove (const juce::StringArray&, int x, int y)
{
    const int p = padAt ({ x, y });
    if (p != dragHover)
    {
        dragHover = p;
        repaint();
    }
}

void PadGrid::fileDragExit (const juce::StringArray&)
{
    dragHover = -1;
    repaint();
}

void PadGrid::filesDropped (const juce::StringArray& files, int x, int y)
{
    dragHover = -1;
    int start = padAt ({ x, y });
    if (start < 0)
        start = 0;

    const int bank = ctx.engine.getCurrentBank();
    const int prog = ctx.currentProgram();
    int index = start, loaded = 0;
    for (auto& f : files)
    {
        if (index >= kPadsPerBank)
            break;
        juce::File file (f);
        if (! sampleops::isSupportedAudioFile (file))
            continue;
        auto s = sampleops::loadFile (file);
        if (s == nullptr)
            continue;
        const int pad = bank * kPadsPerBank + index;
        SamplePtr old;
        ctx.engine.withProject ([&] (Project& p) {
            s->name = p.makeUniqueSampleName (s->name);
            p.addSample (s);
            auto& layer = p.programs[(size_t) prog].pads[(size_t) pad].layers[0];
            old = layer.sample;
            layer.sample = s;
            layer.sliceStart = 0;
            layer.sliceEnd = -1;
        });
        ctx.engine.retire (old);
        ++index;
        ++loaded;
    }
    if (loaded > 0)
    {
        ctx.setSelectedPad (bank * kPadsPerBank + start);
        ctx.changed();
        if (ctx.flashMessage)
            ctx.flashMessage ("Loaded " + juce::String (loaded) + " sample" + (loaded > 1 ? "s" : ""));
    }
    repaint();
}

} // namespace padlab
