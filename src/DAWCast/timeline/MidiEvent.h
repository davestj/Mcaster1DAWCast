// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>

namespace dawcast {

struct MidiEvent {
    enum Type { NoteOn, NoteOff, ControlChange, ProgramChange, PitchBend, Aftertouch };

    Type     type       = NoteOn;
    int64_t  tick       = 0;       // position in MIDI ticks
    uint8_t  channel    = 0;       // 0-15
    uint8_t  note       = 60;      // 0-127 (for NoteOn/NoteOff)
    uint8_t  velocity   = 100;     // 0-127
    uint8_t  controller = 0;       // CC number (for ControlChange)
    uint8_t  value      = 0;       // CC value
    int16_t  pitchBend  = 0;       // -8192 to 8191

    // Convenience: duration in ticks for note events (NoteOn to paired NoteOff)
    int64_t  durationTicks = 480;  // default 1 beat at 480 PPQN
};

} // namespace dawcast
