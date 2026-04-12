// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "MasterStrip.h"

#include <QHBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <cmath>

namespace dawcast::widgets {

// ── Tiny LUFS bar widget (compact vertical meter) ──────────────────────────

class CompactLufsMeter : public QWidget
{
public:
    explicit CompactLufsMeter(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(8, 24);
    }

    void setLevel(float lufs)
    {
        m_lufs = lufs;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Background
        p.fillRect(rect(), QColor(0x12, 0x14, 0x22));

        // Normalize LUFS (-24..0) to 0..1
        float norm = (m_lufs + 24.0f) / 24.0f;
        norm = qBound(0.0f, norm, 1.0f);

        int barH = static_cast<int>(norm * height());
        if (barH < 1 && m_lufs > -60.0f) barH = 1;

        QRect barRect(0, height() - barH, width(), barH);

        // Color: green -> yellow -> red
        QColor barColor;
        if (norm < 0.6f)
            barColor = QColor(0x3e, 0xa8, 0xa0);      // teal/green
        else if (norm < 0.85f)
            barColor = QColor(0xd4, 0xaa, 0x30);       // yellow
        else
            barColor = QColor(0xe0, 0x40, 0x40);       // red

        p.fillRect(barRect, barColor);
    }

private:
    float m_lufs = -60.0f;
};

// ── MasterStrip ────────────────────────────────────────────────────────────

MasterStrip::MasterStrip(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(36);
    setObjectName(QStringLiteral("MasterStrip"));

    setStyleSheet(QStringLiteral(
        "QWidget#MasterStrip {"
        "  background-color: #ececf0;"
        "  border-top: 1px solid #c8c8d0;"
        "  border-bottom: 1px solid #c8c8d0;"
        "}"
    ));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 2, 10, 2);
    layout->setSpacing(8);

    // ── MASTER label ───────────────────────────────────────────────────

    m_masterLabel = new QLabel(QStringLiteral("MASTER"), this);
    m_masterLabel->setToolTip(tr("Master output level - Controls the final volume of the entire mix"));
    m_masterLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: #1a1a1a;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "  letter-spacing: 1px;"
        "  background: transparent;"
        "  border: none;"
        "}"
    ));
    m_masterLabel->setFixedWidth(58);
    layout->addWidget(m_masterLabel);

    // ── Horizontal Fader (QSlider) ─────────────────────────────────────

    m_fader = new QSlider(Qt::Horizontal, this);
    m_fader->setRange(kSliderMin, kSliderMax);
    m_fader->setSingleStep(1);
    m_fader->setPageStep(50);
    // Default: 0 dB -> map to slider position
    int defaultPos = static_cast<int>(
        (0.0f - kMinDb) / (kMaxDb - kMinDb) * kSliderMax);
    m_fader->setValue(defaultPos);

    m_fader->setStyleSheet(QStringLiteral(
        "QSlider::groove:horizontal {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #d0d0d8, stop:1 #c0c0c8);"
        "  height: 6px;"
        "  border-radius: 3px;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #2a7a74, stop:1 #3ea8a0);"
        "  height: 6px;"
        "  border-radius: 3px;"
        "}"
        "QSlider::handle:horizontal {"
        "  background: #ffffff;"
        "  border: 2px solid #3ea8a0;"
        "  width: 14px;"
        "  height: 14px;"
        "  margin: -5px 0;"
        "  border-radius: 8px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "  background: #f0f0f0;"
        "  border-color: #4bbab2;"
        "}"
    ));

    m_fader->setToolTip(tr("Master Fader: 0.0 dB - Drag to adjust the master output volume"));
    layout->addWidget(m_fader, 1);  // stretch factor 1

    // ── Percentage / dB display ────────────────────────────────────────

    m_percentLabel = new QLabel(QStringLiteral("100%"), this);
    m_percentLabel->setToolTip(tr("Master volume as percentage (100% = 0 dB)"));
    m_percentLabel->setFixedWidth(44);
    m_percentLabel->setAlignment(Qt::AlignCenter);
    m_percentLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: #1a1a1a;"
        "  font-size: 11px;"
        "  font-family: monospace;"
        "  background: transparent;"
        "  border: none;"
        "}"
    ));
    layout->addWidget(m_percentLabel);

    // ── Compact LUFS meter ─────────────────────────────────────────────

    auto* lufsMeter = new CompactLufsMeter(this);
    m_lufsMeter = lufsMeter;
    layout->addWidget(m_lufsMeter);

    m_lufsLabel = new QLabel(QStringLiteral("-- LUFS"), this);
    m_lufsLabel->setToolTip(tr("Loudness Units Full Scale - Real-time loudness measurement of the master output"));
    m_lufsLabel->setFixedWidth(62);
    m_lufsLabel->setAlignment(Qt::AlignCenter);
    m_lufsLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: #1a1a1a;"
        "  font-size: 10px;"
        "  font-family: monospace;"
        "  background: transparent;"
        "  border: none;"
        "}"
    ));
    layout->addWidget(m_lufsLabel);

    // ── LIM indicator ──────────────────────────────────────────────────

    m_limIndicator = new QLabel(QStringLiteral("LIM"), this);
    m_limIndicator->setToolTip(tr("Limiter indicator - Lights up red when the output limiter is actively reducing peaks"));
    m_limIndicator->setFixedSize(32, 18);
    m_limIndicator->setAlignment(Qt::AlignCenter);
    m_limIndicator->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: #888;"
        "  font-size: 9px;"
        "  font-weight: bold;"
        "  background-color: #f0f0f4;"
        "  border: 1px solid #c8c8d0;"
        "  border-radius: 3px;"
        "}"
    ));
    layout->addWidget(m_limIndicator);

    // ── Fader signal ───────────────────────────────────────────────────

    connect(m_fader, &QSlider::valueChanged, this, [this](int value) {
        float db = kMinDb + (static_cast<float>(value) / kSliderMax) * (kMaxDb - kMinDb);
        m_currentDb = db;
        updatePercentLabel();
        QString dbStr = (db > 0.0f)
            ? QStringLiteral("+%1 dB").arg(static_cast<double>(db), 0, 'f', 1)
            : QStringLiteral("%1 dB").arg(static_cast<double>(db), 0, 'f', 1);
        m_fader->setToolTip(tr("Master Fader: %1 - Drag to adjust the master output volume").arg(dbStr));
        emit levelChanged(db);
    });
}

