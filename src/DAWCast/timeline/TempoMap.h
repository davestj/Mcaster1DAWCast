// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QPair>
#include <cstdint>

namespace dawcast {

/// A single tempo or time-signature change event on the tempo map.
struct TempoEvent {
    int64_t positionSamples = 0;
    double  bpm             = 120.0;
    int     numerator       = 4;   ///< time signature numerator
    int     denominator     = 4;   ///< time signature denominator
};

/// Manages tempo and time-signature changes over the timeline.
///
/// The first event is always at position 0 and represents the project default.
/// Additional events create tempo/time-signature ramps.  The metronome,
/// MIDI quantise, and beat-grid ruler all read from this map.
class TempoMap : public QObject
{
    Q_OBJECT

public:
    explicit TempoMap(QObject* parent = nullptr);
    ~TempoMap() override;

    // ── Editing ─────────────────────────────────────────────────────────
    void addTempoChange(int64_t position, double bpm);
    void addTimeSignatureChange(int64_t position, int num, int den);
    void removeEvent(int index);

    // ── Query ───────────────────────────────────────────────────────────
    [[nodiscard]] double         tempoAt(int64_t position) const;
    [[nodiscard]] QPair<int,int> timeSignatureAt(int64_t position) const;

    // ── Conversion utilities ────────────────────────────────────────────
    [[nodiscard]] int64_t beatsToSamples(double beats, int sampleRate) const;
    [[nodiscard]] double  samplesToBeats(int64_t samples, int sampleRate) const;
    [[nodiscard]] int64_t barToSamples(int bar, int sampleRate) const;

    // ── Accessors ───────────────────────────────────────────────────────
    [[nodiscard]] QList<TempoEvent> events() const { return m_events; }
    [[nodiscard]] int eventCount() const { return m_events.size(); }

signals:
    void tempoMapChanged();

private:
    void sortEvents();

    /// Guaranteed to have at least one entry (the default at position 0).
    QList<TempoEvent> m_events;
};

} // namespace dawcast
