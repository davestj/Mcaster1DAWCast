// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>
#include <QString>
#include <QTimer>

#include "../broadcast/RTMPStreamer.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QGroupBox;
class QTabWidget;

namespace dawcast::widgets {

class VUMeterWidget;

/// Dialog for configuring and monitoring live RTMP streaming.
///
/// Supports YouTube, Twitch, Facebook, Icecast, and custom RTMP servers.
/// Shows real-time streaming statistics including bitrate, uptime, dropped
/// frames, audio levels, and connection quality.
class StreamingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StreamingDialog(dawcast::RTMPStreamer* streamer,
                             QWidget* parent = nullptr);
    ~StreamingDialog() override;

    /// Set the current audio levels for the VU meters (called from GUI thread).
    void setAudioLevels(float peakDbL, float rmsDbL, float peakDbR, float rmsDbR);

signals:
    /// Emitted when the user clicks "Go Live" and streaming starts successfully.
    void streamingStarted();

    /// Emitted when the user clicks "Stop" or the stream disconnects.
    void streamingStopped();

private slots:
    void onPlatformChanged(int index);
    void onGoLive();
    void onStopStream();
    void onTestConnection();
    void onToggleKeyVisibility();
    void onStatsUpdate();

    // RTMPStreamer signal handlers
    void onStreamerConnected();
    void onStreamerDisconnected();
    void onStreamerError(const QString& message);
    void onStreamerStats(int64_t bytesSent, double bitrateKbps, int droppedFrames);

private:
    void setupUi();
    void setupPlatformSection();
    void setupConnectionSection();
    void setupAudioSection();
    void setupVideoSection();
    void setupMonitorSection();
    void setupBottomButtons();

    void applyPlatformPreset(dawcast::RTMPStreamer::StreamConfig::Platform platform);
    void setMonitorVisible(bool visible);
    void updateConnectionQuality(double bitrateKbps);

    /// Collect settings from the UI into a StreamConfig.
    dawcast::RTMPStreamer::StreamConfig buildConfig() const;

    dawcast::RTMPStreamer* m_streamer = nullptr;

    // -- Platform selector --
    QComboBox*    m_platformCombo = nullptr;

    // -- Connection section --
    QLineEdit*    m_urlEdit       = nullptr;
    QLineEdit*    m_streamKeyEdit = nullptr;
    QPushButton*  m_toggleKeyBtn  = nullptr;
    QPushButton*  m_testConnBtn   = nullptr;

    // -- Audio settings --
    QComboBox*    m_audioCodecCombo   = nullptr;
    QSpinBox*     m_audioBitrateSpin  = nullptr;
    QComboBox*    m_audioSampleRateCombo = nullptr;
    QComboBox*    m_audioChannelsCombo = nullptr;

    // -- Video settings --
    QGroupBox*    m_videoGroup        = nullptr;
    QCheckBox*    m_enableVideoCheck  = nullptr;
    QComboBox*    m_videoCodecCombo   = nullptr;
    QSpinBox*     m_videoBitrateSpin  = nullptr;
    QComboBox*    m_videoResolutionCombo = nullptr;
    QSpinBox*     m_videoFpsSpin      = nullptr;

    // -- Stream monitor --
    QGroupBox*    m_monitorGroup   = nullptr;
    QLabel*       m_onAirIndicator = nullptr;
    QLabel*       m_uptimeLabel    = nullptr;
    QLabel*       m_bitrateLabel   = nullptr;
    QLabel*       m_bytesSentLabel = nullptr;
    QLabel*       m_droppedLabel   = nullptr;
    QLabel*       m_qualityLabel   = nullptr;
    VUMeterWidget* m_vuMeterL     = nullptr;
    VUMeterWidget* m_vuMeterR     = nullptr;

    // -- Bottom buttons --
    QPushButton*  m_goLiveBtn  = nullptr;
    QPushButton*  m_stopBtn    = nullptr;
    QPushButton*  m_closeBtn   = nullptr;

    // -- Timer for periodic stats refresh --
    QTimer*       m_statsTimer = nullptr;

    // -- On-air blink state --
    QTimer*       m_blinkTimer = nullptr;
    bool          m_blinkState = false;
};

} // namespace dawcast::widgets
