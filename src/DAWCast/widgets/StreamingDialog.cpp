// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "StreamingDialog.h"
#include "VUMeterWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QStyle>
#include <QFont>
#include <QPalette>
#include <QApplication>
#include <QDebug>

namespace dawcast::widgets {

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

StreamingDialog::StreamingDialog(dawcast::RTMPStreamer* streamer, QWidget* parent)
    : QDialog(parent)
    , m_streamer(streamer)
{
    setWindowTitle(tr("Stream Live"));
    setMinimumSize(520, 600);
    resize(560, 720);

    setupUi();

    // Connect streamer signals
    if (m_streamer) {
        connect(m_streamer, &RTMPStreamer::connected,
                this, &StreamingDialog::onStreamerConnected);
        connect(m_streamer, &RTMPStreamer::disconnected,
                this, &StreamingDialog::onStreamerDisconnected);
        connect(m_streamer, &RTMPStreamer::error,
                this, &StreamingDialog::onStreamerError);
        connect(m_streamer, &RTMPStreamer::statsUpdated,
                this, &StreamingDialog::onStreamerStats);
    }

    // Stats refresh timer
    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(500);
    connect(m_statsTimer, &QTimer::timeout, this, &StreamingDialog::onStatsUpdate);

    // On-air blink timer
    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(500);
    connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
        m_blinkState = !m_blinkState;
        if (m_onAirIndicator) {
            if (m_blinkState) {
                m_onAirIndicator->setStyleSheet(
                    QStringLiteral("QLabel { color: #ff2020; font-size: 18px; "
                                   "font-weight: bold; }"));
            } else {
                m_onAirIndicator->setStyleSheet(
                    QStringLiteral("QLabel { color: #cc0000; font-size: 18px; "
                                   "font-weight: bold; }"));
            }
        }
    });

    // Set initial monitor state
    setMonitorVisible(false);

    // If streamer is already streaming, show monitor
    if (m_streamer && m_streamer->isStreaming()) {
        setMonitorVisible(true);
        m_goLiveBtn->setEnabled(false);
        m_stopBtn->setEnabled(true);
        m_statsTimer->start();
        m_blinkTimer->start();
    }
}

StreamingDialog::~StreamingDialog() = default;

// ---------------------------------------------------------------------------
// UI Setup
// ---------------------------------------------------------------------------

void StreamingDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    setupPlatformSection();
    setupConnectionSection();
    setupAudioSection();
    setupVideoSection();
    setupMonitorSection();
    setupBottomButtons();
}

void StreamingDialog::setupPlatformSection()
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());

    auto* group = new QGroupBox(tr("Platform"), this);
    auto* form = new QHBoxLayout(group);

    m_platformCombo = new QComboBox(group);
    m_platformCombo->addItem(tr("Custom RTMP"),
        static_cast<int>(RTMPStreamer::StreamConfig::Custom));
    m_platformCombo->addItem(tr("YouTube Live"),
        static_cast<int>(RTMPStreamer::StreamConfig::YouTube));
    m_platformCombo->addItem(tr("Twitch"),
        static_cast<int>(RTMPStreamer::StreamConfig::Twitch));
    m_platformCombo->addItem(tr("Facebook Live"),
        static_cast<int>(RTMPStreamer::StreamConfig::Facebook));
    m_platformCombo->addItem(tr("Icecast"),
        static_cast<int>(RTMPStreamer::StreamConfig::Icecast));

    form->addWidget(new QLabel(tr("Destination:"), group));
    form->addWidget(m_platformCombo, 1);

    connect(m_platformCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StreamingDialog::onPlatformChanged);

    layout->addWidget(group);
}

