// Padlab's side of the GPUI bridge: its panel's parts (every part of the front panel, in its stacking order), and
// what makes the custom ones look different.

#include "PanelBridge.h"
#include "../src/engine/Engine.h"
#include "../src/ui/MainComponent.h"
#include "../src/plugin/PadlabProcessor.h"

namespace padlab
{
using panelbridge::mix;
using panelbridge::mixFloat;
using panelbridge::mixString;

struct GpuiAdapter : panelbridge::Adapter
{
    std::unique_ptr<Engine> engine = std::make_unique<Engine>();
    std::unique_ptr<MainComponent> mc = std::make_unique<MainComponent> (*engine);

    GpuiAdapter() { mc->setSize (PadlabProcessor::kPanelWidth, PadlabProcessor::kPanelHeight); }
    ~GpuiAdapter() override
    {
        MainComponent::keyHook = nullptr;
        mc.reset();
        engine.reset();
    }

    juce::String title() override { return "Padlab"; }
    juce::Component& panel() override { return *mc; }
    void setKeyHook (std::function<bool (int)> f) override { MainComponent::keyHook = std::move (f); }
    float outputLevel() override { return std::max (engine->getMasterPeak (0), engine->getMasterPeak (1)); }
    void testSound (bool start) override
    {
        if (start)
            engine->padDown (0, 120);
        else
            engine->padUp (0);
    }

    std::vector<std::pair<juce::String, juce::Component*>> parts() override
    {
        std::vector<std::pair<juce::String, juce::Component*>> p;
        for (auto* c : mc->getChildren())
        {
            juce::String kind = "static";
            if (dynamic_cast<theme::HardButton*> (c) != nullptr)
                kind = "key";
            else if (dynamic_cast<juce::Slider*> (c) != nullptr || dynamic_cast<DataWheel*> (c) != nullptr)
                kind = "drag";
            else if (dynamic_cast<Screen*> (c) != nullptr || dynamic_cast<PadGrid*> (c) != nullptr || c == &mc->playhead)
                kind = "display";
            p.push_back ({ kind, c });
        }
        return p;
    }

    bool live (juce::Component& c) override { return dynamic_cast<Screen*> (&c) != nullptr; }

    uint64_t custom (juce::Component& c) override
    {
        uint64_t h = 7;
        if (auto* b = dynamic_cast<theme::HardButton*> (&c))
            return mix (mix (h, b->isLit()), b->isBlinking() ? (juce::Time::getMillisecondCounter() / 250) % 2 + 2 : 0);
        if (auto* g = dynamic_cast<PadGrid*> (&c))
        {
            for (int i = 0; i < kPadsPerBank; ++i)
            {
                h = mixFloat (h, g->glow[(size_t) i], 0.02f);
                h = mixString (h, g->names[(size_t) i]);
                h = mix (mix (mix (mix (h, g->hasSample[(size_t) i]), g->padMuted[(size_t) i]), g->trackMuted[(size_t) i]), (uint64_t) (g->pressedAbs[(size_t) i] + 1));
            }
            return mix (mix (h, (uint64_t) (g->mousePad + 1)), (uint64_t) (g->dragHover + 1));
        }
        if (auto* w = dynamic_cast<DataWheel*> (&c))
            return mixFloat (h, w->angle);
        if (auto* cp = dynamic_cast<CursorPad*> (&c))
            return mix (h, (uint64_t) (cp->pressed + 1));
        if (auto* t = dynamic_cast<SoftKeyTabs*> (&c))
        {
            h = mix (h, (uint64_t) (t->active + 1));
            for (auto& l : t->labels)
                h = mixString (h, l);
            for (int x : t->centres)
                h = mix (h, (uint64_t) x);
            return h;
        }
        if (auto* f = dynamic_cast<FloppyDrive*> (&c))
            return mix (h, f->busy);
        if (auto* b = dynamic_cast<BankLeds*> (&c))
            return mix (h, (uint64_t) b->bank);
        if (&c == &mc->playhead)
        {
            const auto& st = mc->playhead.state;
            auto rect = [&h] (juce::Rectangle<int> r) { h = mix (mix (mix (mix (h, (uint64_t) r.getX()), (uint64_t) r.getY()), (uint64_t) r.getWidth()), (uint64_t) r.getHeight()); };
            rect (st.playhead);
            rect (mc->playhead.cursor);
            for (auto& m : st.meters)
            {
                rect (m.area);
                h = mixFloat (h, m.level, 0.01f);
            }
            return h;
        }
        return 0;
    }
};
} // namespace padlab

std::unique_ptr<panelbridge::Adapter> panelbridge::makeAdapter() { return std::make_unique<padlab::GpuiAdapter>(); }
