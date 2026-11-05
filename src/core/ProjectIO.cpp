#include "ProjectIO.h"
#include "SampleOps.h"
#include <map>

namespace padlab::projectio
{
namespace
{
    constexpr int kFormatVersion = 1;

    juce::var obj() { return juce::var (new juce::DynamicObject()); }
    void set (juce::var& o, const char* k, const juce::var& v) { o.getDynamicObject()->setProperty (k, v); }
    juce::var get (const juce::var& o, const char* k, const juce::var& def = {})
    {
        return o.hasProperty (k) ? o[k] : def;
    }

    juce::String safeFileName (const juce::String& name)
    {
        auto s = juce::File::createLegalFileName (name).replaceCharacter (' ', '_');
        return s.isEmpty() ? juce::String ("sample") : s;
    }
}

juce::var toJson (const Project& p, const std::map<const Sample*, juce::String>& sampleFiles)
{
    auto root = obj();
    set (root, "format", "padlab-project");
    set (root, "version", kFormatVersion);
    set (root, "name", p.name);
    set (root, "masterLevelDb", p.masterLevelDb);
    set (root, "vintageMode", p.vintageMode);
    {
        auto fx = obj();
        set (fx, "reverbSize", p.fx.reverbSize);
        set (fx, "reverbDamping", p.fx.reverbDamping);
        set (fx, "reverbReturnDb", p.fx.reverbReturnDb);
        set (fx, "delayDivision", p.fx.delayDivision);
        set (fx, "delayFeedback", p.fx.delayFeedback);
        set (fx, "delayReturnDb", p.fx.delayReturnDb);
        set (root, "fx", fx);
    }

    juce::Array<juce::var> samples;
    for (auto& s : p.samplePool)
    {
        auto it = sampleFiles.find (s.get());
        if (it == sampleFiles.end())
            continue;
        auto so = obj();
        set (so, "name", s->name);
        set (so, "file", it->second);
        samples.add (so);
    }
    set (root, "samples", samples);

    juce::Array<juce::var> programs;
    for (auto& prog : p.programs)
    {
        auto po = obj();
        set (po, "name", prog.name);
        set (po, "levelDb", prog.levelDb);
        set (po, "pan", prog.pan);
        juce::Array<juce::var> pads;
        for (int i = 0; i < kNumPads; ++i)
        {
            const auto& pad = prog.pads[(size_t) i];
            if (! pad.hasSample())
                continue;
            auto pd = obj();
            set (pd, "index", i);
            set (pd, "levelDb", pad.levelDb);
            set (pd, "pan", pad.pan);
            set (pd, "tune", pad.tuneSemis);
            set (pd, "attackMs", pad.attackMs);
            set (pd, "decayMs", pad.decayMs);
            set (pd, "sustain", pad.sustain);
            set (pd, "releaseMs", pad.releaseMs);
            set (pd, "filter", (int) pad.filter);
            set (pd, "cutoffHz", pad.cutoffHz);
            set (pd, "resonance", pad.resonance);
            set (pd, "muteGroup", pad.muteGroup);
            set (pd, "trigger", (int) pad.trigger);
            set (pd, "poly", (int) pad.poly);
            set (pd, "mute", pad.mute);
            set (pd, "reverbSend", pad.reverbSend);
            set (pd, "delaySend", pad.delaySend);
            juce::Array<juce::var> layers;
            for (auto& l : pad.layers)
            {
                auto lo = obj();
                set (lo, "sample", l.sample != nullptr ? juce::var (l.sample->name) : juce::var());
                set (lo, "velStart", l.velStart);
                set (lo, "velEnd", l.velEnd);
                set (lo, "tune", l.tuneSemis);
                set (lo, "levelDb", l.levelDb);
                set (lo, "pan", l.pan);
                set (lo, "sliceStart", l.sliceStart);
                set (lo, "sliceEnd", l.sliceEnd);
                layers.add (lo);
            }
            set (pd, "layers", layers);
            pads.add (pd);
        }
        set (po, "pads", pads);
        programs.add (po);
    }
    set (root, "programs", programs);

    juce::Array<juce::var> seqs;
    const auto defaults = std::make_unique<Project>(); // ~1 MB: keep it off the stack
    for (auto& seq : p.sequences)
    {
        auto so = obj();
        set (so, "name", seq.name);
        set (so, "bpm", seq.bpm);
        set (so, "bars", seq.bars);
        set (so, "beatsPerBar", seq.beatsPerBar);
        set (so, "loop", seq.loop);
        set (so, "loopToBar", seq.loopToBar);
        juce::Array<juce::var> tracks;
        for (int ti = 0; ti < kNumTracks; ++ti)
        {
            const auto& t = seq.tracks[(size_t) ti];
            const auto& d = defaults->sequences[0].tracks[(size_t) ti];
            // only tracks that are used or set up (64 x 99 would be a lot of empty objects)
            if (t.events.empty() && t.name == d.name && t.programIndex == d.programIndex && ! t.mute && ! t.solo
                && t.midiChannel == 0 && t.internalSound)
                continue;
            auto to = obj();
            set (to, "index", ti);
            set (to, "midiChannel", t.midiChannel);
            set (to, "internal", t.internalSound);
            set (to, "name", t.name);
            set (to, "program", t.programIndex);
            set (to, "mute", t.mute);
            set (to, "solo", t.solo);
            juce::Array<juce::var> events;
            for (auto& e : t.events)
            {
                juce::Array<juce::var> ev { (juce::int64) e.tick, (int) e.pad, (int) e.velocity, (int) e.length, e.tune };
                if (e.variation.type != VariationType::None) // optional: [.., varType, varValue]
                {
                    ev.add ((int) e.variation.type);
                    ev.add ((int) e.variation.value);
                }
                events.add (ev);
            }
            set (to, "events", events);
            tracks.add (to);
        }
        set (so, "tracks", tracks);
        seqs.add (so);
    }
    set (root, "sequences", seqs);

    juce::Array<juce::var> songs;
    for (auto& sg : p.songs)
    {
        auto so = obj();
        set (so, "name", sg.name);
        set (so, "loop", sg.loop);
        set (so, "loopToStep", sg.loopToStep);
        juce::Array<juce::var> steps;
        for (int i = 0; i < sg.numSteps; ++i)
            steps.add (juce::Array<juce::var> { sg.steps[(size_t) i].sequence, sg.steps[(size_t) i].repeats });
        set (so, "steps", steps);
        songs.add (so);
    }
    set (root, "songs", songs);
    return root;
}

bool fromJson (const juce::var& json, Project& out, const juce::File& samplesDir, juce::String& error)
{
    if (get (json, "format").toString() != "padlab-project")
    {
        error = "Not a Padlab project";
        return false;
    }
    if ((int) get (json, "version", 0) > kFormatVersion)
    {
        error = "Project was saved by a newer version of Padlab";
        return false;
    }

    auto fresh = std::make_unique<Project>(); // ~1 MB: keep it off the stack
    auto& p = *fresh;
    p.name = get (json, "name", "Untitled").toString();
    p.masterLevelDb = (float) get (json, "masterLevelDb", 0.0);
    p.vintageMode = juce::jlimit (0, 2, (int) get (json, "vintageMode", 0));
    if (auto fx = get (json, "fx"); fx.isObject())
    {
        p.fx.reverbSize = (float) get (fx, "reverbSize", p.fx.reverbSize);
        p.fx.reverbDamping = (float) get (fx, "reverbDamping", p.fx.reverbDamping);
        p.fx.reverbReturnDb = (float) get (fx, "reverbReturnDb", p.fx.reverbReturnDb);
        p.fx.delayDivision = (int) get (fx, "delayDivision", p.fx.delayDivision);
        p.fx.delayFeedback = (float) get (fx, "delayFeedback", p.fx.delayFeedback);
        p.fx.delayReturnDb = (float) get (fx, "delayReturnDb", p.fx.delayReturnDb);
    }

    if (auto* samples = get (json, "samples").getArray())
        for (auto& so : *samples)
        {
            auto file = samplesDir.getChildFile (so["file"].toString());
            auto s = sampleops::loadFile (file);
            if (s == nullptr)
            {
                error = "Missing sample: " + file.getFileName();
                return false;
            }
            s->name = so["name"].toString();
            p.addSample (s);
        }

    if (auto* programs = get (json, "programs").getArray())
        for (int pi = 0; pi < std::min (programs->size(), kNumPrograms); ++pi)
        {
            const auto& po = programs->getReference (pi);
            auto& prog = p.programs[(size_t) pi];
            prog.name = get (po, "name", prog.name).toString();
            prog.levelDb = (float) get (po, "levelDb", 0.0);
            prog.pan = (float) get (po, "pan", 0.0);
            if (auto* pads = get (po, "pads").getArray())
                for (auto& pd : *pads)
                {
                    const int idx = (int) get (pd, "index", -1);
                    if (idx < 0 || idx >= kNumPads)
                        continue;
                    auto& pad = prog.pads[(size_t) idx];
                    pad.levelDb = (float) get (pd, "levelDb", 0.0);
                    pad.pan = (float) get (pd, "pan", 0.0);
                    pad.tuneSemis = (float) get (pd, "tune", 0.0);
                    pad.attackMs = (float) get (pd, "attackMs", 0.0);
                    pad.decayMs = (float) get (pd, "decayMs", 300.0);
                    pad.sustain = (float) get (pd, "sustain", 1.0);
                    pad.releaseMs = (float) get (pd, "releaseMs", 30.0);
                    pad.filter = (FilterType) juce::jlimit (0, 3, (int) get (pd, "filter", 0));
                    pad.cutoffHz = (float) get (pd, "cutoffHz", 20000.0);
                    pad.resonance = (float) get (pd, "resonance", 0.1);
                    pad.muteGroup = (int) get (pd, "muteGroup", 0);
                    pad.trigger = (TriggerMode) juce::jlimit (0, 1, (int) get (pd, "trigger", 0));
                    pad.poly = (PolyMode) juce::jlimit (0, 1, (int) get (pd, "poly", 0));
                    pad.mute = (bool) get (pd, "mute", false);
                    pad.reverbSend = (float) get (pd, "reverbSend", 0.0);
                    pad.delaySend = (float) get (pd, "delaySend", 0.0);
                    if (auto* layers = get (pd, "layers").getArray())
                        for (int li = 0; li < std::min (layers->size(), kNumLayers); ++li)
                        {
                            const auto& lo = layers->getReference (li);
                            auto& l = pad.layers[(size_t) li];
                            const auto sname = get (lo, "sample");
                            l.sample = sname.isVoid() ? nullptr : p.findSample (sname.toString());
                            l.velStart = (int) get (lo, "velStart", 0);
                            l.velEnd = (int) get (lo, "velEnd", 127);
                            l.tuneSemis = (float) get (lo, "tune", 0.0);
                            l.levelDb = (float) get (lo, "levelDb", 0.0);
                            l.pan = (float) get (lo, "pan", 0.0);
                            l.sliceStart = (int) get (lo, "sliceStart", 0);
                            l.sliceEnd = (int) get (lo, "sliceEnd", -1);
                        }
                }
        }

    if (auto* seqs = get (json, "sequences").getArray())
        for (int si = 0; si < std::min (seqs->size(), kNumSequences); ++si)
        {
            const auto& so = seqs->getReference (si);
            auto& seq = p.sequences[(size_t) si];
            seq.name = get (so, "name", seq.name).toString();
            seq.bpm = juce::jlimit (30.0, 300.0, (double) get (so, "bpm", 90.0));
            seq.bars = juce::jlimit (1, kMaxBars, (int) get (so, "bars", 2));
            seq.beatsPerBar = juce::jlimit (1, 16, (int) get (so, "beatsPerBar", 4));
            seq.loop = (bool) get (so, "loop", true);
            seq.loopToBar = juce::jlimit (0, seq.bars - 1, (int) get (so, "loopToBar", 0));
            if (auto* tracks = get (so, "tracks").getArray())
                for (int pos = 0; pos < tracks->size(); ++pos)
                {
                    const auto& to = tracks->getReference (pos);
                    const int ti = (int) get (to, "index", pos); // older files list every track in order
                    if (ti < 0 || ti >= kNumTracks)
                        continue;
                    auto& t = seq.tracks[(size_t) ti];
                    t.name = get (to, "name", t.name).toString();
                    t.programIndex = juce::jlimit (0, kNumPrograms - 1, (int) get (to, "program", ti % kNumPrograms));
                    t.midiChannel = juce::jlimit (0, 16, (int) get (to, "midiChannel", 0));
                    t.internalSound = (bool) get (to, "internal", true);
                    t.mute = (bool) get (to, "mute", false);
                    t.solo = (bool) get (to, "solo", false);
                    t.events.clear();
                    if (auto* evs = get (to, "events").getArray())
                        for (auto& ev : *evs)
                        {
                            if (! ev.isArray() || ev.size() < 5)
                                continue;
                            NoteEvent e;
                            e.tick = (juce::int64) ev[0];
                            e.pad = (int16_t) juce::jlimit (0, kNumPads - 1, (int) ev[1]);
                            e.velocity = (uint8_t) juce::jlimit (1, 127, (int) ev[2]);
                            e.length = std::max (1, (int) ev[3]);
                            e.tune = (float) (double) ev[4];
                            if (ev.size() >= 7)
                                e.variation = { (VariationType) juce::jlimit (0, 4, (int) ev[5]),
                                                (uint8_t) juce::jlimit (0, 127, (int) ev[6]) };
                            if (e.tick >= 0 && e.tick < seq.getLengthTicks())
                                t.insertSorted (e);
                        }
                }
        }

    if (auto* songs = get (json, "songs").getArray())
        for (int si = 0; si < std::min (songs->size(), kNumSongs); ++si)
        {
            const auto& so = songs->getReference (si);
            auto& sg = p.songs[(size_t) si];
            sg.name = get (so, "name", sg.name).toString();
            sg.loop = (bool) get (so, "loop", false);
            sg.numSteps = 0;
            if (auto* steps = get (so, "steps").getArray())
                for (auto& st : *steps)
                    if (st.isArray() && st.size() >= 2)
                        sg.insertStep (sg.numSteps, { (int) st[0], (int) st[1] });
            sg.loopToStep = juce::jlimit (0, juce::jmax (0, sg.numSteps - 1), (int) get (so, "loopToStep", 0));
        }

    out = std::move (p);
    return true;
}

SaveJob prepareSave (const Project& p)
{
    SaveJob job;
    std::map<const Sample*, juce::String> files;
    juce::StringArray used;
    for (auto& s : p.samplePool)
    {
        auto base = safeFileName (s->name);
        auto fname = base + ".wav";
        for (int i = 2; used.contains (fname, true); ++i)
            fname = base + "_" + juce::String (i) + ".wav";
        used.add (fname);
        files[s.get()] = fname;
        job.files.emplace_back (s, fname);
    }
    job.json = toJson (p, files);
    return job;
}

bool writeSave (const SaveJob& job, const juce::File& folder, juce::String& error)
{
    if (! folder.createDirectory())
    {
        error = "Couldn't create " + folder.getFullPathName();
        return false;
    }
    auto samplesDir = folder.getChildFile ("samples");
    samplesDir.deleteRecursively();
    samplesDir.createDirectory();

    for (auto& [sample, fname] : job.files)
        if (! sampleops::writeWav (*sample, samplesDir.getChildFile (fname)))
        {
            error = "Couldn't write sample " + fname;
            return false;
        }

    if (! folder.getChildFile ("project.json").replaceWithText (juce::JSON::toString (job.json, false)))
    {
        error = "Couldn't write project.json";
        return false;
    }
    return true;
}

bool save (const Project& p, const juce::File& folder, juce::String& error)
{
    return writeSave (prepareSave (p), folder, error);
}

bool load (const juce::File& folder, Project& out, juce::String& error)
{
    auto file = folder.getChildFile ("project.json");
    if (! file.existsAsFile())
    {
        error = "No project.json in " + folder.getFullPathName();
        return false;
    }
    auto json = juce::JSON::parse (file);
    if (json.isVoid())
    {
        error = "project.json is not valid JSON";
        return false;
    }
    return fromJson (json, out, folder.getChildFile ("samples"), error);
}

} // namespace padlab::projectio
