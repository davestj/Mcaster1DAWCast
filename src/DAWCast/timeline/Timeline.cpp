// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Timeline.h"
#include "AudioTrack.h"
#include "VideoTrack.h"
#include "MidiTrack.h"
#include "MidiClip.h"
#include "Clip.h"

#include <algorithm>

namespace dawcast {

Timeline::Timeline(QObject* parent)
    : QObject(parent)
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
    track->deleteLater();
    emit trackRemoved(index);
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

} // namespace dawcast
