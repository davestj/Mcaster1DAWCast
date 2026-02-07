// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QWidget>
#include <QElapsedTimer>

class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;

namespace dawcast { class RTMPStreamer; }

namespace dawcast::widgets {

/// DJ / Live Streaming mode panel — live stream monitoring with on-air
/// indicator, uptime, viewer count, stream health, and start/stop controls.
class StreamMonitorPanel : public QWidget {
    Q_OBJECT

public:
    explicit StreamMonitorPanel(QWidget* parent = nullptr);
    ~StreamMonitorPanel() override;

    /// Wire to the RTMP streamer for real-time status updates.
    void setRTMPStreamer(dawcast::RTMPStreamer* streamer);

    /// Update viewer count (future integration with chat/analytics).
    void setViewerCount(int count);

    /// Update stream health (0.0 = dead, 1.0 = perfect).
    void setStreamHealth(float health);

signals:
    void startStreamRequested();
    void stopStreamRequested();

private slots:
    void onUptimeTick();
    void onStreamStarted();
    void onStreamStopped();

private:
    void buildUI();
    void updateUptimeDisplay();
    void updateHealthIndicator(float health);

    // On-air badge
    QLabel*      m_liveBadge       = nullptr;

    // Metrics
    QLabel*      m_uptimeLabel     = nullptr;
    QLabel*      m_viewerLabel     = nullptr;
    QLabel*      m_healthLabel     = nullptr;
    QLabel*      m_healthBar       = nullptr;

    // Chat placeholder
    QLabel*      m_chatPlaceholder = nullptr;

    // Controls
    QPushButton* m_startBtn        = nullptr;
    QPushButton* m_stopBtn         = nullptr;

    QTimer*       m_uptimeTimer    = nullptr;
    QElapsedTimer m_uptime;

    dawcast::RTMPStreamer* m_streamer = nullptr;
    bool          m_isLive         = false;
    int           m_viewerCount    = 0;
    float         m_health         = 1.0f;
};

} // namespace dawcast::widgets
