// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "StreamMonitorPanel.h"
#include "../broadcast/RTMPStreamer.h"

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QTime>

namespace dawcast::widgets {

// ── Style Constants ────────────────────────────────────────────────────────

static const QString kLiveBadgeOn = QStringLiteral(
    "QLabel {"
    "  background: #ff2020;"
    "  color: white;"
    "  font-size: 22px;"
    "  font-weight: bold;"
    "  letter-spacing: 4px;"
    "  padding: 12px 32px;"
    "  border-radius: 8px;"
    "  border: 2px solid #ff4040;"
    "}");

static const QString kLiveBadgeOff = QStringLiteral(
    "QLabel {"
    "  background: #2a3038;"
    "  color: #5a6068;"
    "  font-size: 22px;"
    "  font-weight: bold;"
    "  letter-spacing: 4px;"
    "  padding: 12px 32px;"
    "  border-radius: 8px;"
    "  border: 2px solid #3a4048;"
    "}");

static const QString kMetricLabel = QStringLiteral(
    "QLabel { color: #aab5ba; font-size: 12px; }");

static const QString kMetricValue = QStringLiteral(
    "QLabel { color: #e0e5e8; font-size: 16px; font-weight: bold; }");

// ── Construction ───────────────────────────────────────────────────────────

StreamMonitorPanel::StreamMonitorPanel(QWidget* parent)
    : QWidget(parent)
{
    buildUI();

    m_uptimeTimer = new QTimer(this);
    m_uptimeTimer->setInterval(1000);
    connect(m_uptimeTimer, &QTimer::timeout,
            this, &StreamMonitorPanel::onUptimeTick);
}

StreamMonitorPanel::~StreamMonitorPanel() = default;

// ── Build UI ───────────────────────────────────────────────────────────────

void StreamMonitorPanel::buildUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    setStyleSheet(QStringLiteral(
        "StreamMonitorPanel { background: #141c22; }"));

    // ── LIVE Badge ─────────────────────────────────────────────────────
    m_liveBadge = new QLabel(tr("LIVE"), this);
    m_liveBadge->setAlignment(Qt::AlignCenter);
    m_liveBadge->setStyleSheet(kLiveBadgeOff);
    layout->addWidget(m_liveBadge, 0, Qt::AlignCenter);

    // ── Separator ──────────────────────────────────────────────────────
    auto* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet(QStringLiteral("QFrame { color: #2a3a42; }"));
    layout->addWidget(sep1);

    // ── Uptime ─────────────────────────────────────────────────────────
    auto* uptimeRow = new QHBoxLayout();
    auto* uptimeCaption = new QLabel(tr("Uptime:"), this);
    uptimeCaption->setStyleSheet(kMetricLabel);
    m_uptimeLabel = new QLabel(tr("00:00:00"), this);
    m_uptimeLabel->setStyleSheet(kMetricValue);
    uptimeRow->addWidget(uptimeCaption);
    uptimeRow->addStretch();
    uptimeRow->addWidget(m_uptimeLabel);
    layout->addLayout(uptimeRow);

    // ── Viewer Count ───────────────────────────────────────────────────
    auto* viewerRow = new QHBoxLayout();
    auto* viewerCaption = new QLabel(tr("Viewers:"), this);
    viewerCaption->setStyleSheet(kMetricLabel);
    m_viewerLabel = new QLabel(QStringLiteral("0"), this);
    m_viewerLabel->setStyleSheet(kMetricValue);
    viewerRow->addWidget(viewerCaption);
    viewerRow->addStretch();
    viewerRow->addWidget(m_viewerLabel);
    layout->addLayout(viewerRow);

    // ── Stream Health ──────────────────────────────────────────────────
    auto* healthRow = new QHBoxLayout();
    auto* healthCaption = new QLabel(tr("Health:"), this);
    healthCaption->setStyleSheet(kMetricLabel);
    m_healthLabel = new QLabel(tr("Excellent"), this);
    m_healthLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #00e676; font-size: 14px; font-weight: bold; }"));
    healthRow->addWidget(healthCaption);
    healthRow->addStretch();
    healthRow->addWidget(m_healthLabel);
    layout->addLayout(healthRow);

    // Health bar (simple visual indicator)
    m_healthBar = new QLabel(this);
    m_healthBar->setFixedHeight(6);
    m_healthBar->setStyleSheet(QStringLiteral(
        "QLabel { background: #00e676; border-radius: 3px; }"));
    layout->addWidget(m_healthBar);

    // ── Separator ──────────────────────────────────────────────────────
    auto* sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet(QStringLiteral("QFrame { color: #2a3a42; }"));
    layout->addWidget(sep2);

