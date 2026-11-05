#pragma once

// Original factory content, synthesized at startup (no third-party samples).

#include "Project.h"

namespace padlab::drumsynth
{
constexpr double kRate = 44100.0;

/** The 16 factory sounds in pad order (pad 1 = bottom-left). */
std::vector<SamplePtr> makeFactoryKit();

/** A 2-bar break rendered from the kit, for chopping practice. */
SamplePtr makeDemoBreak (const std::vector<SamplePtr>& kit, double bpm);

/** Fills project with the factory kit on program 1 and a demo groove on sequence 1. */
void loadFactoryProject (Project& project);

} // namespace padlab::drumsynth
