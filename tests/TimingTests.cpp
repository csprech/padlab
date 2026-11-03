#include "core/Timing.h"
#include <juce_core/juce_core.h>

using namespace padlab::timing;

struct TimingTests : juce::UnitTest
{
    TimingTests() : juce::UnitTest ("Timing / swing", "padlab") {}

    void runTest() override
    {
        beginTest ("50% swing is straight 16ths");
        expectEquals ((int) swungOffset (240, true, 50.0, false), 240);
        expectEquals ((int) quantize (250, 240, true, 50.0), 240);
        expectEquals ((int) quantize (100, 240, true, 50.0), 0);
        expectEquals ((int) quantize (130, 240, true, 50.0), 240);

        beginTest ("66% swing puts the off 16th at two thirds of the 8th (triplet feel)");
        expectEquals ((int) swungOffset (240, true, 66.0, false), 317); // 0.66 * 480 = 316.8
        expectEquals ((int) quantize (300, 240, true, 66.0), 317);
        expectEquals ((int) quantize (480 + 330, 240, true, 66.0), 480 + 317);
        expectEquals ((int) quantize (470, 240, true, 66.0), 480);

        beginTest ("swing only applies to 1/8 and 1/16");
        expectEquals ((int) swungOffset (120, false, 66.0, false), 120);
        expectEquals ((int) quantize (130, 120, false, 66.0), 120);

        beginTest ("legacy 96 PPQN grid rounds the swung offset to 10-tick steps");
        expectEquals ((int) swungOffset (240, true, 66.0, true), 320);
        expectEquals ((int) swungOffset (240, true, 54.0, true), 260); // 259.2 -> 260
        expectEquals ((int) swungOffset (240, true, 54.0, false), 259);

        beginTest ("strength moves partway toward the grid");
        expectEquals ((int) quantize (200, 240, true, 50.0, 0.5), 220);
        expectEquals ((int) quantize (200, 240, true, 50.0, 0.0), 200);

        beginTest ("swing is clamped to 50..75");
        expectEquals ((int) swungOffset (240, true, 90.0, false), 360);
        expectEquals ((int) swungOffset (240, true, 10.0, false), 240);

        beginTest ("next grid point");
        expectEquals ((int) nextGridPoint (0, 240, true, 60.0), 0);
        expectEquals ((int) nextGridPoint (1, 240, true, 60.0), 288);
        expectEquals ((int) nextGridPoint (289, 240, true, 60.0), 480);
        expectEquals ((int) nextGridPoint (481, 320, false, 60.0), 640);

        beginTest ("negative ticks (count-in) floor correctly");
        expectEquals ((int) floorDiv (-1, 480), -1);
        expectEquals ((int) quantize (-10, 240, true, 50.0), 0);
    }
};

static TimingTests timingTests;
