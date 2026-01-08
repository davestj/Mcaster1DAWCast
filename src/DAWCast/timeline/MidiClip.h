// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <cstdint>

#include "MidiEvent.h"

namespace dawcast {

class MidiClip : public QObject
{
    Q_OBJECT

public:
    explicit MidiClip(QObject* parent = nullptr);
    ~MidiClip() override;

    void addEvent(const MidiEvent& event);
    void removeEvent(int index);
    [[nodiscard]] const QList<MidiEvent>& events() const;
    [[nodiscard]] int eventCount() const;

    [[nodiscard]] int64_t durationTicks() const { return m_durationTicks; }
    void setDurationTicks(int64_t ticks);

    // Timeline position in samples (where the clip sits on the timeline)
    [[nodiscard]] int64_t timelinePosition() const { return m_timelinePosition; }
    void setTimelinePosition(int64_t samples);

    // Convert between ticks and samples given tempo
    static int64_t ticksToSamples(int64_t ticks, double bpm, int sampleRate, int ticksPerBeat = 480);
    static int64_t samplesToTicks(int64_t samples, double bpm, int sampleRate, int ticksPerBeat = 480);

    // Duration in samples (depends on tempo)
    [[nodiscard]] int64_t durationSamples(double bpm, int sampleRate, int ticksPerBeat = 480) const;

signals:
    void eventAdded(int index);
    void eventRemoved(int index);
    void durationChanged();

private:
    QList<MidiEvent> m_events;
    int64_t m_durationTicks     = 1920;  // 4 beats at 480 PPQN (1 bar in 4/4)
    int64_t m_timelinePosition  = 0;
};

} // namespace dawcast
