# Padlab

A desktop sampling groove workstation built for research. It reproduces the workflow and feel of classic pad-based samplers, using an original name, UI and synthesized factory sounds. It doesn't use or contain any third-party firmware, samples or artwork.

C++20 / JUCE 8, macOS (Apple Silicon tested). The core is platform-neutral.

## Build & run

```sh
scripts/bootstrap.sh                       # fetches JUCE 8.0.15 into external/ (not committed)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
open build/Padlab_artefacts/Release/Padlab.app
./build/PadlabTests_artefacts/Release/PadlabTests   # 128 engine checks
```

## The workflow

**Sample → chop → pads → play/record with swing & note repeat → sequences → mix**

| Area | What it does |
|---|---|
| **Pads** | 4×4 pads, banks A–H (128 pads per program). Mouse: where you hit the pad sets the velocity (top = hardest), dragging while held = pressure, Shift = 127. Drop audio files on a pad to load them (several files fill consecutive pads). |
| **Programs** | 8 drum programs. Each pad has 4 velocity layers plus tune, level, pan, ADSR, an LP/HP/BP filter, mute (choke) groups 1–32, one-shot or note-on triggering, and poly or mono. |
| **Sequencer** | 16 sequences × 8 tracks, 960 PPQN, 1–64 bars, time signatures from 2/4 to 7/4. Timing Correct from 1/4 to 1/32T with **swing 50–75%**: the second 16th of each 8th is delayed, so 66% gives a triplet feel. Optional **96 PPQN grid** snaps swing to late-80s resolution. |
| **Recording** | **REC** replaces the track on the first pass, then auto-switches to overdub. **OVERDUB** merges. Count-in, metronome, and recording plays live hits immediately without double-triggering their quantized copies. Undo is available. |
| **Performance** | **Note Repeat**: hold a pad to repeat it on the swung grid, with velocity following pressure. **Full Level**. **16 Levels**: the selected pad spread over all 16 pads by velocity (Shift-click for tune). **Erase**: hold it plus a pad while playing to delete that pad's notes. **Track Mute / Pad Mute**: pad views. Sequence changes queue at the loop end. |
| **Sample Edit** | Chop by **Threshold** (spectral-flux transients), **Regions**, **BPM** or **Manual**. Click a slice to hear it. Normalize, reverse, trim, or set a pad to play a single slice. **Convert slices to pads** can also write a sequence that replays the loop and set the tempo from the loop length. |
| **Sampler** | Record from the audio input or **resample** the master output. Threshold-triggered start, input meters, then keep the recording on a pad (or keep and go straight to chopping). |
| **Mixer** | Track strips (each track's program level/pan, mute, solo, meters), a pad mixer for the current bank, and a master fader. |
| **Q-Links** | 4 knobs for the selected pad: tune, cutoff, resonance and decay. MIDI CC 70–73 controls them too. |

### Keyboard

| Keys | Action |
|---|---|
| `1 2 3 4` / `Q W E R` / `A S D F` / `Z X C V` | Pads 13–16 / 9–12 / 5–8 / 1–4, laid out like the 4×4 grid (Shift = full velocity) |
| `Space` / `Return` | Play–Stop / Play from start |
| `.` / `,` | Overdub / Rec arm (press again to punch out) |
| `[` `]` | Bank down / up |
| `N` / `L` | Note Repeat / Full Level |
| `F1`–`F5` | Main, Program Edit, Sample Edit, Sampler, Mixer |
| `Cmd-Z` / `Cmd-S` / `Cmd-Shift-S` / `Cmd-O` / `Cmd-N` | Undo / Save / Save As / Open / New |

Double-press STOP to silence every voice. MIDI pad controllers: notes 36–51 play pads 1–16 of the current bank, and poly or channel aftertouch provides pressure.

## Architecture

```
src/core/      platform-neutral engine (unit-tested, no GUI)
  Project      data model: samples, programs (128 pads x 4 layers), 16 sequences x 8 tracks
  Timing       timing correct + swing math
  Transport    sequencer: playback, loop, record/overdub, note repeat, erase, count-in, seq switching
  Sampler      64-voice pool, layers, Hermite interpolation, ADSR, TPT state-variable filter, choke groups
  Chop         transient detection (FFT spectral flux + adaptive threshold), regions, BPM
  DrumSynth    procedurally synthesized factory kit + demo break
  ProjectIO    open project format
src/engine/    Engine: audio/MIDI device I/O, threading, metronome, sampling, preview, undo
src/ui/        hardware-style JUCE UI (screens, pads, Q-Links, transport)
tests/         JUCE UnitTest suites for all of core/
```

**Real-time rules:**
- The audio thread never allocates or frees memory.
- Pad input and transport commands arrive through lock-free FIFOs.
- The project sits behind a short-held SpinLock. Debug builds assert on accidental nested locking.
- Event vectors are pre-reserved (4096 per track).
- Samples that leave the project are retired, then freed on the message thread once no voice uses them.
- Save captures state under the lock and writes files outside it.

**Project format** (`Name.padlab/`): `project.json` (versioned and human-readable) plus `samples/*.wav` (24-bit).

## Research hooks

- `PerformanceSettings::legacy96` compares 960 PPQN swing with the coarse 96 PPQN grid.
- `timing::quantize` / `swungOffset` implement the swing formula in isolation.
- `Transport` runs headless, so `tests/TransportTests.cpp` shows how to script it for timing experiments.

## Not done yet

- Song mode and a linear arranger.
- Keygroup (chromatic) programs, audio tracks, plugin hosting, per-track insert FX.
- Time-stretch and warp.
- Import of other samplers' program formats.
- MIDI clock in/out, MIDI learn.
- The Save/Open file dialogs haven't been exercised by automated tests (the save/load code underneath has).
