// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TransportBar.h"
#include "BevelButton.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QFrame>
#include <QTimer>

namespace dawcast::widgets {

// Helper to format sample position as timecode
static QString formatTimecode(int64_t samples, int sampleRate)
{
    if (sampleRate <= 0) sampleRate = 44100;
    double totalSec = static_cast<double>(samples) / sampleRate;
    int hours   = static_cast<int>(totalSec) / 3600;
    int minutes = (static_cast<int>(totalSec) % 3600) / 60;
    int seconds = static_cast<int>(totalSec) % 60;
    int millis  = static_cast<int>((totalSec - static_cast<int>(totalSec)) * 1000);

    return QStringLiteral("%1:%2:%3.%4")
        .arg(hours,   2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(millis,  3, 10, QLatin1Char('0'));
}

TransportBar::TransportBar(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 3, 6, 3);
    layout->setSpacing(4);

    // ── Transport Buttons ───────────────────────────────────────────────

    const QSize btnSize(42, 30);

    m_rewindBtn = new BevelButton(QStringLiteral("\u23EA"), this);
    m_rewindBtn->setFixedSize(btnSize);
    m_rewindBtn->setToolTip(tr("Rewind"));

    m_playBtn = new BevelButton(QStringLiteral("\u25B6"), this);
    m_playBtn->setFixedSize(btnSize);
    m_playBtn->setCheckable(true);
    m_playBtn->setToolTip(tr("Play"));

    m_pauseBtn = new BevelButton(QStringLiteral("\u23F8"), this);
    m_pauseBtn->setFixedSize(btnSize);
    m_pauseBtn->setToolTip(tr("Pause"));

    m_stopBtn = new BevelButton(QStringLiteral("\u23F9"), this);
    m_stopBtn->setFixedSize(btnSize);
    m_stopBtn->setToolTip(tr("Stop"));

    m_recordBtn = new BevelButton(QStringLiteral("\u23FA"), this);
    m_recordBtn->setFixedSize(btnSize);
    m_recordBtn->setCheckable(true);
    m_recordBtn->setToolTip(tr("Record"));

    m_ffBtn = new BevelButton(QStringLiteral("\u23E9"), this);
    m_ffBtn->setFixedSize(btnSize);
    m_ffBtn->setToolTip(tr("Fast Forward"));

    m_loopBtn = new BevelButton(QStringLiteral("\u21BB"), this);
    m_loopBtn->setFixedSize(btnSize);
    m_loopBtn->setCheckable(true);
    m_loopBtn->setToolTip(tr("Loop"));

    layout->addWidget(m_rewindBtn);
    layout->addWidget(m_playBtn);
    layout->addWidget(m_pauseBtn);
    layout->addWidget(m_stopBtn);
    layout->addWidget(m_recordBtn);
    layout->addWidget(m_ffBtn);

    // Small visual separator before loop
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setFixedWidth(2);
    layout->addWidget(sep);

    layout->addWidget(m_loopBtn);

    // ── Metronome ───────────────────────────────────────────────────────

    m_metronomeBtn = new BevelButton(QStringLiteral("\u266A"), this);  // musical note
    m_metronomeBtn->setFixedSize(btnSize);
    m_metronomeBtn->setCheckable(true);
    m_metronomeBtn->setToolTip(tr("Metronome"));
    layout->addWidget(m_metronomeBtn);

    // Tempo spinner
    m_tempoSpin = new QDoubleSpinBox(this);
    m_tempoSpin->setRange(20.0, 300.0);
    m_tempoSpin->setValue(120.0);
    m_tempoSpin->setSuffix(QStringLiteral(" BPM"));
    m_tempoSpin->setDecimals(1);
    m_tempoSpin->setSingleStep(1.0);
    m_tempoSpin->setFixedWidth(100);
    m_tempoSpin->setToolTip(tr("Tempo (BPM)"));
    layout->addWidget(m_tempoSpin);

    // Beat indicator (small dot that flashes on each beat)
    m_beatIndicator = new QLabel(QStringLiteral("\u25CF"), this);  // filled circle
    m_beatIndicator->setFixedSize(18, 18);
    m_beatIndicator->setAlignment(Qt::AlignCenter);
    m_beatIndicator->setStyleSheet(
        QStringLiteral("QLabel { color: #555555; font-size: 14px; }"));
    m_beatIndicator->setToolTip(tr("Beat indicator"));
    layout->addWidget(m_beatIndicator);

    // Small separator before time display
    auto* sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFrameShadow(QFrame::Sunken);
    sep2->setFixedWidth(2);
    layout->addWidget(sep2);

    // ── Time Display ────────────────────────────────────────────────────

    layout->addSpacing(12);

    m_timeDisplay = new QLabel(
        QStringLiteral("00:00:00.000 / 00:00:00.000"), this);
    m_timeDisplay->setAlignment(Qt::AlignCenter);

    QFont monoFont(QStringLiteral("Menlo"));
    if (!monoFont.exactMatch()) {
        monoFont.setFamily(QStringLiteral("Courier New"));
    }
    monoFont.setPointSize(13);
    monoFont.setStyleHint(QFont::Monospace);
    m_timeDisplay->setFont(monoFont);
    m_timeDisplay->setMinimumWidth(280);

    // Dark inset frame around the time display
    m_timeDisplay->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_timeDisplay->setStyleSheet(
        QStringLiteral("QLabel { background-color: #1a1a1a; color: #00ff88; "
                        "padding: 4px 8px; border-radius: 3px; }"));

    layout->addWidget(m_timeDisplay);

    layout->addStretch();

    // ── Connections ─────────────────────────────────────────────────────

    connect(m_rewindBtn, &BevelButton::clicked, this, &TransportBar::rewindClicked);
    connect(m_playBtn,   &BevelButton::clicked, this, &TransportBar::playClicked);
    connect(m_pauseBtn,  &BevelButton::clicked, this, &TransportBar::pauseClicked);
    connect(m_stopBtn,   &BevelButton::clicked, this, &TransportBar::stopClicked);
    connect(m_recordBtn, &BevelButton::clicked, this, &TransportBar::recordClicked);
    connect(m_ffBtn,     &BevelButton::clicked, this, &TransportBar::fastForwardClicked);
    connect(m_loopBtn,       &BevelButton::toggled, this, &TransportBar::loopToggled);
    connect(m_metronomeBtn,  &BevelButton::toggled, this, &TransportBar::metronomeToggled);
    connect(m_tempoSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TransportBar::tempoChanged);

    // Metronome button highlight: orange when checked
    connect(m_metronomeBtn, &BevelButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_metronomeBtn->setHighlightColor(QColor(255, 180, 40, 200));
        } else {
            m_metronomeBtn->setHighlightColor(QColor(255, 255, 255, 120));
        }
        m_metronomeBtn->update();
    });

    // Play button highlight: green when checked
    connect(m_playBtn, &BevelButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_playBtn->setHighlightColor(QColor(60, 220, 60, 180));
        } else {
            m_playBtn->setHighlightColor(QColor(255, 255, 255, 120));
        }
        m_playBtn->update();
    });

    // Record button highlight: red when checked
    connect(m_recordBtn, &BevelButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_recordBtn->setHighlightColor(QColor(220, 40, 40, 200));
        } else {
            m_recordBtn->setHighlightColor(QColor(255, 255, 255, 120));
        }
        m_recordBtn->update();
    });

    // Stop resets play and record toggle states
    connect(m_stopBtn, &BevelButton::clicked, this, [this]() {
        m_playBtn->setChecked(false);
        m_recordBtn->setChecked(false);
    });
}

