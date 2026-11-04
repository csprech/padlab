#include "engine/Engine.h"
#include "core/DrumSynth.h"
#include <cmath>

namespace padlab
{
namespace
{
    inline float dbToGain (float db) { return db <= -96.0f ? 0.0f : std::pow (10.0f, db / 20.0f); }

    inline float softLimit (float x)
    {
        const float a = std::abs (x);
        if (a < 0.8f)
            return x;
        const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f);
        return x < 0 ? -y : y;
    }
}

Engine::Engine()
{
    project = std::make_unique<Project>();
    drumsynth::loadFactoryProject (*project);

    deviceManager.initialiseWithDefaultDevices (0, 2);
    deviceManager.addAudioCallback (this);

    for (auto& info : juce::MidiInput::getAvailableDevices())
    {
        deviceManager.setMidiInputDeviceEnabled (info.identifier, true);
        deviceManager.addMidiInputDeviceCallback (info.identifier, this);
    }
    // testing aid: PADLAB_MIDI_CLOCK_OUT="<output name>" turns MIDI clock on to that output
    if (auto* name = std::getenv ("PADLAB_MIDI_CLOCK_OUT"))
        for (auto& info : juce::MidiOutput::getAvailableDevices())
            if (info.name == juce::String (name))
            {
                deviceManager.setDefaultMidiOutputDevice (info.identifier);
                sendMidiClock = true;
            }
    clockSender = std::make_unique<ClockSender> (*this);
    clockSender->startTimer (1);
    startTimer (500);
}

Engine::~Engine()
{
    clockSender->stopTimer();
    clockSender.reset();
    stopTimer();
    deviceManager.removeAudioCallback (this);
    for (auto& info : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback (info.identifier, this);
    deviceManager.closeAudioDevice();
}

void Engine::setInputEnabled (bool enabled)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();
    if (enabled)
        setup.inputChannels.setRange (0, 2, true);
    deviceManager.setAudioDeviceSetup (setup, true);
}

bool Engine::isInputEnabled() const
{
    if (auto* d = deviceManager.getCurrentAudioDevice())
        return d->getActiveInputChannels().countNumberOfSetBits() > 0;
    return false;
}

// ---------------------------------------------------------------- project

void Engine::replaceProject (std::unique_ptr<Project> newProject)
{
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        std::swap (project, newProject);
    }
    // newProject now holds the old one: keep its samples alive until voices let go
    for (auto& s : newProject->samplePool)
        retire (s);
    newProject.reset();
    undoStack.clear();
    commands.push ({ Command::Stop });
    commands.push ({ Command::Panic });
    commands.push ({ Command::Queue, 0 });
}

void Engine::retire (SamplePtr s)
{
    if (s != nullptr)
        graveyard.push_back (std::move (s));
}

void Engine::ClockSender::hiResTimerCallback()
{
    events.clear();
    const bool want = engine.sendMidiClock.load();
    const bool playing = want && engine.uiState.load() != (int) Transport::State::Stopped
                         && engine.uiState.load() != (int) Transport::State::CountIn;
    if (! want && ! active)
        return;
    // extrapolate between audio blocks so the clock doesn't jitter by the block size
    double ticks = engine.uiRunningTicks.load();
    if (playing)
        ticks += juce::jmax (0.0, juce::Time::getMillisecondCounterHiRes() - engine.uiRunStampMs.load()) * engine.uiTicksPerMs.load();
    clock.update (playing, ticks, engine.uiPlayStartTick.load(), events);
    active = playing;
    if (events.empty())
        return;
    const juce::ScopedLock sl (engine.clockOutLock);
    if (engine.clockOut == nullptr)
        return;
    for (auto& e : events)
    {
        if (e.type == MidiClock::Msg::SongPosition)
            engine.clockOut->sendMessageNow (juce::MidiMessage::songPositionPointer (e.songPosition));
        else
            engine.clockOut->sendMessageNow (juce::MidiMessage ((int) e.type));
    }
}

