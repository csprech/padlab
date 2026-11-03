#include "core/DrumSynth.h"
#include "core/SampleOps.h"
#include <cmath>
#include <functional>

namespace padlab::drumsynth
{
namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    struct SVF
    {
        enum Mode { LP, BP, HP };
        SVF (double fc, double q, Mode m) : mode (m)
        {
            const double g = std::tan (juce::MathConstants<double>::pi * std::min (fc, kRate * 0.45) / kRate);
            k = 1.0 / q;
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        double process (double v0)
        {
            const double v3 = v0 - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1;
            ic2 = 2 * v2 - ic2;
            return mode == LP ? v2 : mode == BP ? v1 : v0 - k * v1 - v2;
        }
        Mode mode;
        double k, a1, a2, a3, ic1 = 0, ic2 = 0;
    };

    SamplePtr render (const juce::String& name, double seconds, const std::function<double (int, double)>& fn)
    {
        auto s = std::make_shared<Sample>();
        s->name = name;
        s->sampleRate = kRate;
        const int n = (int) (seconds * kRate);
        s->buffer.setSize (1, n);
        auto* d = s->buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i)
            d[i] = (float) fn (i, i / kRate);
        // short fade-out to avoid clicks
        const int fade = std::min (n, 256);
        for (int i = 0; i < fade; ++i)
            d[n - 1 - i] *= (float) i / (float) fade;
        sampleops::normalize (*s, -1.0f);
        return s;
    }

    double env (double t, double decay) { return std::exp (-t / decay); }

    double softClip (double x) { return std::tanh (x); }

    SamplePtr kick (const juce::String& name, double f0, double f1, double sweep, double decay, double drive)
    {
        double phase = 0;
        return render (name, decay * 5.0, [=] (int, double t) mutable {
            const double f = f1 + (f0 - f1) * std::exp (-t / sweep);
            phase += twoPi * f / kRate;
            const double click = t < 0.004 ? (1.0 - t / 0.004) * 0.6 : 0.0;
            return softClip ((std::sin (phase) * env (t, decay) + click) * drive);
        });
    }

    SamplePtr snare()
    {
        juce::Random rng (11);
        SVF bp (3500.0, 0.7, SVF::BP);
        SVF hp (900.0, 0.7, SVF::HP);
        return render ("Snare", 0.45, [=] (int, double t) mutable {
            const double tone = (std::sin (twoPi * 185.0 * t) * 0.7 + std::sin (twoPi * 330.0 * t) * 0.4) * env (t, 0.06);
            const double noise = hp.process (bp.process (rng.nextDouble() * 2 - 1)) * env (t, 0.13) * 2.2;
            return softClip ((tone + noise) * 1.3);
        });
    }

    SamplePtr metal (const juce::String& name, double seconds, double decay, double hpHz, uint64_t seed)
    {
        static const double freqs[] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
        juce::Random rng ((juce::int64) seed);
        SVF hp1 (hpHz, 0.7, SVF::HP), hp2 (hpHz, 0.7, SVF::HP);
        return render (name, seconds, [=] (int, double t) mutable {
            double sq = 0;
            for (double f : freqs)
                sq += std::sin (twoPi * f * 1.37 * t) >= 0 ? 1.0 : -1.0;
            const double noise = rng.nextDouble() * 2 - 1;
            const double x = sq / 6.0 * 0.7 + noise * 0.3;
            const double attack = std::min (1.0, t / 0.0008);
            return hp2.process (hp1.process (x)) * env (t, decay) * attack;
        });
    }

    SamplePtr clap()
    {
        juce::Random rng (21);
        SVF bp (1200.0, 1.2, SVF::BP);
        return render ("Clap", 0.5, [=] (int, double t) mutable {
            double e = 0;
            for (int b = 0; b < 3; ++b)
            {
                const double tb = t - b * 0.011;
                if (tb >= 0 && tb < 0.011)
                    e = std::max (e, env (tb, 0.004));
            }
            const double tail = t >= 0.022 ? env (t - 0.022, 0.12) : 0.0;
            return bp.process (rng.nextDouble() * 2 - 1) * std::max (e, tail) * 3.0;
        });
    }