    // ── Chat Placeholder ───────────────────────────────────────────────
    m_chatPlaceholder = new QLabel(tr("Chat integration coming soon..."), this);
    m_chatPlaceholder->setAlignment(Qt::AlignCenter);
    m_chatPlaceholder->setStyleSheet(QStringLiteral(
        "QLabel { color: #5a6a72; font-size: 12px; font-style: italic; "
        "padding: 20px; background: #1a2228; border-radius: 6px; "
        "border: 1px dashed #2a3a42; }"));
    layout->addWidget(m_chatPlaceholder, 1);

    // ── Controls ───────────────────────────────────────────────────────
    auto* controlRow = new QHBoxLayout();
    controlRow->setSpacing(8);

    m_startBtn = new QPushButton(tr("Start Stream"), this);
    m_startBtn->setFixedHeight(36);
    m_startBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: #00bcb4;"
        "  color: white;"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 0 20px;"
        "}"
        "QPushButton:hover { background: #00d4cc; }"
        "QPushButton:pressed { background: #009a94; }"));
    connect(m_startBtn, &QPushButton::clicked,
            this, &StreamMonitorPanel::startStreamRequested);
    controlRow->addWidget(m_startBtn);

    m_stopBtn = new QPushButton(tr("Stop Stream"), this);
    m_stopBtn->setFixedHeight(36);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: #3a4048;"
        "  color: #aab5ba;"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 0 20px;"
        "}"
        "QPushButton:enabled {"
        "  background: #d32f2f;"
        "  color: white;"
        "}"
        "QPushButton:enabled:hover { background: #e53935; }"
        "QPushButton:enabled:pressed { background: #b71c1c; }"));
    connect(m_stopBtn, &QPushButton::clicked,
            this, &StreamMonitorPanel::stopStreamRequested);
    controlRow->addWidget(m_stopBtn);

    layout->addLayout(controlRow);
}

// ── RTMP Streamer Integration ──────────────────────────────────────────────

void StreamMonitorPanel::setRTMPStreamer(dawcast::RTMPStreamer* streamer)
{
    m_streamer = streamer;
    if (!streamer) return;

    connect(streamer, &RTMPStreamer::connected,
            this, &StreamMonitorPanel::onStreamStarted);
    connect(streamer, &RTMPStreamer::disconnected,
            this, &StreamMonitorPanel::onStreamStopped);
}

void StreamMonitorPanel::setViewerCount(int count)
{
    m_viewerCount = count;
    m_viewerLabel->setText(QString::number(count));
}

void StreamMonitorPanel::setStreamHealth(float health)
{
    m_health = qBound(0.0f, health, 1.0f);
    updateHealthIndicator(m_health);
}

// ── Stream State Changes ───────────────────────────────────────────────────

void StreamMonitorPanel::onStreamStarted()
{
    m_isLive = true;
    m_liveBadge->setStyleSheet(kLiveBadgeOn);

    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);

    m_uptime.start();
    m_uptimeTimer->start();
    updateUptimeDisplay();
}

void StreamMonitorPanel::onStreamStopped()
{
    m_isLive = false;
    m_liveBadge->setStyleSheet(kLiveBadgeOff);

    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);

    m_uptimeTimer->stop();
}

// ── Uptime ─────────────────────────────────────────────────────────────────

void StreamMonitorPanel::onUptimeTick()
{
    updateUptimeDisplay();
}

void StreamMonitorPanel::updateUptimeDisplay()
{
    if (!m_isLive) {
        m_uptimeLabel->setText(QStringLiteral("00:00:00"));
        return;
    }

    qint64 msec = m_uptime.elapsed();
    int totalSec = static_cast<int>(msec / 1000);
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;

    m_uptimeLabel->setText(QStringLiteral("%1:%2:%3")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0')));
}

// ── Health Indicator ───────────────────────────────────────────────────────

void StreamMonitorPanel::updateHealthIndicator(float health)
{
    QString text;
    QString color;

    if (health >= 0.8f) {
        text = tr("Excellent");
        color = QStringLiteral("#00e676");
    } else if (health >= 0.5f) {
        text = tr("Good");
        color = QStringLiteral("#ffeb3b");
    } else if (health >= 0.2f) {
        text = tr("Poor");
        color = QStringLiteral("#ff9800");
    } else {
        text = tr("Critical");
        color = QStringLiteral("#f44336");
    }

    m_healthLabel->setText(text);
    m_healthLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 14px; font-weight: bold; }")
            .arg(color));

    // Scale the health bar width
    int maxWidth = m_healthBar->parentWidget() ? m_healthBar->parentWidget()->width() - 32 : 200;
    int barWidth = qMax(6, static_cast<int>(maxWidth * health));
    m_healthBar->setFixedWidth(barWidth);
    m_healthBar->setStyleSheet(
        QStringLiteral("QLabel { background: %1; border-radius: 3px; }")
            .arg(color));
}

} // namespace dawcast::widgets
