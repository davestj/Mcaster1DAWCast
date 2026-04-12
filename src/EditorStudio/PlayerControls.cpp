// DAWCast Editor Studio — Player Controls
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PlayerControls.h"
#include "ForensicWaveformView.h"

#include <QToolButton>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFile>
#include <QDir>
#include <QIcon>
#include <QFont>
#include <QFontDatabase>
#include <QCoreApplication>

#include <cmath>

namespace dawcast::editor {

PlayerControls::PlayerControls(ForensicWaveformView* view, QWidget* parent)
    : QWidget(parent)
    , m_view(view)
{
    setObjectName(QStringLiteral("playerControls"));
    setMinimumHeight(82);
    setMaximumHeight(96);
    setAutoFillBackground(true);

    setStyleSheet(QStringLiteral(
        "QWidget#playerControls {"
        "  background: qlineargradient(x1:0 y1:0 x2:0 y2:1,"
        "    stop:0 #2a2a32, stop:1 #16161c);"
        "  border-top: 1px solid #4a4a55;"
        "}"
        "QToolButton {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 4px;"
        "  border-radius: 30px;"
        "}"
        "QToolButton:hover {"
        "  background: rgba(255,255,255,18);"
        "}"
        "QToolButton:pressed {"
        "  background: rgba(0,0,0,80);"
        "}"
        "QSlider::groove:horizontal {"
        "  background: #0a0a10;"
        "  height: 6px;"
        "  border: 1px solid #44444c;"
        "  border-radius: 3px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: qlineargradient(x1:0 y1:0 x2:1 y2:0,"
        "    stop:0 #1f9028, stop:1 #5fe060);"
        "  border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: qradialgradient(cx:0.5, cy:0.4, radius:0.6,"
        "    stop:0 #f0f0f8, stop:1 #707080);"
        "  border: 1px solid #1a1a22;"
        "  width: 16px;"
        "  margin: -6px 0;"
        "  border-radius: 8px;"
        "}"
        "QLabel#tcNow, QLabel#tcAll {"
        "  color: #5fe060;"
        "  background: #0a0a10;"
        "  border: 1px solid #44444c;"
        "  border-radius: 3px;"
        "  padding: 2px 8px;"
        "  font-family: 'Menlo', 'Monaco', monospace;"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "}"
        "QLabel#tcAll { color: #888; }"
        "QLabel#sep { color: #555; padding: 0 4px; }"
    ));

    // ── Buttons ──────────────────────────────────────────────────────────
    m_btnRev   = makeIconButton(QStringLiteral("rev.svg"),   QStringLiteral("Rewind 5s (Left Arrow)"));
    m_btnPlay  = makeIconButton(QStringLiteral("play.svg"),  QStringLiteral("Play (Space)"));
    m_btnPause = makeIconButton(QStringLiteral("pause.svg"), QStringLiteral("Pause (Space)"));
    m_btnStop  = makeIconButton(QStringLiteral("stop.svg"),  QStringLiteral("Stop"));
    m_btnFf    = makeIconButton(QStringLiteral("ff.svg"),    QStringLiteral("Fast Forward 5s (Right Arrow)"));

    // Play button is bigger
    m_btnPlay->setIconSize(QSize(56, 56));
    m_btnPlay->setFixedSize(64, 64);
    for (auto* b : { m_btnRev, m_btnPause, m_btnStop, m_btnFf }) {
        b->setIconSize(QSize(44, 44));
        b->setFixedSize(52, 52);
    }

    // ── Scrubber ─────────────────────────────────────────────────────────
    m_scrubber = new QSlider(Qt::Horizontal);
    m_scrubber->setRange(0, 10000);
    m_scrubber->setValue(0);
    m_scrubber->setToolTip(QStringLiteral("Scrub through file — drag to seek"));

    // ── Timecode display ─────────────────────────────────────────────────
    m_timeNow = new QLabel(QStringLiteral("00:00:00.000"));
    m_timeNow->setObjectName(QStringLiteral("tcNow"));
    m_timeNow->setToolTip(QStringLiteral("Current playhead position"));

    auto* sep = new QLabel(QStringLiteral("/"));
    sep->setObjectName(QStringLiteral("sep"));

    m_timeAll = new QLabel(QStringLiteral("00:00:00.000"));
    m_timeAll->setObjectName(QStringLiteral("tcAll"));
    m_timeAll->setToolTip(QStringLiteral("Total duration"));

    // ── Layout: top row scrubber+time, bottom row buttons centered ──────
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 6, 14, 6);
    root->setSpacing(2);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(6);
    topRow->addWidget(m_scrubber, 1);
    topRow->addWidget(m_timeNow);
    topRow->addWidget(sep);
    topRow->addWidget(m_timeAll);
    root->addLayout(topRow);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);
    btnRow->addStretch(1);
    btnRow->addWidget(m_btnRev);
    btnRow->addWidget(m_btnPause);
    btnRow->addWidget(m_btnPlay);
    btnRow->addWidget(m_btnStop);
    btnRow->addWidget(m_btnFf);
    btnRow->addStretch(1);
    root->addLayout(btnRow);

    // ── Wire to view ─────────────────────────────────────────────────────
    connect(m_btnRev,   &QToolButton::clicked, this, [this]() {
        if (m_view) m_view->skipSeconds(-5.0);
    });
    connect(m_btnPlay,  &QToolButton::clicked, this, [this]() {
        if (m_view) m_view->play();
    });
    connect(m_btnPause, &QToolButton::clicked, this, [this]() {
        if (m_view) m_view->pause();
    });
    connect(m_btnStop,  &QToolButton::clicked, this, [this]() {
        if (m_view) m_view->stop();
    });
    connect(m_btnFf,    &QToolButton::clicked, this, [this]() {
        if (m_view) m_view->skipSeconds(5.0);
    });

    connect(m_scrubber, &QSlider::sliderPressed, this, [this]() {
        m_userScrub = true;
    });
    connect(m_scrubber, &QSlider::sliderMoved, this, [this](int value) {
        if (!m_view || m_total <= 0) return;
        int64_t target = static_cast<int64_t>(
            m_total * static_cast<double>(value) / 10000.0);
        m_view->seek(target);
        m_timeNow->setText(formatTime(target));
    });
    connect(m_scrubber, &QSlider::sliderReleased, this, [this]() {
        m_userScrub = false;
    });

    setEnabled(false);
}