    SamplePtr tom (const juce::String& name, double f0, double f1)
    {
        double phase = 0;
        juce::Random rng (31);
        SVF lp (2500.0, 0.7, SVF::LP);
        return render (name, 0.7, [=] (int, double t) mutable {
            const double f = f1 + (f0 - f1) * std::exp (-t / 0.05);
            phase += twoPi * f / kRate;
            const double n = lp.process (rng.nextDouble() * 2 - 1) * env (t, 0.02) * 0.4;
            return softClip ((std::sin (phase) * env (t, 0.22) + n) * 1.4);
        });
    }

    SamplePtr rim()
    {
        SVF bp (1700.0, 4.0, SVF::BP);
        return render ("Rim", 0.12, [=] (int i, double t) mutable {
            const double impulse = i < 8 ? 1.0 : 0.0;
            return bp.process (impulse * 4.0 + std::sin (twoPi * 500.0 * t) * env (t, 0.008));
        });
    }

    SamplePtr cowbell()
    {
        SVF bp (900.0, 2.0, SVF::BP);
        return render ("Cowbell", 0.6, [=] (int, double t) mutable {
            const double sq = (std::sin (twoPi * 540.0 * t) >= 0 ? 1.0 : -1.0) + (std::sin (twoPi * 800.0 * t) >= 0 ? 1.0 : -1.0);
            const double e = t < 0.02 ? 1.0 : 0.55 * env (t - 0.02, 0.15);
            return bp.process (sq * 0.5) * e;
        });
    }

    SamplePtr shaker()
    {
        juce::Random rng (41);
        SVF hp (6000.0, 0.7, SVF::HP);
        return render ("Shaker", 0.2, [=] (int, double t) mutable {
            const double e = std::min (1.0, t / 0.02) * env (std::max (0.0, t - 0.02), 0.04);
            return hp.process (rng.nextDouble() * 2 - 1) * e;
        });
    }

    SamplePtr subBass()
    {
        double phase = 0;
        return render ("Sub Bass", 1.8, [=] (int, double t) mutable {
            const double f = 55.0 + 30.0 * std::exp (-t / 0.03);
            phase += twoPi * f / kRate;
            return softClip (std::sin (phase) * 1.6) * env (t, 0.6);
        });
    }

    SamplePtr fmPerc()
    {
        return render ("FM Perc", 0.35, [] (int, double t) {
            const double mod = std::sin (twoPi * 840.0 * t) * 3.0 * env (t, 0.04);
            return std::sin (twoPi * 600.0 * t + mod) * env (t, 0.09);
        });
    }

    SamplePtr stab()
    {
        static const double notes[] = { 48, 51, 55, 58, 62 }; // C minor 9 voicing
        SVF lp (1800.0, 0.9, SVF::LP);
        return render ("Stab", 1.2, [=] (int, double t) mutable {
            double x = 0;
            for (double m : notes)
            {
                const double f = 440.0 * std::pow (2.0, (m - 69.0) / 12.0);
                for (double detune : { 0.997, 1.003 })
                {
                    const double ph = std::fmod (f * detune * t, 1.0);
                    x += (2.0 * ph - 1.0) * 0.1;
                }
            }
            return lp.process (x) * (t < 0.005 ? t / 0.005 : env (t - 0.005, 0.35));
        });
    }
} // namespace

std::vector<SamplePtr> makeFactoryKit()
{
    return {
        kick ("Kick", 160.0, 48.0, 0.035, 0.18, 1.8),
        snare(),
        metal ("Closed Hat", 0.25, 0.035, 7000.0, 51),
        metal ("Open Hat", 1.0, 0.32, 6500.0, 52),
        clap(),
        rim(),
        tom ("Low Tom", 140.0, 82.0),
        tom ("Mid Tom", 200.0, 120.0),
        tom ("High Tom", 280.0, 170.0),
        cowbell(),
        shaker(),
        metal ("Crash", 2.5, 0.9, 4000.0, 53),
        subBass(),
        fmPerc(),
        stab(),
        kick ("Kick Tight", 220.0, 60.0, 0.02, 0.09, 2.4),
    };
}

