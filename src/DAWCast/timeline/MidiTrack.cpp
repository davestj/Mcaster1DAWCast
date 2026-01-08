// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MidiTrack.h"
#include "MidiClip.h"

#include <algorithm>

namespace dawcast {

MidiTrack::MidiTrack(QObject* parent)
    : QObject(parent)
{
}

MidiTrack::~MidiTrack() = default;

void MidiTrack::addClip(MidiClip* clip)
{
    if (!clip) return;
    clip->setParent(this);

    // Insert in sorted order by timeline position
    auto it = std::lower_bound(m_clips.begin(), m_clips.end(), clip,
        [](const MidiClip* a, const MidiClip* b) {
            return a->timelinePosition() < b->timelinePosition();
        });
    m_clips.insert(it, clip);
}

void MidiTrack::removeClip(int index)
{
    if (index < 0 || index >= m_clips.size()) return;
    auto* clip = m_clips.takeAt(index);
    clip->deleteLater();
}

MidiClip* MidiTrack::clip(int index) const
{
    if (index < 0 || index >= m_clips.size()) return nullptr;
    return m_clips.at(index);
}

int MidiTrack::clipCount() const
{
    return m_clips.size();
}

void MidiTrack::setChannel(int ch)
{
    m_channel = qBound(0, ch, 15);
}

void MidiTrack::setInstrumentName(const QString& name)
{
    m_instrumentName = name;
}

void MidiTrack::setMuted(bool muted)
{
    m_muted = muted;
}

void MidiTrack::setSolo(bool solo)
{
    m_solo = solo;
}

void MidiTrack::setRecordArmed(bool armed)
{
    m_recordArmed = armed;
}

} // namespace dawcast