QToolButton* PlayerControls::makeIconButton(const QString& iconName, const QString& tip)
{
    auto* btn = new QToolButton();
    btn->setToolTip(tip);
    btn->setFocusPolicy(Qt::NoFocus); // don't steal Space key from waveform
    btn->setCursor(Qt::PointingHandCursor);
    btn->setAutoRaise(false);

    QString path = findIcon(iconName);
    if (!path.isEmpty()) {
        btn->setIcon(QIcon(path));
    }
    return btn;
}

QString PlayerControls::findIcon(const QString& filename) const
{
    const QString rel = QStringLiteral("image_resources/player/") + filename;
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/") + rel,
        appDir + QStringLiteral("/../") + rel,
        appDir + QStringLiteral("/../../") + rel,
        appDir + QStringLiteral("/../../../") + rel,
        QDir::currentPath() + QStringLiteral("/") + rel,
    };
    for (const QString& c : candidates) {
        if (QFile::exists(c)) return c;
    }
    return QString();
}

void PlayerControls::handleFileLoaded(const QString& /*path*/, int64_t frames,
                                       int /*channels*/, int sampleRate)
{
    m_total      = frames;
    m_sampleRate = sampleRate > 0 ? sampleRate : 44100;
    m_scrubber->setValue(0);
    m_timeNow->setText(formatTime(0));
    m_timeAll->setText(formatTime(frames));
    setEnabled(true);
}

void PlayerControls::handlePosition(int64_t samplePosition)
{
    if (m_total <= 0) return;
    if (!m_userScrub) {
        int v = static_cast<int>(
            10000.0 * static_cast<double>(samplePosition) /
            static_cast<double>(m_total));
        m_scrubber->blockSignals(true);
        m_scrubber->setValue(v);
        m_scrubber->blockSignals(false);
    }
    m_timeNow->setText(formatTime(samplePosition));
}

void PlayerControls::handlePlayState(bool /*playing*/)
{
    // Visual feedback handled by hover/press styling.
}

QString PlayerControls::formatTime(int64_t samples) const
{
    if (m_sampleRate <= 0) return QStringLiteral("00:00:00.000");
    double seconds = static_cast<double>(samples) / static_cast<double>(m_sampleRate);
    int hours = static_cast<int>(seconds) / 3600;
    int mins  = (static_cast<int>(seconds) / 60) % 60;
    int secs  = static_cast<int>(seconds) % 60;
    int ms    = static_cast<int>((seconds - std::floor(seconds)) * 1000.0);
    return QString("%1:%2:%3.%4")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(mins,  2, 10, QLatin1Char('0'))
        .arg(secs,  2, 10, QLatin1Char('0'))
        .arg(ms,    3, 10, QLatin1Char('0'));
}

} // namespace dawcast::editor
