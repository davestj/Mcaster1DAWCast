// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TempoMap.h"

#include <algorithm>
#include <cmath>

namespace dawcast {

TempoMap::TempoMap(QObject* parent)
    : QObject(parent)
{
    // Always have a default event at position 0
    TempoEvent def;
    def.positionSamples = 0;
    def.bpm             = 120.0;
    def.numerator       = 4;
    def.denominator     = 4;
    m_events.append(def);
}

TempoMap::~TempoMap() = default;

// ── Editing ─────────────────────────────────────────────────────────────────

void TempoMap::addTempoChange(int64_t position, double bpm)
{
    // If an event already exists at this position, update it
    for (auto& ev : m_events) {
        if (ev.positionSamples == position) {
            ev.bpm = bpm;
            emit tempoMapChanged();
            return;
        }
    }

    TempoEvent ev;
    ev.positionSamples = position;
    ev.bpm             = bpm;

    // Inherit time signature from the preceding event
    auto ts = timeSignatureAt(position);
    ev.numerator   = ts.first;
    ev.denominator = ts.second;

    m_events.append(ev);
    sortEvents();
    emit tempoMapChanged();
}

void TempoMap::addTimeSignatureChange(int64_t position, int num, int den)
{
    // If an event already exists at this position, update it
    for (auto& ev : m_events) {
        if (ev.positionSamples == position) {
            ev.numerator   = num;
            ev.denominator = den;
            emit tempoMapChanged();
            return;
        }
    }

    TempoEvent ev;
    ev.positionSamples = position;
    ev.numerator       = num;
    ev.denominator     = den;
    ev.bpm             = tempoAt(position);

    m_events.append(ev);
    sortEvents();
    emit tempoMapChanged();
}

void TempoMap::removeEvent(int index)
{
    if (index <= 0 || index >= m_events.size())
        return;  // Never remove the default event at index 0

    m_events.removeAt(index);
    emit tempoMapChanged();
}

// ── Query ───────────────────────────────────────────────────────────────────

double TempoMap::tempoAt(int64_t position) const
{
    double bpm = m_events.first().bpm;
    for (const auto& ev : m_events) {
        if (ev.positionSamples <= position)
            bpm = ev.bpm;
        else
            break;
    }
    return bpm;
}

QPair<int,int> TempoMap::timeSignatureAt(int64_t position) const
{
    int num = m_events.first().numerator;
    int den = m_events.first().denominator;
    for (const auto& ev : m_events) {
        if (ev.positionSamples <= position) {
            num = ev.numerator;
            den = ev.denominator;
        } else {
            break;
        }
    }
    return {num, den};
}

// ── Conversion utilities ────────────────────────────────────────────────────

int64_t TempoMap::beatsToSamples(double beats, int sampleRate) const
{
    // Use the tempo at position 0 for a simple conversion.
    // For variable tempo, a more sophisticated walk through events is needed.
    double bpm = m_events.first().bpm;
    double secondsPerBeat = 60.0 / bpm;
    return static_cast<int64_t>(beats * secondsPerBeat * sampleRate);
}

double TempoMap::samplesToBeats(int64_t samples, int sampleRate) const
{
    // Walk through tempo events to accumulate beats across tempo changes
    double totalBeats = 0.0;
    int64_t prevPos = 0;
    double currentBpm = m_events.first().bpm;

    for (int i = 1; i < m_events.size(); ++i) {
        int64_t evPos = m_events[i].positionSamples;
        if (evPos >= samples) break;

        // Accumulate beats from prevPos to evPos at currentBpm
        double seconds = static_cast<double>(evPos - prevPos) / sampleRate;
        totalBeats += seconds * (currentBpm / 60.0);

        prevPos    = evPos;
        currentBpm = m_events[i].bpm;
    }

    // Remaining samples from last event to target position
    double seconds = static_cast<double>(samples - prevPos) / sampleRate;
    totalBeats += seconds * (currentBpm / 60.0);

    return totalBeats;
}

int64_t TempoMap::barToSamples(int bar, int sampleRate) const
{
    // Walk through events, counting bars
    int64_t pos = 0;
    int barsRemaining = bar;
    double currentBpm = m_events.first().bpm;
    int    num        = m_events.first().numerator;

    for (int i = 1; i < m_events.size() && barsRemaining > 0; ++i) {
        int64_t evPos = m_events[i].positionSamples;
        // How many bars fit between pos and evPos at current tempo?
        double beatsPerBar = static_cast<double>(num);
        double secondsPerBar = beatsPerBar * (60.0 / currentBpm);
        int64_t samplesPerBar = static_cast<int64_t>(secondsPerBar * sampleRate);

        while (barsRemaining > 0 && pos + samplesPerBar <= evPos) {
            pos += samplesPerBar;
            barsRemaining--;
        }

        if (barsRemaining <= 0) break;

        currentBpm = m_events[i].bpm;
        num        = m_events[i].numerator;
        pos        = evPos;
    }

    // Remaining bars at current tempo
    if (barsRemaining > 0) {
        double beatsPerBar = static_cast<double>(num);
        double secondsPerBar = beatsPerBar * (60.0 / currentBpm);
        int64_t samplesPerBar = static_cast<int64_t>(secondsPerBar * sampleRate);
        pos += static_cast<int64_t>(barsRemaining) * samplesPerBar;
    }

    return pos;
}

// ── Private ─────────────────────────────────────────────────────────────────

void TempoMap::sortEvents()
{
    std::sort(m_events.begin(), m_events.end(),
              [](const TempoEvent& a, const TempoEvent& b) {
        return a.positionSamples < b.positionSamples;
    });
}

} // namespace dawcast