void MasterStrip::setLevel(float db)
{
    m_currentDb = qBound(kMinDb, db, kMaxDb);
    int pos = static_cast<int>(
        (m_currentDb - kMinDb) / (kMaxDb - kMinDb) * kSliderMax);
    m_fader->blockSignals(true);
    m_fader->setValue(pos);
    m_fader->blockSignals(false);
    updatePercentLabel();
}

void MasterStrip::setLUFS(float lufs)
{
    m_currentLufs = lufs;
    updateLufsDisplay();
}

void MasterStrip::setLimiterActive(bool active)
{
    m_limiterOn = active;
    if (active) {
        m_limIndicator->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  color: #ffffff;"
            "  font-size: 9px;"
            "  font-weight: bold;"
            "  background-color: #d03030;"
            "  border: 1px solid #e04040;"
            "  border-radius: 3px;"
            "}"
        ));
    } else {
        m_limIndicator->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  color: #555870;"
            "  font-size: 9px;"
            "  font-weight: bold;"
            "  background-color: #1e2236;"
            "  border: 1px solid #2a2f42;"
            "  border-radius: 3px;"
            "}"
        ));
    }
}

float MasterStrip::levelDb() const
{
    return m_currentDb;
}

void MasterStrip::updatePercentLabel()
{
    // Convert dB to percentage (0 dB = 100%)
    // percentage = 10^(dB/20) * 100
    float pct = std::pow(10.0f, m_currentDb / 20.0f) * 100.0f;
    if (pct < 0.5f)
        m_percentLabel->setText(QStringLiteral("0%"));
    else
        m_percentLabel->setText(QStringLiteral("%1%").arg(qRound(pct)));
}

void MasterStrip::updateLufsDisplay()
{
    // Update the numeric label
    if (m_currentLufs <= -60.0f) {
        m_lufsLabel->setText(QStringLiteral("-- LUFS"));
    } else {
        m_lufsLabel->setText(QStringLiteral("%1 LUFS")
                                 .arg(m_currentLufs, 0, 'f', 1));
    }

    // Update the compact meter bar
    auto* meter = static_cast<CompactLufsMeter*>(m_lufsMeter);
    if (meter) meter->setLevel(m_currentLufs);
}

} // namespace dawcast::widgets