juce::String Engine::getMidiClockOutputName() const
{
    const juce::ScopedLock sl (clockOutLock);
    return clockOut != nullptr ? clockOut->getName() : juce::String();
}

void Engine::timerCallback()
{
    // follow the MIDI output chosen in Audio/MIDI setup (opened here, used by the clock thread)
    const auto id = deviceManager.getDefaultMidiOutputIdentifier();
    if (id != clockOutId)
    {
        auto out = id.isNotEmpty() ? juce::MidiOutput::openDevice (id) : nullptr;
        const juce::ScopedLock sl (clockOutLock);
        clockOut = std::move (out);
        clockOutId = id;
    }

    // free retired samples nobody references any more (we hold the only reference)
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(),
                                     [] (const SamplePtr& s) { return s.use_count() <= 1; }),
                     graveyard.end());
}

PerformanceSettings Engine::getSettings() const
{
    const LockGuard g (*this);
    return settings;
}

// ---------------------------------------------------------------- input

void Engine::pushPad (LiveInput in, bool fromMidi)
{
    if (fromMidi)
        midiPads.push (in);
    else
        uiPads.push (in);
}

void Engine::padDown (int pad, int velocity) { pushPad ({ LiveInput::PadDown, pad, velocity, 0 }, false); }
void Engine::padUp (int pad) { pushPad ({ LiveInput::PadUp, pad, 0, 0 }, false); }
void Engine::padPressure (int pad, int pressure) { pushPad ({ LiveInput::Pressure, pad, pressure, 0 }, false); }

void Engine::audition (int track, int pad, int velocity)
{
    commands.push ({ Command::Audition, velocity, track, pad });
}

void Engine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    // Pad controllers: notes 36..51 -> pads 1..16 of the current bank
    auto toPad = [this] (int note) -> int {
        if (note < 36 || note > 51)
            return -1;
        return currentBank.load() * kPadsPerBank + (note - 36);
    };

    if (m.isNoteOn())
    {
        if (auto p = toPad (m.getNoteNumber()); p >= 0)
        {
            midiNotePad[(size_t) m.getNoteNumber()] = p;
            pushPad ({ LiveInput::PadDown, p, m.getVelocity(), 0 }, true);
        }
    }
    else if (m.isNoteOff())
    {
        if (auto p = midiNotePad[(size_t) m.getNoteNumber()]; p >= 0)
        {
            midiNotePad[(size_t) m.getNoteNumber()] = -1;
            pushPad ({ LiveInput::PadUp, p, 0, 0 }, true);
        }
    }
    else if (m.isAftertouch())
    {
        if (auto p = midiNotePad[(size_t) m.getNoteNumber()]; p >= 0)
            pushPad ({ LiveInput::Pressure, p, m.getAfterTouchValue(), 0 }, true);
    }
    else if (m.isChannelPressure())
    {
        for (int i = 0; i < kPadsPerBank; ++i)
            pushPad ({ LiveInput::Pressure, currentBank.load() * kPadsPerBank + i, m.getChannelPressureValue(), 0 }, true);
    }
    else if (m.isController() && m.getControllerNumber() >= 70 && m.getControllerNumber() <= 74)
    {
        const int knob = m.getControllerNumber() - 70, value = m.getControllerValue();
        juce::MessageManager::callAsync ([this, knob, value] {
            if (onQLinkMidi)
                onQLinkMidi (knob, value);
        });
    }
}

// ---------------------------------------------------------------- transport

void Engine::play (bool fromStart) { commands.push ({ fromStart ? Command::PlayStart : Command::Play }); }
void Engine::stop() { commands.push ({ Command::Stop }); }
void Engine::setRecord (RecordMode mode) { commands.push ({ Command::Record, (int64_t) mode }); }
void Engine::queueSequence (int index) { commands.push ({ Command::Queue, index }); }
void Engine::locate (int64_t tick) { commands.push ({ Command::Locate, tick }); }
void Engine::panic() { commands.push ({ Command::Panic }); }

