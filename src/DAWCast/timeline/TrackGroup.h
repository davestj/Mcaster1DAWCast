// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QColor>

namespace dawcast {

/// A folder-like grouping of tracks that can be collapsed / expanded
/// in the timeline UI.  Analogous to folder tracks in Cubase or
/// track stacks in Logic Pro.
///
/// The group does not own the track objects -- they remain children of
/// the Timeline.  The group only holds pointers for logical grouping.
class TrackGroup : public QObject
{
    Q_OBJECT

public:
    explicit TrackGroup(const QString& name, QObject* parent = nullptr);
    ~TrackGroup() override;

    [[nodiscard]] QString name() const { return m_name; }
    void setName(const QString& name);

    [[nodiscard]] QColor color() const { return m_color; }
    void setColor(QColor color);

    [[nodiscard]] bool isCollapsed() const { return m_collapsed; }
    void setCollapsed(bool collapsed);

    /// Add a track to this group (AudioTrack, VideoTrack, or MidiTrack).
    void addTrack(QObject* track);

    /// Remove a track from this group.
    void removeTrack(QObject* track);

    /// All tracks in this group, in order.
    [[nodiscard]] QList<QObject*> tracks() const { return m_tracks; }

    [[nodiscard]] int trackCount() const { return m_tracks.size(); }

    /// Check if a track belongs to this group.
    [[nodiscard]] bool containsTrack(QObject* track) const;

    // ── Group-level mute/solo (affects all child tracks) ──────────────

    void setMuted(bool muted);
    [[nodiscard]] bool isMuted() const { return m_muted; }

    void setSolo(bool solo);
    [[nodiscard]] bool isSolo() const { return m_solo; }

signals:
    void collapsedChanged(bool collapsed);
    void tracksChanged();
    void nameChanged(const QString& name);
    void colorChanged(QColor color);
    void mutedChanged(bool muted);
    void soloChanged(bool solo);

private:
    QString        m_name;
    QColor         m_color     = QColor(120, 140, 180);
    bool           m_collapsed = false;
    bool           m_muted     = false;
    bool           m_solo      = false;
    QList<QObject*> m_tracks;
};

} // namespace dawcast