void StreamingDialog::setupConnectionSection()
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());

    auto* group = new QGroupBox(tr("Connection"), this);
    auto* form = new QFormLayout(group);

    m_urlEdit = new QLineEdit(group);
    m_urlEdit->setPlaceholderText(tr("rtmp://server/app"));
    form->addRow(tr("Stream URL:"), m_urlEdit);

    // Stream key with show/hide toggle
    auto* keyLayout = new QHBoxLayout;
    m_streamKeyEdit = new QLineEdit(group);
    m_streamKeyEdit->setEchoMode(QLineEdit::Password);
    m_streamKeyEdit->setPlaceholderText(tr("Your stream key"));
    keyLayout->addWidget(m_streamKeyEdit, 1);

    m_toggleKeyBtn = new QPushButton(tr("Show"), group);
    m_toggleKeyBtn->setFixedWidth(60);
    connect(m_toggleKeyBtn, &QPushButton::clicked,
            this, &StreamingDialog::onToggleKeyVisibility);
    keyLayout->addWidget(m_toggleKeyBtn);

    form->addRow(tr("Stream Key:"), keyLayout);

    m_testConnBtn = new QPushButton(tr("Test Connection"), group);
    connect(m_testConnBtn, &QPushButton::clicked,
            this, &StreamingDialog::onTestConnection);
    form->addRow(QString(), m_testConnBtn);

    layout->addWidget(group);
}

void StreamingDialog::setupAudioSection()
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());

    auto* group = new QGroupBox(tr("Audio Settings"), this);
    auto* form = new QFormLayout(group);

    m_audioCodecCombo = new QComboBox(group);
    m_audioCodecCombo->addItem(QStringLiteral("AAC"), QStringLiteral("aac"));
    form->addRow(tr("Codec:"), m_audioCodecCombo);

    m_audioBitrateSpin = new QSpinBox(group);
    m_audioBitrateSpin->setRange(64, 320);
    m_audioBitrateSpin->setSingleStep(32);
    m_audioBitrateSpin->setValue(128);
    m_audioBitrateSpin->setSuffix(tr(" kbps"));
    form->addRow(tr("Bitrate:"), m_audioBitrateSpin);

    m_audioSampleRateCombo = new QComboBox(group);
    m_audioSampleRateCombo->addItem(QStringLiteral("44100 Hz"), 44100);
    m_audioSampleRateCombo->addItem(QStringLiteral("48000 Hz"), 48000);
    m_audioSampleRateCombo->setCurrentIndex(1);
    form->addRow(tr("Sample Rate:"), m_audioSampleRateCombo);

    m_audioChannelsCombo = new QComboBox(group);
    m_audioChannelsCombo->addItem(tr("Mono"), 1);
    m_audioChannelsCombo->addItem(tr("Stereo"), 2);
    m_audioChannelsCombo->setCurrentIndex(1);
    form->addRow(tr("Channels:"), m_audioChannelsCombo);

    layout->addWidget(group);
}

void StreamingDialog::setupVideoSection()
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());

    m_videoGroup = new QGroupBox(tr("Video Settings (Optional)"), this);
    m_videoGroup->setCheckable(true);
    m_videoGroup->setChecked(false);
    auto* form = new QFormLayout(m_videoGroup);

    m_enableVideoCheck = nullptr; // The group box's check state serves this role

    m_videoCodecCombo = new QComboBox(m_videoGroup);
    m_videoCodecCombo->addItem(QStringLiteral("H.264"), QStringLiteral("h264"));
    form->addRow(tr("Codec:"), m_videoCodecCombo);

    m_videoBitrateSpin = new QSpinBox(m_videoGroup);
    m_videoBitrateSpin->setRange(500, 20000);
    m_videoBitrateSpin->setSingleStep(500);
    m_videoBitrateSpin->setValue(2500);
    m_videoBitrateSpin->setSuffix(tr(" kbps"));
    form->addRow(tr("Bitrate:"), m_videoBitrateSpin);

    m_videoResolutionCombo = new QComboBox(m_videoGroup);
    m_videoResolutionCombo->addItem(QStringLiteral("1920x1080 (1080p)"), QStringLiteral("1920x1080"));
    m_videoResolutionCombo->addItem(QStringLiteral("1280x720 (720p)"), QStringLiteral("1280x720"));
    m_videoResolutionCombo->addItem(QStringLiteral("854x480 (480p)"), QStringLiteral("854x480"));
    m_videoResolutionCombo->addItem(QStringLiteral("640x360 (360p)"), QStringLiteral("640x360"));
    m_videoResolutionCombo->setCurrentIndex(1); // Default 720p
    form->addRow(tr("Resolution:"), m_videoResolutionCombo);

    m_videoFpsSpin = new QSpinBox(m_videoGroup);
    m_videoFpsSpin->setRange(15, 60);
    m_videoFpsSpin->setValue(30);
    m_videoFpsSpin->setSuffix(tr(" fps"));
    form->addRow(tr("Frame Rate:"), m_videoFpsSpin);

    layout->addWidget(m_videoGroup);
}