TransportBar::~TransportBar() = default;

void TransportBar::setPlaying(bool playing)
{
    m_playing = playing;
    m_playBtn->setChecked(playing);
}

void TransportBar::setRecording(bool recording)
{
    m_recording = recording;
    m_recordBtn->setChecked(recording);
}

void TransportBar::setPosition(int64_t samples, int sampleRate)
{
    m_position   = samples;
    m_sampleRate = sampleRate > 0 ? sampleRate : 44100;
    updateTimeDisplay();
}

void TransportBar::setDuration(int64_t samples, int sampleRate)
{
    m_duration   = samples;
    m_sampleRate = sampleRate > 0 ? sampleRate : m_sampleRate;
    updateTimeDisplay();
}

void TransportBar::flashBeat(int /*beatNumber*/, bool isDownbeat)
{
    // Flash the beat indicator: bright color for 80ms, then dim
    if (isDownbeat) {
        m_beatIndicator->setStyleSheet(
            QStringLiteral("QLabel { color: #ff6600; font-size: 14px; }"));
    } else {
        m_beatIndicator->setStyleSheet(
            QStringLiteral("QLabel { color: #ffcc00; font-size: 14px; }"));
    }

    // Reset after 80ms
    QTimer::singleShot(80, this, [this]() {
        m_beatIndicator->setStyleSheet(
            QStringLiteral("QLabel { color: #555555; font-size: 14px; }"));
    });
}

void TransportBar::updateTimeDisplay()
{
    QString posStr = formatTimecode(m_position, m_sampleRate);
    QString durStr = formatTimecode(m_duration, m_sampleRate);
    m_timeDisplay->setText(posStr + QStringLiteral(" / ") + durStr);
}

} // namespace dawcast::widgets
