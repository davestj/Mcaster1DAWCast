// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QList>
#include <cstdint>

namespace dawcast {

class AudioTrack;
class VideoTrack;
class MidiTrack;

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

signals:
    void trackAdded(int index);
    void trackRemoved(int index);
    void playheadChanged(int64_t samples);

private:
    QList<QObject*> m_tracks;  // Mix of AudioTrack*, VideoTrack*, and MidiTrack*
    int64_t m_playhead   = 0;
    int     m_sampleRate = 48000;
    double  m_bpm        = 120.0;
};

} // namespace dawcast