void StreamingDialog::setupMonitorSection()
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());

    m_monitorGroup = new QGroupBox(tr("Stream Monitor"), this);
    auto* monitorLayout = new QGridLayout(m_monitorGroup);
    monitorLayout->setSpacing(8);

    // Row 0: On-air indicator (spans 2 columns)
    m_onAirIndicator = new QLabel(tr("LIVE"), m_monitorGroup);
    m_onAirIndicator->setAlignment(Qt::AlignCenter);
    m_onAirIndicator->setStyleSheet(
        QStringLiteral("QLabel { color: #ff2020; font-size: 18px; "
                        "font-weight: bold; background-color: #1a1a1a; "
                        "border: 2px solid #ff2020; border-radius: 6px; "
                        "padding: 4px 16px; }"));
    monitorLayout->addWidget(m_onAirIndicator, 0, 0, 1, 2, Qt::AlignCenter);

    // Row 1: Uptime
    monitorLayout->addWidget(new QLabel(tr("Uptime:"), m_monitorGroup), 1, 0);
    m_uptimeLabel = new QLabel(QStringLiteral("00:00:00"), m_monitorGroup);
    m_uptimeLabel->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 14px;"));
    monitorLayout->addWidget(m_uptimeLabel, 1, 1);

    // Row 2: Bitrate
    monitorLayout->addWidget(new QLabel(tr("Bitrate:"), m_monitorGroup), 2, 0);
    m_bitrateLabel = new QLabel(QStringLiteral("0 kbps"), m_monitorGroup);
    m_bitrateLabel->setStyleSheet(QStringLiteral("font-family: monospace;"));
    monitorLayout->addWidget(m_bitrateLabel, 2, 1);

    // Row 3: Bytes sent
    monitorLayout->addWidget(new QLabel(tr("Data Sent:"), m_monitorGroup), 3, 0);
    m_bytesSentLabel = new QLabel(QStringLiteral("0 KB"), m_monitorGroup);
    m_bytesSentLabel->setStyleSheet(QStringLiteral("font-family: monospace;"));
    monitorLayout->addWidget(m_bytesSentLabel, 3, 1);

    // Row 4: Dropped frames
    monitorLayout->addWidget(new QLabel(tr("Dropped:"), m_monitorGroup), 4, 0);
    m_droppedLabel = new QLabel(QStringLiteral("0"), m_monitorGroup);
    m_droppedLabel->setStyleSheet(QStringLiteral("font-family: monospace;"));
    monitorLayout->addWidget(m_droppedLabel, 4, 1);

    // Row 5: Connection quality
    monitorLayout->addWidget(new QLabel(tr("Quality:"), m_monitorGroup), 5, 0);
    m_qualityLabel = new QLabel(tr("--"), m_monitorGroup);
    monitorLayout->addWidget(m_qualityLabel, 5, 1);

    // Row 6: VU meters
    auto* vuLayout = new QHBoxLayout;
    vuLayout->addWidget(new QLabel(QStringLiteral("L"), m_monitorGroup));
    m_vuMeterL = new VUMeterWidget(m_monitorGroup);
    m_vuMeterL->setOrientation(Qt::Horizontal);
    m_vuMeterL->setMinimumSize(120, 20);
    m_vuMeterL->setMaximumHeight(24);
    vuLayout->addWidget(m_vuMeterL, 1);

    vuLayout->addSpacing(8);

    vuLayout->addWidget(new QLabel(QStringLiteral("R"), m_monitorGroup));
    m_vuMeterR = new VUMeterWidget(m_monitorGroup);
    m_vuMeterR->setOrientation(Qt::Horizontal);
    m_vuMeterR->setMinimumSize(120, 20);
    m_vuMeterR->setMaximumHeight(24);
    vuLayout->addWidget(m_vuMeterR, 1);

    monitorLayout->addLayout(vuLayout, 6, 0, 1, 2);

    layout->addWidget(m_monitorGroup);
}

