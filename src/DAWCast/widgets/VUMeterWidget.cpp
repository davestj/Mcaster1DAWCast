// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VUMeterWidget.h"

#include <QPainter>
#include <QLinearGradient>
#include <QTimerEvent>
#include <QFont>

#include <cmath>

namespace dawcast::widgets {

VUMeterWidget::VUMeterWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(kScaleWidth + kMeterBarWidth + 4, 120);
    m_decayTimer = startTimer(kRefreshMs);
}

VUMeterWidget::~VUMeterWidget() = default;

QSize VUMeterWidget::sizeHint() const
{
    if (m_orientation == Qt::Vertical)
        return QSize(kScaleWidth + kMeterBarWidth + 6, 220);
    return QSize(220, kScaleWidth + kMeterBarWidth + 6);
}

QSize VUMeterWidget::minimumSizeHint() const
{
    if (m_orientation == Qt::Vertical)
        return QSize(kScaleWidth + kMeterBarWidth + 4, 100);
    return QSize(100, kScaleWidth + kMeterBarWidth + 4);
}

float VUMeterWidget::dbToNormalized(float db) const
{
    // Map dB range [m_minDb .. 0] to [0 .. 1] with a slight log-ish curve
    if (db <= m_minDb) return 0.0f;
    if (db >= 0.0f) return 1.0f;
    return (db - m_minDb) / (0.0f - m_minDb);
}

void VUMeterWidget::setLevel(float peakDb, float rmsDb)
{
    m_peakDb = qBound(m_minDb, peakDb, 6.0f);
    m_rmsDb  = qBound(m_minDb, rmsDb, 6.0f);

    if (m_peakDb > m_peakHoldDb) {
        m_peakHoldDb = m_peakDb;
        m_peakHoldCountdown = m_peakHoldMs / kRefreshMs;
    }

    update();
}

void VUMeterWidget::setPeakHoldTime(int ms)
{
    m_peakHoldMs = ms;
}

void VUMeterWidget::setOrientation(Qt::Orientation orientation)
{
    m_orientation = orientation;
    updateGeometry();
    update();
}

void VUMeterWidget::setDisplayMode(DisplayMode mode)
{
    m_displayMode = mode;
    update();
}

void VUMeterWidget::setMinDb(float db)
{
    m_minDb = db;
    update();
}

void VUMeterWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QRect widgetRect = rect();

    // Background
    p.fillRect(widgetRect, QColor(30, 30, 30));

    // Compute meter bar area (right side for vertical, leaving room for scale on left)
    QRect meterRect;
    if (m_orientation == Qt::Vertical) {
        int barX = widgetRect.right() - kMeterBarWidth - 2;
        meterRect = QRect(barX, widgetRect.top() + 4,
                          kMeterBarWidth, widgetRect.height() - 8);
    } else {
        int barY = widgetRect.bottom() - kMeterBarWidth - 2;
        meterRect = QRect(widgetRect.left() + 4, barY,
                          widgetRect.width() - 8, kMeterBarWidth);
    }

    // Black background for meter bar
    p.fillRect(meterRect, QColor(10, 10, 10));

    // ── Build meter gradient ────────────────────────────────────────────
    QLinearGradient gradient;
    if (m_orientation == Qt::Vertical) {
        gradient = QLinearGradient(0, meterRect.bottom(), 0, meterRect.top());
    } else {
        gradient = QLinearGradient(meterRect.left(), 0, meterRect.right(), 0);
    }
    gradient.setColorAt(0.00, QColor(0, 180, 0));       // -60 dB: green
    gradient.setColorAt(0.50, QColor(0, 220, 0));       // -30 dB: bright green
    gradient.setColorAt(0.70, QColor(220, 220, 0));     // -18 dB: yellow
    gradient.setColorAt(0.85, QColor(255, 160, 0));     // -9 dB:  orange
    gradient.setColorAt(0.95, QColor(255, 50, 0));      // -3 dB:  red-orange
    gradient.setColorAt(1.00, QColor(255, 0, 0));       //  0 dB:  red

    float rmsNorm  = dbToNormalized(m_rmsDb);
    float peakNorm = dbToNormalized(m_peakDb);
    float holdNorm = dbToNormalized(m_peakHoldDb);

    // ── Draw RMS fill ───────────────────────────────────────────────────
    p.setPen(Qt::NoPen);

    if (m_displayMode == Gradient) {
        p.setBrush(QBrush(gradient));
        if (m_orientation == Qt::Vertical) {
            int rmsH = static_cast<int>(rmsNorm * meterRect.height());
            if (rmsH > 0) {
                p.drawRect(meterRect.left(), meterRect.bottom() - rmsH + 1,
                           meterRect.width(), rmsH);
            }
        } else {
            int rmsW = static_cast<int>(rmsNorm * meterRect.width());
            if (rmsW > 0) {
                p.drawRect(meterRect.left(), meterRect.top(), rmsW, meterRect.height());
            }
        }
    } else {
        // Segmented mode
        const int segHeight = 3;
        const int segGap    = 1;
        const int segStep   = segHeight + segGap;

        if (m_orientation == Qt::Vertical) {
            int totalSegs = meterRect.height() / segStep;
            int litSegs   = static_cast<int>(rmsNorm * totalSegs);

            for (int s = 0; s < litSegs && s < totalSegs; ++s) {
                float segNorm = static_cast<float>(s) / totalSegs;
                // Pick color from gradient by sampling
                QColor segColor;
                if (segNorm < 0.50f)      segColor = QColor(0, 200, 0);
                else if (segNorm < 0.70f) segColor = QColor(180, 200, 0);
                else if (segNorm < 0.85f) segColor = QColor(220, 180, 0);
                else if (segNorm < 0.95f) segColor = QColor(255, 100, 0);
                else                      segColor = QColor(255, 0, 0);

                int y = meterRect.bottom() - (s + 1) * segStep + segGap;
                p.fillRect(meterRect.left() + 1, y,
                           meterRect.width() - 2, segHeight, segColor);
            }
        } else {
            int totalSegs = meterRect.width() / segStep;
            int litSegs   = static_cast<int>(rmsNorm * totalSegs);

            for (int s = 0; s < litSegs && s < totalSegs; ++s) {
                float segNorm = static_cast<float>(s) / totalSegs;
                QColor segColor;
                if (segNorm < 0.50f)      segColor = QColor(0, 200, 0);
                else if (segNorm < 0.70f) segColor = QColor(180, 200, 0);
                else if (segNorm < 0.85f) segColor = QColor(220, 180, 0);
                else if (segNorm < 0.95f) segColor = QColor(255, 100, 0);
                else                      segColor = QColor(255, 0, 0);

                int x = meterRect.left() + s * segStep;
                p.fillRect(x, meterRect.top() + 1,
                           segHeight, meterRect.height() - 2, segColor);
            }
        }
    }

    // ── Draw instantaneous peak as a thin bright line ───────────────────
    if (peakNorm > rmsNorm) {
        QColor peakColor(255, 255, 200, 180);
        p.setPen(QPen(peakColor, 1));
        if (m_orientation == Qt::Vertical) {
            int peakY = meterRect.bottom() - static_cast<int>(peakNorm * meterRect.height());
            p.drawLine(meterRect.left(), peakY, meterRect.right(), peakY);
        } else {
            int peakX = meterRect.left() + static_cast<int>(peakNorm * meterRect.width());
            p.drawLine(peakX, meterRect.top(), peakX, meterRect.bottom());
        }
    }

    // ── Draw peak hold indicator ────────────────────────────────────────
    if (holdNorm > 0.001f) {
        p.setPen(QPen(Qt::white, 2));
        if (m_orientation == Qt::Vertical) {
            int holdY = meterRect.bottom() - static_cast<int>(holdNorm * meterRect.height());
            p.drawLine(meterRect.left(), holdY, meterRect.right(), holdY);
        } else {
            int holdX = meterRect.left() + static_cast<int>(holdNorm * meterRect.width());
            p.drawLine(holdX, meterRect.top(), holdX, meterRect.bottom());
        }
    }

    // ── Draw dB scale markings on the left side ─────────────────────────
    drawScaleMarkings(p, meterRect);

    // Thin border around meter bar
    p.setPen(QPen(QColor(60, 60, 60), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(meterRect);
}

void VUMeterWidget::drawScaleMarkings(QPainter& p, const QRect& meterRect) const
{
    static const float dbMarks[] = { 0.0f, -3.0f, -6.0f, -12.0f, -20.0f, -40.0f, -60.0f };
    static const int   numMarks  = sizeof(dbMarks) / sizeof(dbMarks[0]);

    QFont f;
    f.setPointSize(7);
    f.setFamily(QStringLiteral("Menlo"));
    f.setStyleHint(QFont::Monospace);
    p.setFont(f);
    p.setPen(QColor(180, 180, 180));

    QFontMetrics fm(f);

    for (int i = 0; i < numMarks; ++i) {
        float norm = dbToNormalized(dbMarks[i]);

        QString label;
        int dbInt = static_cast<int>(dbMarks[i]);
        if (dbInt == 0)
            label = QStringLiteral(" 0");
        else
            label = QString::number(dbInt);

        if (m_orientation == Qt::Vertical) {
            int y = meterRect.bottom() - static_cast<int>(norm * meterRect.height());

            // Tick mark
            p.drawLine(meterRect.left() - 4, y, meterRect.left() - 1, y);

            // Label text
            int textW = fm.horizontalAdvance(label);
            int textX = meterRect.left() - 6 - textW;
            int textY = y + fm.ascent() / 2 - 1;
            p.drawText(textX, textY, label);
        } else {
            int x = meterRect.left() + static_cast<int>(norm * meterRect.width());
            p.drawLine(x, meterRect.top() - 4, x, meterRect.top() - 1);

            int textW = fm.horizontalAdvance(label);
            p.drawText(x - textW / 2, meterRect.top() - 6, label);
        }
    }
}

void VUMeterWidget::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != m_decayTimer) return;

    bool needsUpdate = false;

    // Decay peak hold after hold period expires
    if (m_peakHoldCountdown > 0) {
        m_peakHoldCountdown--;
    } else {
        // Decay peak hold: drop ~12 dB/second
        float decayRate = 12.0f * (static_cast<float>(kRefreshMs) / 1000.0f);
        if (m_peakHoldDb > m_minDb) {
            m_peakHoldDb -= decayRate;
            if (m_peakHoldDb < m_minDb) m_peakHoldDb = m_minDb;
            needsUpdate = true;
        }
    }

    // Decay RMS display smoothly (~20 dB/sec)
    float rmsDecay = 20.0f * (static_cast<float>(kRefreshMs) / 1000.0f);
    if (m_rmsDb > m_minDb) {
        m_rmsDb -= rmsDecay;
        if (m_rmsDb < m_minDb) m_rmsDb = m_minDb;
        needsUpdate = true;
    }

    // Decay peak display smoothly (~30 dB/sec)
    float peakDecay = 30.0f * (static_cast<float>(kRefreshMs) / 1000.0f);
    if (m_peakDb > m_minDb) {
        m_peakDb -= peakDecay;
        if (m_peakDb < m_minDb) m_peakDb = m_minDb;
        needsUpdate = true;
    }

    if (needsUpdate) update();
}

} // namespace dawcast::widgets
