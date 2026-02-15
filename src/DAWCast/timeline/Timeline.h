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

    // ── Punch-In / Punch-Out markers ──────────────────────────────────
    void setPunchIn(int64_t samples)  { m_punchIn = samples; }
    void setPunchOut(int64_t samples) { m_punchOut = samples; }
    [[nodiscard]] int64_t punchIn()  const { return m_punchIn; }
    [[nodiscard]] int64_t punchOut() const { return m_punchOut; }

    void setPunchInEnabled(bool enabled)  { m_punchInEnabled = enabled; }
    void setPunchOutEnabled(bool enabled) { m_punchOutEnabled = enabled; }
    [[nodiscard]] bool punchInEnabled()  const { return m_punchInEnabled; }
    [[nodiscard]] bool punchOutEnabled() const { return m_punchOutEnabled; }

signals:
    void trackAdded(int index);
    void trackRemoved(int index);
    void playheadChanged(int64_t samples);

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
};

} // namespace dawcast
