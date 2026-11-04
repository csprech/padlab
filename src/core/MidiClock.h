#pragma once

// MIDI clock out: 24 pulses per quarter note plus Start / Continue / Stop and Song Position
// Pointer, derived from the transport's running tick count. Pure logic (no I/O) so it can be
// tested; the engine drives it from a 1 ms timer thread and sends the bytes.

#include <cstdint>
#include <vector>

namespace padlab
{
class MidiClock
{
public:
    static constexpr int kTicksPerClock = 960 / 24; // 960 PPQN -> 24 PPQN

    enum class Msg : uint8_t { Clock = 0xF8, Start = 0xFA, Continue = 0xFB, Stop = 0xFC, SongPosition = 0xF2 };
    struct Event
    {
        Msg type;
        int songPosition = 0; // in MIDI beats (16ths), for SongPosition
    };

    /** playing: transport running; ticks: running position (never wraps while playing, i.e. it keeps
        counting across sequence loops); startTicks: where playback started inside the sequence. */
    void update (bool playing, double ticks, double startTicks, std::vector<Event>& out)
    {
        if (playing && ! wasPlaying)
        {
            // from the top: Start. From elsewhere: tell the slave where, then Continue.
            if (startTicks < 1.0)
                out.push_back ({ Msg::Start });
            else
            {
                out.push_back ({ Msg::SongPosition, (int) (startTicks / (960 / 4)) });
                out.push_back ({ Msg::Continue });
            }
            sent = 0;
            origin = ticks;
        }
        else if (! playing && wasPlaying)
            out.push_back ({ Msg::Stop });
        wasPlaying = playing;
        if (! playing)
            return;

        // one clock per 40 ticks since playback started; the first goes out right after Start
        const auto due = (int64_t) ((ticks - origin) / kTicksPerClock) + 1;
        for (int burst = 0; sent < due && burst < 64; ++sent, ++burst) // never flood after a stall
            out.push_back ({ Msg::Clock });
    }

private:
    bool wasPlaying = false;
    int64_t sent = 0;
    double origin = 0.0;
};
} // namespace padlab
