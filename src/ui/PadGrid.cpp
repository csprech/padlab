#include "PadGrid.h"
#include "../core/SampleOps.h"
#include "Screens.h"
#include "Materials.h"

namespace padlab
{
PadGrid::PadGrid (AppContext& c) : ctx (c) { startTimerHz (40); }

juce::Rectangle<float> PadGrid::padBounds (int i) const
{
    auto area = getLocalBounds().toFloat().reduced (kMargin);
    const float size = juce::jmin ((area.getWidth() - kGap * 3) / 4.0f, (area.getHeight() - kGap * 3 - kLabelH * 4) / 4.0f);
    const float ox = area.getX() + (area.getWidth() - (size * 4 + kGap * 3)) * 0.5f;
    const float oy = area.getY() + (area.getHeight() - ((size + kLabelH) * 4 + kGap * 3)) * 0.5f;
    const int col = i % 4, row = 3 - i / 4; // pad 1 bottom-left
    return { ox + col * (size + kGap), oy + kLabelH + row * (size + kLabelH + kGap), size, size }; // label printed above
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
            if (pad < kNumTracks) // banks A-D = tracks 1-16 / 17-32 / 33-48 / 49-64
            {
                ctx.engine.withProject ([&] (Project& p) {
                    auto& t = p.sequences[(size_t) ctx.engine.getCurrentSequence()].tracks[(size_t) pad];
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
    pressedAbs[(size_t) i] = pad;
    if (ctx.onPadHit)
        ctx.onPadHit (pad, velocity);
    if (! ctx.engine.getSettings().sixteenLevels)
        ctx.setSelectedPad (pad);
    flashPad (i, velocity);
}

void PadGrid::release (int i)
{
    // release the pad that was actually pressed, even if the bank changed meanwhile
    if (pressedAbs[(size_t) i] >= 0)
        ctx.engine.padUp (pressedAbs[(size_t) i]);
    pressedAbs[(size_t) i] = -1;
}

void PadGrid::flashPad (int i, int velocity)
{
    glow[(size_t) i] = juce::jmax (glow[(size_t) i], velocity / 127.0f);
    repaint (padBounds (i).withTop (padBounds (i).getY() - kLabelH).expanded (8.0f).toNearestInt());
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
    if (mousePad >= 0 && pressedAbs[(size_t) mousePad] >= 0)
        ctx.engine.padPressure (pressedAbs[(size_t) mousePad], velocityFor (mousePad, e.getPosition()));
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
    std::array<bool, kPadsPerBank> padDirty {};
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
            glow[(size_t) i] = juce::jmax (0.0f, glow[(size_t) i] - 0.12f);
            padDirty[(size_t) i] = true;
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
            const int trackIdx = bank * kPadsPerBank + i;
            const bool tm = trackIdx < kNumTracks && seq.tracks[(size_t) trackIdx].mute;
            if (tm != trackMuted[(size_t) i])
                dirty = true;
            trackMuted[(size_t) i] = tm;
        }
    });
    if (dirty)
        repaint();
    else
        for (int i = 0; i < kPadsPerBank; ++i)
            if (padDirty[(size_t) i] || lastPainted[(size_t) i] != bank * kPadsPerBank + i)
            {
                // pad + its label strip only
                repaint (padBounds (i).withTop (padBounds (i).getY() - kLabelH).expanded (8.0f).toNearestInt());
                lastPainted[(size_t) i] = bank * kPadsPerBank + i;
            }
}

void PadGrid::resized() {}

void PadGrid::paint (juce::Graphics& g)
{
    using namespace theme;
    const int bank = ctx.engine.getCurrentBank();
    const int selected = ctx.selectedPad;
    const auto view = ctx.padView;

    const auto clip = g.getClipBounds().toFloat();
    for (int i = 0; i < kPadsPerBank; ++i)
    {
        const auto r = padBounds (i);
        if (! clip.intersects (r.withTop (r.getY() - kLabelH).expanded (10.0f)))
            continue;
        const int pad = bank * kPadsPerBank + i;
        const float hit = glow[(size_t) i];

        // pads stand a little proud of the plate through square cut-outs: a thin dark gap around each
        auto gap = r.expanded (2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.drawRoundedRectangle (gap.translated (0, 1.0f), 5.0f, 1.0f);
        g.setColour (juce::Colour (0xff1d1f21));
        g.fillRoundedRectangle (gap, 5.0f);

        // the rubber gives a little when struck
        const float press = juce::jmin (1.0f, hit * 1.25f);
        auto p = r.translated (0, press * 1.0f);
        if (press < 0.5f)
            juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 4, { 1, 2 }).drawForRectangle (g, p.toNearestInt().reduced (2));

        const float sc = materials::scaleOf (g);
        const int px = (int) std::ceil (r.getWidth() * sc);
        // the bottom row (kick, snare, hats) gets played the most, so it's the most worn
        static constexpr float kWear[kPadsPerBank] = { 0.95f, 0.85f, 0.7f, 0.45f, 0.4f, 0.35f, 0.25f, 0.2f,
                                                       0.2f,  0.15f, 0.12f, 0.1f, 0.12f, 0.1f, 0.08f, 0.15f };
        const auto& padImg = materials::cached ("pad:" + juce::String (px) + ":" + juce::String (i), [=] {
            auto sh = materials::rubber();
            sh.bump = 0.12f;
            sh.bumpScale = 0.5f;
            sh.grain = 0.025f;
            sh.specular = 0.06f;
            sh.ambient = 0.40f;
            sh.diffuse = 0.66f;
            return materials::pillow (px, px, px * 0.035f, px * 0.045f, px * 0.006f, colours::rubber, sh, 23u + (uint32_t) i, kWear[i]);
        });
        materials::drawAt (g, padImg, p);
        if (press > 0.0f)
        {
            // compressed rubber catches less light
            g.setColour (juce::Colours::black.withAlpha (0.14f * press));
            g.fillRoundedRectangle (p, px / sc * 0.035f);
        }

        if (dragHover == i)
        {
            g.setColour (colours::ledAmber.withAlpha (0.9f));
            g.drawRoundedRectangle (r.expanded (2.0f), 8.0f, 2.5f);
        }

        // printed label + status LED under the pad
        auto label = juce::Rectangle<float> (r.getX(), r.getY() - kLabelH + 1.0f, r.getWidth(), kLabelH - 5.0f);
        juce::String top = AppContext::padName (pad), sub = names[(size_t) i].toUpperCase();
        juce::Colour ledColour = colours::ledRed;
        float led = juce::jmax (hit, pad == selected ? 0.45f : 0.0f);

        if (view == PadView::TrackMute)
        {
            ledColour = colours::ledGreen;
            if (pad < kNumTracks)
            {
                top = "TRACK " + juce::String (pad + 1);
                sub = trackMuted[(size_t) i] ? "MUTED" : "ON";
                led = trackMuted[(size_t) i] ? 0.0f : 1.0f;
            }
            else
            {
                top = {};
                sub = {};
                led = 0.0f;
            }
        }
        else if (view == PadView::PadMute)
        {
            ledColour = colours::ledGreen;
            led = hasSample[(size_t) i] && ! padMuted[(size_t) i] ? 1.0f : 0.0f;
            if (padMuted[(size_t) i])
                sub = "MUTED";
        }

        const float d = 6.0f;
        drawLed (g, { label.getX() + 1.0f, label.getCentreY() - d * 0.5f, d, d }, ledColour, led);
        auto textArea = label.withTrimmedLeft (d + 6.0f).toNearestInt();
        g.setColour (colours::silk);
        g.setFont (labelFont (12.0f));
        const int topW = (int) std::ceil (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), top)) + 6;
        g.drawText (top, textArea.removeFromLeft (topW), juce::Justification::centredLeft);
        g.setColour (colours::silk.withAlpha (0.7f));
        g.setFont (labelFont (10.5f));
        g.drawFittedText (sub, textArea, juce::Justification::centredLeft, 1, 0.8f);
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
    int index = start, loaded = 0, converted = 0;
    juce::String lastName;
    juce::StringArray failed;
    for (auto& f : files)
    {
        if (index >= kPadsPerBank)
            break;
        juce::File file (f);
        if (! sampleops::isSupportedAudioFile (file))
            continue;
        auto s = sampleops::loadFile (file);
        if (s == nullptr)
        {
            failed.add (file.getFileName());
            continue;
        }
        if (sampleops::isCompressedAudioFile (file))
            ++converted;
        lastName = file.getFileName();
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
        {
            const auto what = loaded == 1 ? lastName : juce::String (loaded) + " samples";
            ctx.flashMessage ((converted > 0 ? "Converted " + what + " to WAV" : "Loaded " + what)
                              + " on " + AppContext::padName (bank * kPadsPerBank + start));
        }
    }
    else if (! failed.isEmpty() && ctx.flashMessage)
        ctx.flashMessage ("Couldn't read " + failed[0]);
    repaint();
}

} // namespace padlab
