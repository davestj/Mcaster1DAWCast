// DAWCast Editor Studio — Player Controls
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>
#include <cstdint>

class QToolButton;
class QSlider;
class QLabel;

namespace dawcast::editor {

class ForensicWaveformView;

/// Compact player control strip for DAWCast Editor Studio.
/// Hosts: rewind, play, pause, stop, fast-forward, scrubber, time display.
/// Sits beneath the waveform view and drives ForensicWaveformView playback.
class PlayerControls : public QWidget
{
    Q_OBJECT

public:
    explicit PlayerControls(ForensicWaveformView* view, QWidget* parent = nullptr);
    ~PlayerControls() override = default;

public slots:
    void handleFileLoaded(const QString& path, int64_t frames, int channels, int sampleRate);
    void handlePosition(int64_t samplePosition);
    void handlePlayState(bool playing);

private:
    QToolButton* makeIconButton(const QString& iconName, const QString& tip);
    QString findIcon(const QString& filename) const;
    QString formatTime(int64_t samples) const;

    ForensicWaveformView* m_view = nullptr;

    QToolButton* m_btnRev   = nullptr;
    QToolButton* m_btnPlay  = nullptr;
    QToolButton* m_btnPause = nullptr;
    QToolButton* m_btnStop  = nullptr;
    QToolButton* m_btnFf    = nullptr;

    QSlider* m_scrubber = nullptr;
    QLabel*  m_timeNow  = nullptr;
    QLabel*  m_timeAll  = nullptr;

    int64_t m_total      = 0;
    int     m_sampleRate = 44100;
    bool    m_userScrub  = false;
};

} // namespace dawcast::editor
