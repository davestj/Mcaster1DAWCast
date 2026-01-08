// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MidiClip.h"

#include <algorithm>
#include <cmath>

namespace dawcast {

MidiClip::MidiClip(QObject* parent)
    : QObject(parent)
{
}

MidiClip::~MidiClip() = default;

void MidiClip::addEvent(const MidiEvent& event)
{
    // Insert in sorted order by tick position
    auto it = std::lower_bound(m_events.begin(), m_events.end(), event,
        [](const MidiEvent& a, const MidiEvent& b) {
            return a.tick < b.tick;
        });
    int index = static_cast<int>(it - m_events.begin());
    m_events.insert(it, event);
    emit eventAdded(index);
}

void MidiClip::removeEvent(int index)
{
    if (index < 0 || index >= m_events.size()) return;
    m_events.removeAt(index);
    emit eventRemoved(index);
}

const QList<MidiEvent>& MidiClip::events() const
{
    return m_events;
}

int MidiClip::eventCount() const
{
    return m_events.size();
}

void MidiClip::setDurationTicks(int64_t ticks)
{
    if (ticks > 0 && ticks != m_durationTicks) {
        m_durationTicks = ticks;
        emit durationChanged();
    }
}

void MidiClip::setTimelinePosition(int64_t samples)
{
    m_timelinePosition = samples;
}

int64_t MidiClip::ticksToSamples(int64_t ticks, double bpm, int sampleRate, int ticksPerBeat)
{
    if (bpm <= 0.0 || ticksPerBeat <= 0) return 0;
    // seconds = ticks / ticksPerBeat / (bpm / 60)
    double seconds = static_cast<double>(ticks) / ticksPerBeat / (bpm / 60.0);
    return static_cast<int64_t>(std::round(seconds * sampleRate));
}

int64_t MidiClip::samplesToTicks(int64_t samples, double bpm, int sampleRate, int ticksPerBeat)
{
    if (sampleRate <= 0 || bpm <= 0.0) return 0;
    double seconds = static_cast<double>(samples) / sampleRate;
    // ticks = seconds * (bpm / 60) * ticksPerBeat
    return static_cast<int64_t>(std::round(seconds * (bpm / 60.0) * ticksPerBeat));
}

int64_t MidiClip::durationSamples(double bpm, int sampleRate, int ticksPerBeat) const
{
    return ticksToSamples(m_durationTicks, bpm, sampleRate, ticksPerBeat);
}

} // namespace dawcast