void StreamingDialog::setupBottomButtons()
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());

    layout->addStretch(1);

    auto* btnLayout = new QHBoxLayout;

    m_goLiveBtn = new QPushButton(tr("Go Live"), this);
    m_goLiveBtn->setMinimumHeight(40);
    m_goLiveBtn->setStyleSheet(
        QStringLiteral("QPushButton { background-color: #cc2020; color: white; "
                        "font-size: 16px; font-weight: bold; border-radius: 6px; "
                        "padding: 8px 24px; }"
                        "QPushButton:hover { background-color: #ff3333; }"
                        "QPushButton:disabled { background-color: #666666; }"));
    connect(m_goLiveBtn, &QPushButton::clicked, this, &StreamingDialog::onGoLive);
    btnLayout->addWidget(m_goLiveBtn);

    m_stopBtn = new QPushButton(tr("Stop"), this);
    m_stopBtn->setMinimumHeight(40);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet(
        QStringLiteral("QPushButton { font-size: 14px; padding: 8px 16px; }"
                        "QPushButton:disabled { color: #888888; }"));
    connect(m_stopBtn, &QPushButton::clicked, this, &StreamingDialog::onStopStream);
    btnLayout->addWidget(m_stopBtn);

    m_closeBtn = new QPushButton(tr("Close"), this);
    m_closeBtn->setMinimumHeight(40);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::close);
    btnLayout->addWidget(m_closeBtn);

    layout->addLayout(btnLayout);
}

// ---------------------------------------------------------------------------
// Platform presets
// ---------------------------------------------------------------------------

void StreamingDialog::onPlatformChanged(int index)
{
    auto platform = static_cast<RTMPStreamer::StreamConfig::Platform>(
        m_platformCombo->itemData(index).toInt());
    applyPlatformPreset(platform);
}

void StreamingDialog::applyPlatformPreset(RTMPStreamer::StreamConfig::Platform platform)
{
    switch (platform) {
    case RTMPStreamer::StreamConfig::YouTube:
        m_urlEdit->setText(QStringLiteral("rtmp://a.rtmp.youtube.com/live2"));
        m_audioBitrateSpin->setValue(128);
        m_audioSampleRateCombo->setCurrentIndex(1); // 48000
        m_audioChannelsCombo->setCurrentIndex(1);    // Stereo
        break;

    case RTMPStreamer::StreamConfig::Twitch:
        m_urlEdit->setText(QStringLiteral("rtmp://live.twitch.tv/app"));
        m_audioBitrateSpin->setValue(160);
        m_audioSampleRateCombo->setCurrentIndex(1); // 48000
        m_audioChannelsCombo->setCurrentIndex(1);    // Stereo
        break;

    case RTMPStreamer::StreamConfig::Facebook:
        m_urlEdit->setText(QStringLiteral("rtmps://live-api-s.facebook.com:443/rtmp"));
        m_audioBitrateSpin->setValue(128);
        m_audioSampleRateCombo->setCurrentIndex(0); // 44100
        m_audioChannelsCombo->setCurrentIndex(1);    // Stereo
        break;

    case RTMPStreamer::StreamConfig::Icecast:
        m_urlEdit->setText(QString());
        m_urlEdit->setPlaceholderText(tr("http://server:8000/mount"));
        m_audioBitrateSpin->setValue(128);
        break;

    case RTMPStreamer::StreamConfig::Custom:
    default:
        m_urlEdit->clear();
        m_urlEdit->setPlaceholderText(tr("rtmp://server/app"));
        break;
    }
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------

void StreamingDialog::onToggleKeyVisibility()
{
    if (m_streamKeyEdit->echoMode() == QLineEdit::Password) {
        m_streamKeyEdit->setEchoMode(QLineEdit::Normal);
        m_toggleKeyBtn->setText(tr("Hide"));
    } else {
        m_streamKeyEdit->setEchoMode(QLineEdit::Password);
        m_toggleKeyBtn->setText(tr("Show"));
    }
}

void StreamingDialog::onTestConnection()
{
    if (m_urlEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Test Connection"),
                             tr("Please enter a stream URL first."));
        return;
    }

    // Basic URL validation
    QString url = m_urlEdit->text().trimmed();
    if (!url.startsWith(QLatin1String("rtmp://")) &&
        !url.startsWith(QLatin1String("rtmps://")) &&
        !url.startsWith(QLatin1String("http://")) &&
        !url.startsWith(QLatin1String("https://"))) {
        QMessageBox::warning(this, tr("Test Connection"),
                             tr("URL must start with rtmp://, rtmps://, http://, or https://"));
        return;
    }

    QMessageBox::information(this, tr("Test Connection"),
                             tr("URL format looks valid.\n\n"
                                "A full connection test will be performed when you "
                                "click 'Go Live'. Make sure your stream key is correct."));
}

