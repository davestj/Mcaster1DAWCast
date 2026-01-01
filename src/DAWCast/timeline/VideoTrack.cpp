// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoTrack.h"
#include "Clip.h"
#include "../vfx/VideoEffectChain.h"
#include <algorithm>

namespace dawcast {

VideoTrack::VideoTrack(QObject* parent)
    : QObject(parent)
{
    // VideoEffectChain is created lazily in videoEffectChain()
}

VideoTrack::~VideoTrack() = default;

void VideoTrack::addClip(Clip* clip)
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

void VideoTrack::removeClip(int index)
{
    if (index < 0 || index >= m_clips.size()) return;
    auto* clip = m_clips.takeAt(index);
    clip->deleteLater();
}

int VideoTrack::clipCount() const
{
    return m_clips.size();
}

Clip* VideoTrack::clip(int index) const
{
    if (index < 0 || index >= m_clips.size()) return nullptr;
    return m_clips.at(index);
}

void VideoTrack::setVisible(bool visible)
{
    m_visible = visible;
}

void VideoTrack::setOpacity(float opacity)
{
    m_opacity = std::clamp(opacity, 0.0f, 1.0f);
}

void VideoTrack::setMuted(bool muted)
{
    m_muted = muted;
}

void VideoTrack::setSolo(bool solo)
{
    m_solo = solo;
}

VideoEffectChain* VideoTrack::videoEffectChain()
{
    if (!m_videoEffectChain) {
        m_videoEffectChain = new VideoEffectChain(this);
    }
    return m_videoEffectChain;
}

} // namespace dawcast