// ---------------------------------------------------------------- undo

void Engine::pushUndo (const juce::String& what)
{
    UndoState u;
    u.what = what;
    withProject ([&] (Project& p) {
        u.sequence = uiSequence.load();
        auto& seq = p.sequences[(size_t) u.sequence];
        u.bpm = seq.bpm;
        u.bars = seq.bars;
        for (int t = 0; t < kNumTracks; ++t)
            u.events[(size_t) t] = seq.tracks[(size_t) t].events;
    });
    undoStack.push_back (std::move (u));
    if (undoStack.size() > 50)
        undoStack.erase (undoStack.begin());
}

bool Engine::undo()
{
    if (undoStack.empty())
        return false;
    auto u = std::move (undoStack.back());
    undoStack.pop_back();
    withProject ([&] (Project& p) {
        auto& seq = p.sequences[(size_t) u.sequence];
        seq.bpm = u.bpm;
        seq.bars = u.bars;
        for (int t = 0; t < kNumTracks; ++t)
            seq.tracks[(size_t) t].events.assign (u.events[(size_t) t].begin(), u.events[(size_t) t].end());
    });
    return true;
}

juce::String Engine::getUndoDescription() const
{
    return undoStack.empty() ? juce::String() : undoStack.back().what;
}

// ---------------------------------------------------------------- preview

void Engine::previewSample (SamplePtr smp, int start, int end)
{
    if (smp == nullptr)
        return;
    SamplePtr old;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        old = std::move (previewSamplePtr);
        const int n = smp->getNumFrames();
        previewPos = juce::jlimit (0, std::max (0, n - 1), start);
        previewEnd = end < 0 ? n : juce::jlimit ((int) previewPos, n, end);
        previewInc = smp->sampleRate / sampleRate.load();
        previewSamplePtr = std::move (smp);
        previewActive = true;
    }
    retire (old);
}

void Engine::stopPreview()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    previewActive = false;
}

// ---------------------------------------------------------------- sampling

void Engine::armSampling (bool resampleMaster, float thresholdDb, double maxSeconds)
{
    const int frames = (int) (std::clamp (maxSeconds, 1.0, 120.0) * sampleRate.load());
    juce::AudioBuffer<float> fresh;
    if (recBuffer.getNumSamples() < frames)
        fresh.setSize (2, frames); // allocate outside the audio lock

    const LockGuard g (*this);
    if (fresh.getNumSamples() > 0)
        std::swap (recBuffer, fresh); // old buffer is freed on this (message) thread
    recMaxFrames = frames;
    recResample = resampleMaster;
    recThreshold = thresholdDb <= -60.5f ? 0.0f : dbToGain (thresholdDb);
    recWritten = 0;
    recStopRequest = false;
    recState = (int) RecState::Armed;
}

void Engine::stopSampling() { recStopRequest = true; }

double Engine::getSampledSeconds() const { return recWritten.load() / sampleRate.load(); }

SamplePtr Engine::takeSampledAudio (const juce::String& name)
{
    if (getSamplingState() != RecState::Done)
        return nullptr;
    // Once Done, the audio thread no longer touches recBuffer, so copy without the lock.
    auto s = std::make_shared<Sample>();
    s->name = name;
    s->sampleRate = sampleRate.load();
    const int n = std::max (1, recWritten.load());
    s->buffer.setSize (2, n);
    for (int c = 0; c < 2; ++c)
        s->buffer.copyFrom (c, 0, recBuffer, c, 0, n);
    recWritten = 0;
    recState = (int) RecState::Idle;
    return s;
}

// ---------------------------------------------------------------- audio

void Engine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    sampleRate = device->getCurrentSampleRate();
    transport.prepare (sampleRate);
    sampler.prepare (sampleRate);
    trackBuffers.setSize (kNumTracks * 2, std::max (4096, device->getCurrentBufferSizeSamples() * 2));
    clickLength = (int) (0.03 * sampleRate);
    if (recBuffer.getNumSamples() == 0)
        recBuffer.setSize (2, (int) (30.0 * sampleRate));
}