// ---------------------------------------------------------------------------
// Go Live / Stop
// ---------------------------------------------------------------------------

void StreamingDialog::onGoLive()
{
    if (!m_streamer) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Streaming engine not available."));
        return;
    }

    if (m_urlEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Stream Live"),
                             tr("Please enter a stream URL."));
        return;
    }

    // Build and apply config
    auto config = buildConfig();
    m_streamer->setConfig(config);

    // Disable the Go Live button while connecting
    m_goLiveBtn->setEnabled(false);
    m_goLiveBtn->setText(tr("Connecting..."));

    if (!m_streamer->startStreaming()) {
        m_goLiveBtn->setEnabled(true);
        m_goLiveBtn->setText(tr("Go Live"));
        QMessageBox::critical(this, tr("Stream Error"),
                              tr("Failed to start streaming. Check your URL and settings."));
    }
}

void StreamingDialog::onStopStream()
{
    if (m_streamer) {
        m_streamer->stopStreaming();
    }
}

// ---------------------------------------------------------------------------
// Streamer signal handlers
// ---------------------------------------------------------------------------

void StreamingDialog::onStreamerConnected()
{
    setMonitorVisible(true);
    m_goLiveBtn->setEnabled(false);
    m_goLiveBtn->setText(tr("On Air"));
    m_stopBtn->setEnabled(true);
    m_statsTimer->start();
    m_blinkTimer->start();

    emit streamingStarted();
}

void StreamingDialog::onStreamerDisconnected()
{
    setMonitorVisible(false);
    m_goLiveBtn->setEnabled(true);
    m_goLiveBtn->setText(tr("Go Live"));
    m_stopBtn->setEnabled(false);
    m_statsTimer->stop();
    m_blinkTimer->stop();

    // Reset on-air indicator
    if (m_onAirIndicator) {
        m_onAirIndicator->setStyleSheet(
            QStringLiteral("QLabel { color: #666666; font-size: 18px; "
                            "font-weight: bold; }"));
    }

    emit streamingStopped();
}

void StreamingDialog::onStreamerError(const QString& message)
{
    m_goLiveBtn->setEnabled(true);
    m_goLiveBtn->setText(tr("Go Live"));
    m_stopBtn->setEnabled(false);
    m_statsTimer->stop();
    m_blinkTimer->stop();

    QMessageBox::critical(this, tr("Streaming Error"), message);
}

