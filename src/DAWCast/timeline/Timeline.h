// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <cstdint>

#include "Marker.h"

namespace dawcast {

class AudioTrack;
class VideoTrack;
class MidiTrack;
class TrackGroup;

class Timeline : public QObject
{
    Q_OBJECT

public:
    explicit Timeline(QObject* parent = nullptr);
    ~Timeline() override;

    AudioTrack* addAudioTrack();
    VideoTrack* addVideoTrack();
    MidiTrack*  addMidiTrack();
    void removeTrack(int index);

    [[nodiscard]] int      trackCount() const;
    [[nodiscard]] QObject* track(int index) const;
    [[nodiscard]] int64_t  duration() const;

    void    setPlayhead(int64_t samples);
    int64_t playhead() const;

    void setSampleRate(int rate) { m_sampleRate = rate; }
    [[nodiscard]] int sampleRate() const { return m_sampleRate; }

    void   setTempo(double bpm) { m_bpm = bpm; }
    [[nodiscard]] double tempo() const { return m_bpm; }

    // ── Punch-In / Punch-Out markers ──────────────────────────────────
    void setPunchIn(int64_t samples)  { m_punchIn = samples; }
    void setPunchOut(int64_t samples) { m_punchOut = samples; }
    [[nodiscard]] int64_t punchIn()  const { return m_punchIn; }
    [[nodiscard]] int64_t punchOut() const { return m_punchOut; }

    void setPunchInEnabled(bool enabled)  { m_punchInEnabled = enabled; }
    void setPunchOutEnabled(bool enabled) { m_punchOutEnabled = enabled; }
    [[nodiscard]] bool punchInEnabled()  const { return m_punchInEnabled; }
    [[nodiscard]] bool punchOutEnabled() const { return m_punchOutEnabled; }

    // ── Marker system ─────────────────────────────────────────────────
    void addMarker(const Marker& marker);
    void removeMarker(int index);
    void setMarker(int index, const Marker& marker);
    [[nodiscard]] int markerCount() const { return m_markers.size(); }
    [[nodiscard]] const Marker& marker(int index) const;
    [[nodiscard]] QList<Marker>& markers() { return m_markers; }
    [[nodiscard]] const QList<Marker>& markers() const { return m_markers; }

    /// Sort markers by position (ascending).
    void sortMarkers();

    /// Find the index of the marker immediately before @a position,
    /// or -1 if none exists.
    [[nodiscard]] int previousMarkerIndex(int64_t position) const;

    /// Find the index of the marker immediately after @a position,
    /// or -1 if none exists.
    [[nodiscard]] int nextMarkerIndex(int64_t position) const;

    // ── Track groups (folders) ────────────────────────────────────────
    TrackGroup* addTrackGroup(const QString& name);
    void removeTrackGroup(int index);
    [[nodiscard]] QList<TrackGroup*> trackGroups() const { return m_trackGroups; }
    [[nodiscard]] int trackGroupCount() const { return m_trackGroups.size(); }
    [[nodiscard]] TrackGroup* trackGroup(int index) const;

    /// Move a track into a group (or out of any group if group is nullptr).
    void moveTrackToGroup(int trackIndex, TrackGroup* group);

    /// Find which group a track belongs to (nullptr if ungrouped).
    [[nodiscard]] TrackGroup* groupForTrack(QObject* track) const;

    // ── Loop region ───────────────────────────────────────────────────
    void setLoopStart(int64_t samples);
    void setLoopEnd(int64_t samples);
    void setLoopEnabled(bool enabled);

    [[nodiscard]] int64_t loopStart()   const { return m_loopStart; }
    [[nodiscard]] int64_t loopEnd()     const { return m_loopEnd; }
    [[nodiscard]] bool    loopEnabled() const { return m_loopEnabled; }

signals:
    void trackAdded(int index);
    void trackRemoved(int index);
    void playheadChanged(int64_t samples);
    void markersChanged();
    void loopChanged();
    void trackGroupAdded(int index);
    void trackGroupRemoved(int index);
    void trackGroupChanged();

private:
    QList<QObject*> m_tracks;  // Mix of AudioTrack*, VideoTrack*, and MidiTrack*
    int64_t m_playhead   = 0;
    int     m_sampleRate = 48000;
    double  m_bpm        = 120.0;

    // Punch-In / Punch-Out
    int64_t m_punchIn        = 0;
    int64_t m_punchOut       = 0;
    bool    m_punchInEnabled = false;
    bool    m_punchOutEnabled = false;

    // Markers
    QList<Marker> m_markers;

    // Track groups
    QList<TrackGroup*> m_trackGroups;

    // Loop region
    int64_t m_loopStart   = 0;
    int64_t m_loopEnd     = 0;
    bool    m_loopEnabled = false;
};

} // namespace dawcast
