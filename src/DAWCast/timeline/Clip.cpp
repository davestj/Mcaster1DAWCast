// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Clip.h"

namespace dawcast {

Clip::Clip(QObject* parent)
    : QObject(parent)
{
}

Clip::~Clip() = default;

void Clip::setSourcePath(const QString& path)
{
    m_sourcePath = path;
}

void Clip::setSourceIn(int64_t samples)
{
    m_sourceIn = samples;
}

void Clip::setSourceOut(int64_t samples)
{
    m_sourceOut = samples;
}

void Clip::setTimelinePosition(int64_t samples)
{
    m_timelinePosition = samples;
}

void Clip::setGain(float gain)
{
    m_gain = gain;
}

void Clip::setFadeIn(int64_t samples)
{
    m_fadeIn = samples;
}

void Clip::setFadeOut(int64_t samples)
{
    m_fadeOut = samples;
}

} // namespace dawcast
