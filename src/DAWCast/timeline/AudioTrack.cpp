// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioTrack.h"
#include "Clip.h"
#include "../dsp/DspChain.h"
#include <algorithm>

namespace dawcast {

AudioTrack::AudioTrack(QObject* parent)
    : QObject(parent)
{
    // DspChain is created lazily in effectChain()
}

AudioTrack::~AudioTrack() = default;

void AudioTrack::addClip(Clip* clip)
{
    if (!clip) return;
    clip->setParent(this);

    // Insert in sorted order by timeline position
    auto it = std::lower_bound(m_clips.begin(), m_clips.end(), clip,
        [](const Clip* a, const Clip* b) {
            return a->timelinePosition() < b->timelinePosition();
        });
    m_clips.insert(it, clip);
}

void AudioTrack::removeClip(int index)
{
    if (index < 0 || index >= m_clips.size()) return;
    auto* clip = m_clips.takeAt(index);
    clip->deleteLater();
}

int AudioTrack::clipCount() const
{
    return m_clips.size();
}

Clip* AudioTrack::clip(int index) const
{
    if (index < 0 || index >= m_clips.size()) return nullptr;
    return m_clips.at(index);
}

void AudioTrack::setVolume(float db)
{
    m_volumeDb = db;
}

void AudioTrack::setPan(float pan)
{
    m_pan = std::clamp(pan, -1.0f, 1.0f);
}

void AudioTrack::setMuted(bool muted)
{
    m_muted = muted;
}

void AudioTrack::setSolo(bool solo)
{
    m_solo = solo;
}

void AudioTrack::setRecordArmed(bool armed)
{
    m_recordArmed = armed;
}

DspChain* AudioTrack::effectChain()
{
    if (!m_effectChain) {
        m_effectChain = new DspChain(this);
    }
    return m_effectChain;
}

} // namespace dawcast
