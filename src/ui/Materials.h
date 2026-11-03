#pragma once

// Tiny offline "renderer" for photoreal hardware parts.
//
// Each part is described as a height field. We take its normals and shade them
// with a single overhead key light (top-left, like a studio product shot):
// ambient + Lambert diffuse + Blinn-Phong specular, plus micro-bump grain for
// rubber/paint. Results are cached ARGB images rendered at device-pixel
// resolution, so painting stays cheap.

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace padlab::materials
{
struct Shading
{
    float ambient = 0.35f;
    float diffuse = 0.75f;
    float specular = 0.2f;
    float shininess = 20.0f;
    float grain = 0.06f;   // albedo noise
    float bump = 0.0f;     // micro-bump height (px)
    float bumpScale = 1.0f;
};

Shading rubber();
Shading plastic();
Shading metal();

/** Device pixel scale of the graphics context (2 on Retina). */
float scaleOf (juce::Graphics& g);

/** Cache by key: images are generated once and reused. */
const juce::Image& cached (const juce::String& key, const std::function<juce::Image()>& make);

/** A rounded "pillow" (pad, key cap, badge): rounded-rect footprint, round-over bevel, slight dome. */
juce::Image pillow (int w, int h, float corner, float bevel, float dome, juce::Colour albedo, const Shading& s,
                    uint32_t seed = 1);

/** A domed LED lens in a black bezel. */
juce::Image led (int diameter, juce::Colour colour, bool on);

/** A Phillips-head screw. */
juce::Image screw (int diameter, float angle);

/** A knurled black knob with a white indicator; lighting stays fixed while it turns. */
juce::Image knob (int diameter, float angle);

/** Tileable powder-coat paint texture (opaque). */
juce::Image paintTile (juce::Colour base, int size, float roughness = 1.0f, uint32_t seed = 7);

/** Draws an image rendered at device resolution into a logical rectangle. */
void drawAt (juce::Graphics& g, const juce::Image& img, juce::Rectangle<float> dest, float opacity = 1.0f);

/** Soft, round contact shadow under an object. */
void contactShadow (juce::Graphics& g, juce::Rectangle<float> footprint, float softness, float offsetY, float strength);

} // namespace padlab::materials
