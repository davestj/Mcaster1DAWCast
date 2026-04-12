// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LUFSMeterWidget.h"

#include <QPainter>
#include <QLinearGradient>
#include <QTimerEvent>
#include <QFont>
#include <QFontMetrics>

#include <cmath>
#include <algorithm>

namespace dawcast::widgets {

LUFSMeterWidget::LUFSMeterWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(kScaleWidth + kMeterBarWidth + 6, 200);
    m_decayTimer = startTimer(kRefreshMs);
}

LUFSMeterWidget::~LUFSMeterWidget() = default;

QSize LUFSMeterWidget::sizeHint() const
{
    return QSize(80, 340);
}

QSize LUFSMeterWidget::minimumSizeHint() const
{
    return QSize(kScaleWidth + kMeterBarWidth + 6, 200);
}

// ── Public setters ─────────────────────────────────────────────────────────

void LUFSMeterWidget::setMomentaryLUFS(float lufs)
{
    m_momentaryLUFS = std::clamp(lufs, kMinLUFS, kMaxLUFS);
    update();
}

void LUFSMeterWidget::setShortTermLUFS(float lufs)
{
    m_shortTermLUFS = std::clamp(lufs, kMinLUFS, kMaxLUFS);
    update();
}

void LUFSMeterWidget::setIntegratedLUFS(float lufs)
{
    m_integratedLUFS = std::clamp(lufs, kMinLUFS, kMaxLUFS);
    update();
}

void LUFSMeterWidget::setLoudnessRange(float lu)
{
    m_loudnessRange = std::max(0.0f, lu);
    update();
}

void LUFSMeterWidget::setTruePeak(float dbTP)
{
    m_truePeak = std::clamp(dbTP, kMinLUFS, kMaxLUFS);
    update();
}

void LUFSMeterWidget::reset()
{
    m_momentaryLUFS  = kMinLUFS;
    m_shortTermLUFS  = kMinLUFS;
    m_integratedLUFS = kMinLUFS;
    m_loudnessRange  = 0.0f;
    m_truePeak       = kMinLUFS;
    m_displayMomentary = kMinLUFS;
    m_displayShortTerm = kMinLUFS;
    update();
}

// ── Helpers ────────────────────────────────────────────────────────────────

float LUFSMeterWidget::lufsToNormalized(float lufs) const
{
    // Map [kMinLUFS .. kMaxLUFS] to [0 .. 1]
    if (lufs <= kMinLUFS) return 0.0f;
    if (lufs >= kMaxLUFS) return 1.0f;
    return (lufs - kMinLUFS) / (kMaxLUFS - kMinLUFS);
}

QColor LUFSMeterWidget::colorForLufs(float lufs) const
{
    if (lufs > -9.0f)  return QColor(255, 0, 0);       // red
    if (lufs > -16.0f) return QColor(255, 160, 0);     // orange
    if (lufs > -24.0f) return QColor(220, 220, 0);     // yellow
    return QColor(0, 200, 0);                           // green
}

// ── Paint ──────────────────────────────────────────────────────────────────

void LUFSMeterWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QRect widgetRect = rect();

    // Background — light theme
    p.fillRect(widgetRect, QColor(240, 240, 244));

    // ── "LUFS" label at top ────────────────────────────────────────────
    QFont labelFont;
    labelFont.setPointSize(9);
    labelFont.setFamily(QStringLiteral("Menlo"));
    labelFont.setBold(true);
    p.setFont(labelFont);
    p.setPen(QColor(26, 26, 26));
    QRect labelRect(widgetRect.left(), widgetRect.top() + 2,
                    widgetRect.width(), kLabelHeight);
    p.drawText(labelRect, Qt::AlignCenter, QStringLiteral("LUFS"));

    // ── Meter bar area (between label and readouts) ────────────────────
    int meterTop    = widgetRect.top() + kLabelHeight + 4;
    int meterBottom = widgetRect.bottom() - kReadoutHeight - 4;
    int meterHeight = meterBottom - meterTop;
    if (meterHeight < 40) meterHeight = 40;

    int barX = widgetRect.right() - kMeterBarWidth - 2;
    QRect meterRect(barX, meterTop, kMeterBarWidth, meterHeight);

    // Light background for meter bar (so the bright gradient still pops)
    p.fillRect(meterRect, QColor(220, 220, 226));

    // ── Draw target zone band ──────────────────────────────────────────
    drawTargetZone(p, meterRect);

    // ── Build meter gradient ───────────────────────────────────────────
    QLinearGradient gradient(0, meterRect.bottom(), 0, meterRect.top());
    gradient.setColorAt(0.00, QColor(0, 180, 0));       // -60 LUFS: green
    gradient.setColorAt(0.36, QColor(0, 220, 0));       // ~-36 LUFS: bright green
    gradient.setColorAt(0.55, QColor(220, 220, 0));     // ~-24 LUFS: yellow
    gradient.setColorAt(0.67, QColor(255, 200, 0));     // ~-16 LUFS: warm yellow
    gradient.setColorAt(0.77, QColor(255, 160, 0));     // ~-9 LUFS: orange
    gradient.setColorAt(0.91, QColor(255, 50, 0));      // ~0 LUFS: red-orange
    gradient.setColorAt(1.00, QColor(255, 0, 0));       // +6 LUFS: red

    // ── Draw momentary LUFS fill ───────────────────────────────────────
    float momentaryNorm = lufsToNormalized(m_displayMomentary);
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(gradient));
    int fillH = static_cast<int>(momentaryNorm * meterRect.height());
    if (fillH > 0) {
        p.drawRect(meterRect.left(), meterRect.bottom() - fillH + 1,
                   meterRect.width(), fillH);
    }

    // ── Short-term indicator (thin horizontal line) ────────────────────
    float shortTermNorm = lufsToNormalized(m_displayShortTerm);
    if (shortTermNorm > 0.001f) {
        int stY = meterRect.bottom() - static_cast<int>(shortTermNorm * meterRect.height());
        p.setPen(QPen(QColor(20, 20, 20, 220), 2));
        p.drawLine(meterRect.left(), stY, meterRect.right(), stY);
    }

    // ── Thin border around meter bar ───────────────────────────────────
    p.setPen(QPen(QColor(170, 170, 178), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(meterRect);

    // ── Scale markings on the left ─────────────────────────────────────
    drawScaleMarkings(p, meterRect);

    // ── Numeric readouts at the bottom ─────────────────────────────────
    QRect readoutArea(widgetRect.left(), meterBottom + 6,
                      widgetRect.width(), kReadoutHeight);
    drawReadouts(p, readoutArea);
}

void LUFSMeterWidget::drawTargetZone(QPainter& p, const QRect& meterRect) const
{
    float normLow  = lufsToNormalized(kTargetLow);
    float normHigh = lufsToNormalized(kTargetHigh);

    int yHigh = meterRect.bottom() - static_cast<int>(normHigh * meterRect.height());
    int yLow  = meterRect.bottom() - static_cast<int>(normLow  * meterRect.height());

    // Semi-transparent green band for the target zone
    p.fillRect(meterRect.left(), yHigh,
               meterRect.width(), yLow - yHigh,
               QColor(0, 180, 0, 40));

    // Thin green border lines at target edges
    p.setPen(QPen(QColor(0, 200, 0, 120), 1, Qt::DashLine));
    p.drawLine(meterRect.left(), yHigh, meterRect.right(), yHigh);
    p.drawLine(meterRect.left(), yLow, meterRect.right(), yLow);
}

void LUFSMeterWidget::drawScaleMarkings(QPainter& p, const QRect& meterRect) const
{
    static const float lufsMarks[] = {
        0.0f, -6.0f, -12.0f, -16.0f, -24.0f, -36.0f, -48.0f, -60.0f
    };
    static const int numMarks = sizeof(lufsMarks) / sizeof(lufsMarks[0]);

    QFont f;
    f.setPointSize(7);
    f.setFamily(QStringLiteral("Menlo"));
    f.setStyleHint(QFont::Monospace);
    p.setFont(f);
    p.setPen(QColor(60, 60, 70));

    QFontMetrics fm(f);

    for (int i = 0; i < numMarks; ++i) {
        float norm = lufsToNormalized(lufsMarks[i]);
        int y = meterRect.bottom() - static_cast<int>(norm * meterRect.height());

        // Tick mark
        p.drawLine(meterRect.left() - 4, y, meterRect.left() - 1, y);

        // Label text
        int lufsInt = static_cast<int>(lufsMarks[i]);
        QString label;
        if (lufsInt == 0)
            label = QStringLiteral(" 0");
        else
            label = QString::number(lufsInt);

        int textW = fm.horizontalAdvance(label);
        int textX = meterRect.left() - 6 - textW;
        int textY = y + fm.ascent() / 2 - 1;
        p.drawText(textX, textY, label);
    }
}

void LUFSMeterWidget::drawReadouts(QPainter& p, const QRect& area) const
{
    QFont f;
    f.setPointSize(8);
    f.setFamily(QStringLiteral("Menlo"));
    f.setStyleHint(QFont::Monospace);
    p.setFont(f);

    QFontMetrics fm(f);
    int lineH = fm.height() + 2;
    int y = area.top();

    // Integrated LUFS
    p.setPen(QColor(26, 26, 26));
    QString intText = QStringLiteral("INT:%1")
        .arg(m_integratedLUFS <= kMinLUFS
             ? QStringLiteral(" ---")
             : QString::number(static_cast<double>(m_integratedLUFS), 'f', 1));
    p.drawText(area.left() + 2, y + fm.ascent(), intText);
    y += lineH;

    // LRA
    p.setPen(QColor(60, 60, 70));
    QString lraText = QStringLiteral("LRA:%1")
        .arg(m_loudnessRange < 0.1f
             ? QStringLiteral(" ---")
             : QString::number(static_cast<double>(m_loudnessRange), 'f', 1));
    p.drawText(area.left() + 2, y + fm.ascent(), lraText);
    y += lineH;

    // True Peak — red if above -1.0 dBTP
    bool tpHot = m_truePeak > -1.0f;
    p.setPen(tpHot ? QColor(200, 0, 0) : QColor(40, 90, 40));
    QString tpText = QStringLiteral("TP: %1")
        .arg(m_truePeak <= kMinLUFS
             ? QStringLiteral("---")
             : QString::number(static_cast<double>(m_truePeak), 'f', 1));
    p.drawText(area.left() + 2, y + fm.ascent(), tpText);
}

// ── Timer-based decay animation ────────────────────────────────────────────

void LUFSMeterWidget::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != m_decayTimer) return;

    bool needsUpdate = false;

    // Smooth attack/release for momentary display
    float targetMom = m_momentaryLUFS;
    if (targetMom > m_displayMomentary) {
        // Fast attack
        m_displayMomentary = targetMom;
        needsUpdate = true;
    } else {
        // Smooth decay: ~20 dB/sec
        float decay = 20.0f * (static_cast<float>(kRefreshMs) / 1000.0f);
        if (m_displayMomentary > kMinLUFS) {
            m_displayMomentary -= decay;
            if (m_displayMomentary < kMinLUFS) m_displayMomentary = kMinLUFS;
            needsUpdate = true;
        }
    }

    // Smooth attack/release for short-term display
    float targetST = m_shortTermLUFS;
    if (targetST > m_displayShortTerm) {
        m_displayShortTerm = targetST;
        needsUpdate = true;
    } else {
        float decay = 10.0f * (static_cast<float>(kRefreshMs) / 1000.0f);
        if (m_displayShortTerm > kMinLUFS) {
            m_displayShortTerm -= decay;
            if (m_displayShortTerm < kMinLUFS) m_displayShortTerm = kMinLUFS;
            needsUpdate = true;
        }
    }

    if (needsUpdate) update();
}

} // namespace dawcast::widgets