void Engine::audioDeviceStopped() {}

void Engine::renderClick (float* l, float* r, int start, int num)
{
    const double sr = sampleRate.load();
    for (int i = start; i < start + num && clickRemaining > 0; ++i, --clickRemaining)
    {
        const float env = (float) clickRemaining / (float) clickLength;
        const float x = (float) std::sin (clickPhase) * env * env * clickGain;
        clickPhase += juce::MathConstants<double>::twoPi * clickFreq / sr;
        l[i] += x;
        r[i] += x;
    }
}

void Engine::audioDeviceIOCallbackWithContext (const float* const* inputs, int numIn, float* const* outputs,
                                               int numOut, int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c)
        if (outputs[c] != nullptr)
            juce::FloatVectorOperations::clear (outputs[c], numSamples);

    const juce::SpinLock::ScopedLockType sl (lock);
    if (numSamples > trackBuffers.getNumSamples())
        return; // device misreported its block size; skip rather than allocate

    auto& proj = *project;

    // ---- commands
    commands.drain ([&] (const Command& c) {
        switch (c.type)
        {
            case Command::Play:
                if (settings.songMode)
                    transport.playSong (proj, settings.currentSong, settings.songStartStep);
                else
                    transport.play (false, settings.countIn);
                break;
            case Command::PlayStart:
                if (settings.songMode)
                    transport.playSong (proj, settings.currentSong, 0);
                else
                {
                    transport.locate (0);
                    transport.play (true, settings.countIn);
                }
                break;
            case Command::Stop:      transport.stop(); break;
            case Command::Record:    transport.armRecord ((RecordMode) c.value); break;
            case Command::Queue:     transport.queueSequence ((int) c.value); break;
            case Command::Locate:    transport.locate (c.value); break;
            case Command::Panic:     sampler.killAll(); break;
            case Command::Audition:
            {
                const auto& seq = proj.sequences[(size_t) transport.getCurrentSequence()];
                const int t = juce::jlimit (0, kNumTracks - 1, c.a);
                const auto& prog = proj.programs[(size_t) seq.tracks[(size_t) t].programIndex];
                sampler.trigger (prog, t, c.b, (int) c.value, 0.0f, (int) (0.5 * sampleRate.load()));
                break;
            }
        }
    });

    // ---- live input
    int numInputs = 0;
    auto collect = [&] (const LiveInput& in) {
        if (numInputs < (int) liveInputs.size())
            liveInputs[(size_t) numInputs++] = in;
    };
    uiPads.drain (collect);
    midiPads.drain (collect);

    // ---- sequencer
    transport.process (proj, settings, liveInputs.data(), numInputs, numSamples, actions);

    // ---- render with sample-accurate action offsets
    trackBuffers.clear (0, numSamples);
    const auto& seq = proj.sequences[(size_t) transport.getCurrentSequence()];
    auto* masterL = outputs[0];
    auto* masterR = numOut > 1 ? outputs[1] : nullptr;

    int cursor = 0;
    for (int i = 0; i < actions.size; ++i)
    {
        const auto& a = actions.items[(size_t) i];
        const int at = juce::jlimit (0, numSamples, a.offset);
        if (at > cursor)
        {
            sampler.render (trackBuffers, cursor, at - cursor);
            if (masterL != nullptr && masterR != nullptr)
                renderClick (masterL, masterR, cursor, at - cursor);
            cursor = at;
        }
        switch (a.type)
        {
            case Action::Trigger:
            {
                const auto& prog = proj.programs[(size_t) seq.tracks[(size_t) a.track].programIndex];
                sampler.trigger (prog, a.track, a.pad, a.velocity, a.tune, a.releaseAfter, a.variation);
                if (a.track == settings.currentTrack && a.pad >= 0 && a.pad < kNumPads)
                {
                    padHitVel[(size_t) a.pad] = a.velocity;
                    padHits[(size_t) a.pad].fetch_add (1);
                }
                break;
            }
            case Action::Release:
                sampler.release (a.track, a.pad);
                break;
            case Action::Click:
                clickPhase = 0.0;
                clickFreq = a.accent ? 1600.0 : 1000.0;
                clickGain = a.accent ? 0.45f : 0.3f;
                clickRemaining = clickLength;
                break;
        }
    }
    if (cursor < numSamples)
    {
        sampler.render (trackBuffers, cursor, numSamples - cursor);
        if (masterL != nullptr && masterR != nullptr)
            renderClick (masterL, masterR, cursor, numSamples - cursor);
    }

    // ---- sample preview (straight to the master bus)
    if (previewActive && previewSamplePtr != nullptr && masterL != nullptr)
    {
        const auto& b = previewSamplePtr->buffer;
        const float* pl = b.getReadPointer (0);
        const float* pr = b.getNumChannels() > 1 ? b.getReadPointer (1) : pl;
        for (int i = 0; i < numSamples; ++i)
        {
            const int idx = (int) previewPos;
            if (idx >= previewEnd)
            {
                previewActive = false;
                break;
            }
            masterL[i] += pl[idx] * 0.8f;
            if (masterR != nullptr)
                masterR[i] += pr[idx] * 0.8f;
            previewPos += previewInc;
        }
    }
    previewPosUi = previewActive ? (int) previewPos : -1;

    // ---- mix tracks through their program channel strips
    const float master = dbToGain (proj.masterLevelDb);
    for (int t = 0; t < kNumTracks; ++t)
    {
        const auto& prog = proj.programs[(size_t) seq.tracks[(size_t) t].programIndex];
        const float g = dbToGain (prog.levelDb) * master;
        const float angle = (juce::jlimit (-1.0f, 1.0f, prog.pan) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float gl = g * std::cos (angle) * juce::MathConstants<float>::sqrt2;
        const float gr = g * std::sin (angle) * juce::MathConstants<float>::sqrt2;
        const float* tl = trackBuffers.getReadPointer (t * 2);
        const float* tr = trackBuffers.getReadPointer (t * 2 + 1);
        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float l = tl[i] * gl, r = tr[i] * gr;
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
            if (masterL != nullptr) masterL[i] += l;
            if (masterR != nullptr) masterR[i] += r;
        }
        trackPeaks[(size_t) t] = peak;
    }

    if (proj.vintageMode != 0 && masterL != nullptr)
        applyVintage (proj.vintageMode, masterL, masterR, numSamples);

    float peakL = 0.0f, peakR = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        if (masterL != nullptr) { masterL[i] = softLimit (masterL[i]); peakL = std::max (peakL, std::abs (masterL[i])); }
        if (masterR != nullptr) { masterR[i] = softLimit (masterR[i]); peakR = std::max (peakR, std::abs (masterR[i])); }
    }
    masterPeaks[0] = peakL;
    masterPeaks[1] = peakR;

    // ---- sampling (from audio input or the master bus)
    {
        const bool resample = recResample.load();
        const float* srcL = resample ? masterL : (numIn > 0 ? inputs[0] : nullptr);
        const float* srcR = resample ? (masterR != nullptr ? masterR : masterL) : (numIn > 1 ? inputs[1] : srcL);
        const float inGain = resample ? 1.0f : recGain.load(); // REC GAIN acts on the audio input only
        float inPeakL = 0.0f, inPeakR = 0.0f;
        if (srcL != nullptr)
            for (int i = 0; i < numSamples; ++i)
            {
                inPeakL = std::max (inPeakL, std::abs (srcL[i] * inGain));
                inPeakR = std::max (inPeakR, std::abs (srcR[i] * inGain));
            }
        inputPeaks[0] = inPeakL;
        inputPeaks[1] = inPeakR;

        auto state = (RecState) recState.load();
        if (state == RecState::Armed && srcL != nullptr
            && (recThreshold.load() <= 0.0f || std::max (inPeakL, inPeakR) >= recThreshold.load()))
            recState = (int) (state = RecState::Recording);

        if (state == RecState::Recording && srcL != nullptr)
        {
            const int written = recWritten.load();
            const int room = std::min (recMaxFrames.load(), recBuffer.getNumSamples()) - written;
            const int n = std::min (room, numSamples);
            if (n > 0)
            {
                recBuffer.copyFrom (0, written, srcL, n, inGain);
                recBuffer.copyFrom (1, written, srcR, n, inGain);
                recWritten = written + n;
            }
            if (n < numSamples || recStopRequest.load())
                recState = (int) RecState::Done;
        }
        else if (state == RecState::Recording && srcL == nullptr)
        {
            recState = (int) RecState::Done; // input went away mid-recording
        }
        else if (recStopRequest.load() && state == RecState::Armed)
        {
            recState = (int) RecState::Idle;
        }
        recStopRequest = false;
    }

    // ---- MAIN VOLUME (after resampling, like the analog output stage); smoothed to avoid zipper noise
    {
        const float v = mainVolume.load();
        const float target = v * v; // audio taper
        const float start = smoothedVolume;
        const float step = (target - start) / (float) juce::jmax (1, numSamples);
        for (int c = 0; c < juce::jmin (2, numOut); ++c)
            if (auto* out = outputs[c])
                for (int i = 0; i < numSamples; ++i)
                    out[i] *= start + step * (float) (i + 1);
        smoothedVolume = target;
    }

    // ---- publish state for the UI (and the MIDI clock thread)
    uiRunningTicks = transport.getRunningTicks();
    uiPlayStartTick = transport.getPlayStartTick();
    uiTicksPerMs = proj.sequences[(size_t) transport.getCurrentSequence()].bpm / 60.0 * kPPQN / 1000.0;
    uiRunStampMs = juce::Time::getMillisecondCounterHiRes();
    uiPlayingSong = transport.isPlayingSong();
    uiSongStep = transport.getSongStep();
    uiSongRepeat = transport.getSongRepeat();
    uiState = (int) transport.getState();
    uiRecordMode = (int) transport.getRecordMode();
    uiPosition = transport.getPositionTicks();
    uiCountIn = transport.getCountInTicks();
    uiSequence = transport.getCurrentSequence();
    uiQueued = transport.getQueuedSequence();
    activeVoices = sampler.getActiveVoiceCount();

    for (int c = 2; c < numOut; ++c)
        if (outputs[c] != nullptr)
            juce::FloatVectorOperations::clear (outputs[c], numSamples);
}

