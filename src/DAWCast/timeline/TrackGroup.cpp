// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TrackGroup.h"
#include "AudioTrack.h"

namespace dawcast {

TrackGroup::TrackGroup(const QString& name, QObject* parent)
    : QObject(parent)
    , m_name(name)
{
}

TrackGroup::~TrackGroup() = default;

void TrackGroup::setName(const QString& name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged(m_name);
    }
}

void TrackGroup::setColor(QColor color)
{
    if (m_color != color) {
        m_color = color;
        emit colorChanged(m_color);
    }
}

void TrackGroup::setCollapsed(bool collapsed)
{
    if (m_collapsed != collapsed) {
        m_collapsed = collapsed;
        emit collapsedChanged(m_collapsed);
    }
}

void TrackGroup::addTrack(QObject* track)
{
    if (!track || m_tracks.contains(track)) return;
    m_tracks.append(track);
    emit tracksChanged();
}

void TrackGroup::removeTrack(QObject* track)
{
    if (m_tracks.removeOne(track)) {
        emit tracksChanged();
    }
}

bool TrackGroup::containsTrack(QObject* track) const
{
    return m_tracks.contains(track);
}

void TrackGroup::setMuted(bool muted)
{
    if (m_muted != muted) {
        m_muted = muted;

        // Propagate mute state to all child audio tracks
        for (auto* trackObj : m_tracks) {
            auto* audioTrack = qobject_cast<AudioTrack*>(trackObj);
            if (audioTrack) {
                audioTrack->setMuted(muted);
            }
        }

        emit mutedChanged(m_muted);
    }
}

void TrackGroup::setSolo(bool solo)
{
    if (m_solo != solo) {
        m_solo = solo;

        // Propagate solo state to all child audio tracks
        for (auto* trackObj : m_tracks) {
            auto* audioTrack = qobject_cast<AudioTrack*>(trackObj);
            if (audioTrack) {
                audioTrack->setSolo(solo);
            }
        }

        emit soloChanged(m_solo);
    }
}

} // namespace dawcast