SamplePtr makeDemoBreak (const std::vector<SamplePtr>& kit, double bpm)
{
    const double beat = 60.0 / bpm;
    const double sixteenth = beat / 4.0;
    const int steps = 32; // two bars of 16ths
    const int frames = (int) std::ceil (steps * sixteenth * kRate);

    auto s = std::make_shared<Sample>();
    s->name = "Demo Break " + juce::String ((int) bpm);
    s->sampleRate = kRate;
    s->buffer.setSize (2, frames);
    s->buffer.clear();

    auto mixIn = [&] (int padIdx, double timeSec, float gain, float pan) {
        const auto& src = kit[(size_t) padIdx]->buffer;
        const int start = (int) (timeSec * kRate);
        const float gl = gain * (1.0f - std::max (0.0f, pan));
        const float gr = gain * (1.0f + std::min (0.0f, pan));
        for (int i = 0; i < src.getNumSamples() && start + i < frames; ++i)
        {
            const float x = src.getSample (0, i);
            s->buffer.addSample (0, start + i, x * gl);
            s->buffer.addSample (1, start + i, x * gr);
        }
    };

    // a loose, swung boom-bap pattern (straight positions; swing on the off 16ths)
    const char* kickPat  = "x.....x...x.....x.....x..x......";
    const char* snarePat = "....x.......x.......x.......x...";
    const char* hatPat   = "x.x.x.x.x.x.x.xxx.x.x.x.x.x.x.x.";
    for (int i = 0; i < steps; ++i)
    {
        const bool odd = (i % 2) == 1;
        const double t = i * sixteenth + (odd ? sixteenth * 0.24 : 0.0); // ~62% swing
        if (kickPat[i] == 'x')  mixIn (0, t, 0.9f, 0.0f);
        if (snarePat[i] == 'x') mixIn (1, t, 0.8f, 0.0f);
        if (hatPat[i] == 'x')   mixIn (2, t, odd ? 0.35f : 0.55f, 0.3f);
    }
    mixIn (3, 15 * sixteenth + sixteenth * 0.24, 0.35f, 0.3f);
    sampleops::normalize (*s, -1.0f);
    return s;
}

void loadFactoryProject (Project& project)
{
    project = Project();
    project.name = "Factory";

    auto kit = makeFactoryKit();
    auto& prog = project.programs[0];
    prog.name = "Padlab Kit 01";
    for (size_t i = 0; i < kit.size(); ++i)
    {
        project.addSample (kit[i]);
        auto& pad = prog.pads[i];
        pad.layers[0].sample = kit[i];
    }
    // hats choke each other, like a real hi-hat
    prog.pads[2].muteGroup = 1;
    prog.pads[3].muteGroup = 1;
    prog.pads[2].levelDb = -4.0f;
    prog.pads[3].levelDb = -5.0f;
    prog.pads[10].levelDb = -6.0f;
    prog.pads[11].levelDb = -6.0f;
    // melodic pads play as long as they're held
    for (int i : { 12, 14 })
    {
        prog.pads[(size_t) i].trigger = TriggerMode::NoteOn;
        prog.pads[(size_t) i].releaseMs = 120.0f;
    }

    auto demo = makeDemoBreak (kit, 90.0);
    project.addSample (demo);
    auto& chopProg = project.programs[1];
    chopProg.name = "Chop Me";
    chopProg.pads[0].layers[0].sample = demo;

    // a starter groove on sequence 1 / track 1
    auto& seq = project.sequences[0];
    seq.bpm = 90.0;
    seq.bars = 2;
    seq.tracks[0].name = "Drums";
    seq.tracks[1].name = "Chops";
    auto add = [&] (int step, int pad, int vel) {
        NoteEvent e;
        e.tick = (int64_t) step * 240;
        e.pad = (int16_t) pad;
        e.velocity = (uint8_t) vel;
        seq.tracks[0].insertSorted (e);
    };
    for (int bar = 0; bar < 2; ++bar)
    {
        const int o = bar * 16;
        add (o + 0, 0, 120);
        add (o + 10, 0, 100);
        add (o + 4, 1, 115);
        add (o + 12, 1, 118);
        for (int st = 0; st < 16; st += 2)
            add (o + st, 2, st % 4 == 0 ? 96 : 70);
    }
    add (7, 0, 80);
    add (30, 3, 80);
}

} // namespace padlab::drumsynth
