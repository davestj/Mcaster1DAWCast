// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QMap>
#include <QPair>
#include "AudioBus.h"

namespace dawcast {

/// Manages all audio buses and routes track audio between them.
///
/// The default configuration has a single Master bus (index 0).
/// Sub-group buses feed into the master; aux/send buses are parallel
/// paths that tracks can send a copy of their audio to.
///
/// Thread safety:
///   - processAll() runs on the audio thread.
///   - add/remove/set methods are called from the GUI thread while
///     playback is stopped or paused.
class BusRouter : public QObject
{
    Q_OBJECT

public:
    explicit BusRouter(QObject* parent = nullptr);
    ~BusRouter() override;

    /// The master bus is always at index 0 and cannot be removed.
    [[nodiscard]] AudioBus* masterBus() const;

    /// Create a new bus and return a pointer to it.
    AudioBus* addBus(const QString& name, AudioBus::BusType type);

    /// Remove a bus by index. The master bus (index 0) cannot be removed.
    void removeBus(int index);

    /// Access a bus by index.
    [[nodiscard]] AudioBus* bus(int index) const;

    /// Find a bus by name (returns nullptr if not found).
    [[nodiscard]] AudioBus* busByName(const QString& name) const;

    /// Total number of buses (including master).
    [[nodiscard]] int busCount() const;

    /// All buses in order.
    [[nodiscard]] QList<AudioBus*> buses() const { return m_buses; }

    // ── Per-track routing ─────────────────────────────────────────────

    /// Set the primary output bus for a track (default: master).
    void setTrackOutput(int trackIndex, AudioBus* bus);

    /// Get the primary output bus for a track.
    [[nodiscard]] AudioBus* trackOutput(int trackIndex) const;

    /// Configure a send from a track to an aux/send bus at a given level
    /// (0.0 = off, 1.0 = unity).
    void setTrackSend(int trackIndex, AudioBus* sendBus, float level);

    /// Remove a send from a track.
    void removeTrackSend(int trackIndex, AudioBus* sendBus);

    /// Get all sends for a track as (bus, level) pairs.
    [[nodiscard]] QList<QPair<AudioBus*, float>> trackSends(int trackIndex) const;

    // ── Audio-thread processing ───────────────────────────────────────

    /// Clear all bus buffers in preparation for a new block.
    void clearAll(int frames, int channels);

    /// Route a track's processed audio to its assigned bus and sends.
    /// Called once per track after the track's DSP chain has been applied.
    void routeTrack(int trackIndex, const float* buffer, int frames, int channels);

    /// Process all buses: sub-groups first (they feed master), then master.
    /// Call after all tracks have been routed via routeTrack().
    void processAll(int frames, int channels);

signals:
    void busAdded(int index);
    void busRemoved(int index);
    void routingChanged();

private:
    QList<AudioBus*> m_buses;

    // Per-track output routing: trackIndex -> bus pointer (default: master)
    QMap<int, AudioBus*> m_trackOutputs;

    // Per-track sends: trackIndex -> list of (bus, level)
    QMap<int, QList<QPair<AudioBus*, float>>> m_trackSends;
};

} // namespace dawcast
