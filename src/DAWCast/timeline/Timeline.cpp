// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Timeline.h"
#include "AudioTrack.h"
#include "VideoTrack.h"
#include "MidiTrack.h"
#include "MidiClip.h"
#include "Clip.h"
#include "TrackGroup.h"
#include "TempoMap.h"

#include <algorithm>

namespace dawcast {

Timeline::Timeline(QObject* parent)
    : QObject(parent)
    , m_tempoMap(new TempoMap(this))
{
}

Timeline::~Timeline() = default;

AudioTrack* Timeline::addAudioTrack()
{
    auto* track = new AudioTrack(this);
    track->setName(QStringLiteral("Audio %1").arg(m_tracks.size() + 1));
    m_tracks.append(track);
    emit trackAdded(m_tracks.size() - 1);
    return track;
}

VideoTrack* Timeline::addVideoTrack()
{
    auto* track = new VideoTrack(this);
    track->setName(QStringLiteral("Video %1").arg(m_tracks.size() + 1));
    m_tracks.append(track);
    emit trackAdded(m_tracks.size() - 1);
    return track;
}

MidiTrack* Timeline::addMidiTrack()
{
    auto* track = new MidiTrack(this);
    track->setName(QStringLiteral("MIDI %1").arg(m_tracks.size() + 1));
    m_tracks.append(track);
    emit trackAdded(m_tracks.size() - 1);
    return track;
}

void Timeline::removeTrack(int index)
{
    if (index < 0 || index >= m_tracks.size()) return;
    auto* track = m_tracks.takeAt(index);

    // Remove from any group it belongs to
    for (auto* group : m_trackGroups) {
        group->removeTrack(track);
    }

    track->deleteLater();
    emit trackRemoved(index);
}

void Timeline::moveTrack(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_tracks.size()) return;
    if (toIndex < 0 || toIndex >= m_tracks.size()) return;
    if (fromIndex == toIndex) return;

    m_tracks.move(fromIndex, toIndex);
    emit trackMoved(fromIndex, toIndex);
}

int Timeline::trackCount() const
{
    return m_tracks.size();
}

QObject* Timeline::track(int index) const
{
    if (index < 0 || index >= m_tracks.size()) return nullptr;
    return m_tracks.at(index);
}

int64_t Timeline::duration() const
{
    int64_t maxEnd = 0;

    for (const auto* trackObj : m_tracks) {
        // Check if this is an AudioTrack
        const auto* audioTrack = qobject_cast<const AudioTrack*>(trackObj);
        if (audioTrack) {
            for (int i = 0; i < audioTrack->clipCount(); ++i) {
                const Clip* c = audioTrack->clip(i);
                if (c) {
                    int64_t clipEnd = c->timelinePosition() + c->duration();
                    maxEnd = std::max(maxEnd, clipEnd);
                }
            }
            continue;
        }

        // Check if this is a VideoTrack
        const auto* videoTrack = qobject_cast<const VideoTrack*>(trackObj);
        if (videoTrack) {
            for (int i = 0; i < videoTrack->clipCount(); ++i) {
                const Clip* c = videoTrack->clip(i);
                if (c) {
                    int64_t clipEnd = c->timelinePosition() + c->duration();
                    maxEnd = std::max(maxEnd, clipEnd);
                }
            }
            continue;
        }

        // Check if this is a MidiTrack
        const auto* midiTrack = qobject_cast<const MidiTrack*>(trackObj);
        if (midiTrack) {
            for (int i = 0; i < midiTrack->clipCount(); ++i) {
                const MidiClip* mc = midiTrack->clip(i);
                if (mc) {
                    int64_t clipEnd = mc->timelinePosition()
                                    + MidiClip::ticksToSamples(mc->durationTicks(), m_bpm, m_sampleRate);
                    maxEnd = std::max(maxEnd, clipEnd);
                }
            }
        }
    }

    return maxEnd;
}

void Timeline::setPlayhead(int64_t samples)
{
    // Clamp to [0, duration]
    int64_t dur = duration();
    samples = std::max(int64_t(0), std::min(samples, dur));

    if (m_playhead != samples) {
        m_playhead = samples;
        emit playheadChanged(m_playhead);
    }
}

int64_t Timeline::playhead() const
{
    return m_playhead;
}

// ── Marker system ─────────────────────────────────────────────────────────

void Timeline::addMarker(const Marker& marker)
{
    m_markers.append(marker);
    sortMarkers();
    emit markersChanged();
}

void Timeline::removeMarker(int index)
{
    if (index < 0 || index >= m_markers.size()) return;
    m_markers.removeAt(index);
    emit markersChanged();
}

void Timeline::setMarker(int index, const Marker& marker)
{
    if (index < 0 || index >= m_markers.size()) return;
    m_markers[index] = marker;
    emit markersChanged();
}

static const Marker kNullMarker;

const Marker& Timeline::marker(int index) const
{
    if (index < 0 || index >= m_markers.size()) return kNullMarker;
    return m_markers.at(index);
}

void Timeline::sortMarkers()
{
    std::sort(m_markers.begin(), m_markers.end(),
              [](const Marker& a, const Marker& b) {
        return a.position() < b.position();
    });
}

int Timeline::previousMarkerIndex(int64_t position) const
{
    int result = -1;
    for (int i = 0; i < m_markers.size(); ++i) {
        if (m_markers[i].position() < position) {
            result = i;
        } else {
            break;  // markers are sorted
        }
    }
    return result;
}

int Timeline::nextMarkerIndex(int64_t position) const
{
    for (int i = 0; i < m_markers.size(); ++i) {
        if (m_markers[i].position() > position) {
            return i;
        }
    }
    return -1;
}

// ── Loop region ───────────────────────────────────────────────────────────

void Timeline::setLoopStart(int64_t samples)
{
    if (m_loopStart != samples) {
        m_loopStart = samples;
        emit loopChanged();
    }
}

void Timeline::setLoopEnd(int64_t samples)
{
    if (m_loopEnd != samples) {
        m_loopEnd = samples;
        emit loopChanged();
    }
}

void Timeline::setLoopEnabled(bool enabled)
{
    if (m_loopEnabled != enabled) {
        m_loopEnabled = enabled;
        emit loopChanged();
    }
}

// ── Track groups (folders) ────────────────────────────────────────────────

TrackGroup* Timeline::addTrackGroup(const QString& name)
{
    auto* group = new TrackGroup(name, this);
    m_trackGroups.append(group);
    int idx = m_trackGroups.size() - 1;
    emit trackGroupAdded(idx);
    return group;
}

void Timeline::removeTrackGroup(int index)
{
    if (index < 0 || index >= m_trackGroups.size()) return;
    auto* group = m_trackGroups.takeAt(index);
    group->deleteLater();
    emit trackGroupRemoved(index);
}

TrackGroup* Timeline::trackGroup(int index) const
{
    if (index < 0 || index >= m_trackGroups.size()) return nullptr;
    return m_trackGroups.at(index);
}

void Timeline::moveTrackToGroup(int trackIndex, TrackGroup* group)
{
    QObject* trackObj = track(trackIndex);
    if (!trackObj) return;

    // Remove from any existing group
    for (auto* g : m_trackGroups) {
        g->removeTrack(trackObj);
    }

    // Add to new group
    if (group) {
        group->addTrack(trackObj);
    }

    emit trackGroupChanged();
}

TrackGroup* Timeline::groupForTrack(QObject* trackObj) const
{
    for (auto* g : m_trackGroups) {
        if (g->containsTrack(trackObj)) {
            return g;
        }
    }
    return nullptr;
}

} // namespace dawcast
