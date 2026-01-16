// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QLabel>
#include <QDoubleSpinBox>
#include <cstdint>

namespace dawcast::widgets {

class BevelButton;

class TransportBar : public QWidget {
    Q_OBJECT

public:
    explicit TransportBar(QWidget* parent = nullptr);
    ~TransportBar() override;

    void setPlaying(bool playing);
    void setRecording(bool recording);
    void setPosition(int64_t samples, int sampleRate);
    void setDuration(int64_t samples, int sampleRate);

    /// Flash the beat indicator. Call from Metronome::beat signal
    /// with Qt::QueuedConnection.
    void flashBeat(int beatNumber, bool isDownbeat);

signals:
    void rewindClicked();
    void playClicked();
    void pauseClicked();
    void stopClicked();
    void recordClicked();
    void fastForwardClicked();
    void loopToggled(bool enabled);
    void metronomeToggled(bool enabled);
    void tempoChanged(double bpm);

private:
    void updateTimeDisplay();

    bool    m_playing    = false;
    bool    m_recording  = false;
    int64_t m_position   = 0;
    int64_t m_duration   = 0;
    int     m_sampleRate = 44100;

    BevelButton* m_rewindBtn  = nullptr;
    BevelButton* m_playBtn    = nullptr;
    BevelButton* m_pauseBtn   = nullptr;
    BevelButton* m_stopBtn    = nullptr;
    BevelButton* m_recordBtn  = nullptr;
    BevelButton* m_ffBtn      = nullptr;
    BevelButton* m_loopBtn       = nullptr;
    BevelButton* m_metronomeBtn  = nullptr;

    QDoubleSpinBox* m_tempoSpin  = nullptr;
    QLabel*         m_beatIndicator = nullptr;

    QLabel* m_timeDisplay = nullptr;
};

} // namespace dawcast::widgets