void Engine::applyVintage (int mode, float* l, float* r, int n)
{
    // Converter character of classic samplers, applied on the master bus:
    //  1: 12-bit / 40 kHz era - coarse 12-bit quantisation and a steep-ish anti-alias roll-off (~12 kHz)
    //  2: 16-bit warm - gentle roll-off (~17 kHz) and soft analog-style saturation
    const double sr = sampleRate.load();
    const float fc = mode == 1 ? 12000.0f : 17000.0f;
    const float a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sr);
    float* ch[2] = { l, r != nullptr ? r : nullptr };
    for (int c = 0; c < 2; ++c)
    {
        if (ch[c] == nullptr)
            continue;
        float s1 = vintageLp1[c], s2 = vintageLp2[c];
        for (int i = 0; i < n; ++i)
        {
            float x = ch[c][i];
            s1 += a * (x - s1);
            s2 += a * (s1 - s2);
            x = mode == 1 ? s2 : s1;
            if (mode == 1)
                x = std::round (juce::jlimit (-1.0f, 1.0f, x) * 2047.0f) / 2047.0f;
            else
                x = std::tanh (x * 1.15f) / std::tanh (1.15f);
            ch[c][i] = x;
        }
        vintageLp1[c] = s1;
        vintageLp2[c] = s2;
    }
}

} // namespace padlab