void StreamingDialog::onStreamerStats(int64_t bytesSent, double bitrateKbps,
                                      int droppedFrames)
{
    // Update bitrate
    m_bitrateLabel->setText(QStringLiteral("%1 kbps").arg(bitrateKbps, 0, 'f', 0));

    // Update bytes sent (human-readable)
    QString sizeStr;
    if (bytesSent < 1024LL * 1024) {
        sizeStr = QStringLiteral("%1 KB").arg(bytesSent / 1024);
    } else if (bytesSent < 1024LL * 1024 * 1024) {
        sizeStr = QStringLiteral("%1 MB").arg(
            static_cast<double>(bytesSent) / (1024.0 * 1024.0), 0, 'f', 1);
    } else {
        sizeStr = QStringLiteral("%1 GB").arg(
            static_cast<double>(bytesSent) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
    m_bytesSentLabel->setText(sizeStr);

    // Update dropped frames
    m_droppedLabel->setText(QString::number(droppedFrames));
    if (droppedFrames > 0) {
        m_droppedLabel->setStyleSheet(
            QStringLiteral("font-family: monospace; color: #ff6600;"));
    }

    // Update connection quality
    updateConnectionQuality(bitrateKbps);
}

void StreamingDialog::onStatsUpdate()
{
    if (!m_streamer || !m_streamer->isStreaming()) return;

    // Update uptime
    double uptime = m_streamer->uptimeSeconds();
    int hours   = static_cast<int>(uptime) / 3600;
    int minutes = (static_cast<int>(uptime) % 3600) / 60;
    int seconds = static_cast<int>(uptime) % 60;
    m_uptimeLabel->setText(QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0')));
}

// ---------------------------------------------------------------------------
// Audio levels (called externally from the main window)
// ---------------------------------------------------------------------------

void StreamingDialog::setAudioLevels(float peakDbL, float rmsDbL,
                                      float peakDbR, float rmsDbR)
{
    if (m_vuMeterL) m_vuMeterL->setLevel(peakDbL, rmsDbL);
    if (m_vuMeterR) m_vuMeterR->setLevel(peakDbR, rmsDbR);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void StreamingDialog::setMonitorVisible(bool visible)
{
    if (m_monitorGroup) {
        m_monitorGroup->setVisible(visible);
    }
}

void StreamingDialog::updateConnectionQuality(double bitrateKbps)
{
    if (!m_qualityLabel) return;

    // Target bitrate from the config
    double targetKbps = m_audioBitrateSpin->value();
    if (m_videoGroup && m_videoGroup->isChecked()) {
        targetKbps += m_videoBitrateSpin->value();
    }

    double ratio = (targetKbps > 0) ? (bitrateKbps / targetKbps) : 0.0;

    if (ratio >= 0.9) {
        m_qualityLabel->setText(tr("Excellent"));
        m_qualityLabel->setStyleSheet(QStringLiteral("color: #22cc22; font-weight: bold;"));
    } else if (ratio >= 0.7) {
        m_qualityLabel->setText(tr("Good"));
        m_qualityLabel->setStyleSheet(QStringLiteral("color: #88cc22; font-weight: bold;"));
    } else if (ratio >= 0.4) {
        m_qualityLabel->setText(tr("Fair"));
        m_qualityLabel->setStyleSheet(QStringLiteral("color: #ccaa00; font-weight: bold;"));
    } else if (bitrateKbps > 0) {
        m_qualityLabel->setText(tr("Poor"));
        m_qualityLabel->setStyleSheet(QStringLiteral("color: #cc2020; font-weight: bold;"));
    } else {
        m_qualityLabel->setText(tr("--"));
        m_qualityLabel->setStyleSheet(QString());
    }
}

dawcast::RTMPStreamer::StreamConfig StreamingDialog::buildConfig() const
{
    RTMPStreamer::StreamConfig cfg;

    cfg.platform = static_cast<RTMPStreamer::StreamConfig::Platform>(
        m_platformCombo->currentData().toInt());
    cfg.url       = m_urlEdit->text().trimmed();
    cfg.streamKey = m_streamKeyEdit->text().trimmed();

    // Audio
    cfg.audioCodec      = m_audioCodecCombo->currentData().toString();
    cfg.audioBitrate    = m_audioBitrateSpin->value();
    cfg.audioSampleRate = m_audioSampleRateCombo->currentData().toInt();
    cfg.audioChannels   = m_audioChannelsCombo->currentData().toInt();

    // Video
    cfg.enableVideo = m_videoGroup->isChecked();
    if (cfg.enableVideo) {
        cfg.videoCodec   = m_videoCodecCombo->currentData().toString();
        cfg.videoBitrate = m_videoBitrateSpin->value();
        cfg.videoFps     = m_videoFpsSpin->value();

        // Parse resolution string "WIDTHxHEIGHT"
        QString res = m_videoResolutionCombo->currentData().toString();
        QStringList parts = res.split(QLatin1Char('x'));
        if (parts.size() == 2) {
            cfg.videoWidth  = parts[0].toInt();
            cfg.videoHeight = parts[1].toInt();
        }
    }

    return cfg;
}

} // namespace dawcast::widgets
