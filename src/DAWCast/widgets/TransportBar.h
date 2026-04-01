// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QElapsedTimer>
#include <QVector>
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

    /// Update the selection display in the transport bar.
    /// Pass start == end to clear the selection display.
    void setSelection(int64_t startSamples, int64_t endSamples, int sampleRate);

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
    void zoomChanged(int pixelsPerSecond);
    void automationWriteToggled(bool enabled);
    void crossfadeModeChanged(int mode);
    void punchToggled(bool enabled);
    void busesClicked();
    void prevMarkerClicked();
    void nextMarkerClicked();
    void markerViewToggled(bool enabled);
    void listViewToggled(bool enabled);
    void gridViewToggled(bool enabled);
    void snapModeChanged(int mode);
    void rippleModeToggled(bool enabled);

private slots:
    void onTapTempo();

private:
    void updateTimeDisplay();
    void updateZoomLabel(int value);

    bool    m_playing    = false;
    bool    m_recording  = false;
    int64_t m_position   = 0;
    int64_t m_duration   = 0;
    int     m_sampleRate = 44100;

    // Transport buttons
    BevelButton* m_rewindBtn  = nullptr;
    BevelButton* m_playBtn    = nullptr;
    BevelButton* m_pauseBtn   = nullptr;
    BevelButton* m_stopBtn    = nullptr;
    BevelButton* m_recordBtn  = nullptr;
    BevelButton* m_ffBtn      = nullptr;
    BevelButton* m_loopBtn       = nullptr;
    BevelButton* m_metronomeBtn  = nullptr;

    // Tempo
    QDoubleSpinBox* m_tempoSpin     = nullptr;
    QLabel*         m_beatIndicator = nullptr;
    BevelButton*    m_tapBtn        = nullptr;

    // TAP tempo state
    QElapsedTimer   m_tapTimer;
    QVector<qint64> m_tapIntervals;
    static constexpr int kMaxTapSamples = 4;

    // Time display
    QLabel* m_timeDisplay    = nullptr;
    QLabel* m_selectionLabel = nullptr;

    // Zoom
    QSlider* m_zoomSlider = nullptr;
    QLabel*  m_zoomLabel  = nullptr;

    // Automation / Crossfade / Punch
    BevelButton* m_autoBtn   = nullptr;
    BevelButton* m_punchBtn  = nullptr;
    QComboBox*   m_xfCombo   = nullptr;

    // Secondary row
    BevelButton* m_busesBtn    = nullptr;
    BevelButton* m_flagBtn     = nullptr;
    BevelButton* m_prevBtn     = nullptr;
    BevelButton* m_nextBtn     = nullptr;
    BevelButton* m_listBtn     = nullptr;
    BevelButton* m_gridBtn     = nullptr;
    QComboBox*   m_zoomPreset  = nullptr;

    // Snap-to-grid
    QComboBox*   m_snapCombo   = nullptr;

    // Ripple edit
    BevelButton* m_rippleBtn   = nullptr;
};

} // namespace dawcast::widgets
