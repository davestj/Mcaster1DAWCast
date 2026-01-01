// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "EmbossedKnob.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QConicalGradient>
#include <QRadialGradient>
#include <QFont>
#include <QFontMetrics>

#include <cmath>

namespace dawcast::widgets {

EmbossedKnob::EmbossedKnob(QWidget* parent)
    : QWidget(parent)
{
    // Default size: knob + value text above + label text below
    setMinimumSize(m_knobDiam + 8, m_knobDiam + 30);
    setFocusPolicy(Qt::StrongFocus);
}

EmbossedKnob::~EmbossedKnob() = default;

QSize EmbossedKnob::sizeHint() const
{
    return QSize(m_knobDiam + 12, m_knobDiam + 34);
}

QSize EmbossedKnob::minimumSizeHint() const
{
    return QSize(m_knobDiam + 4, m_knobDiam + 26);
}

float EmbossedKnob::normalizedValue() const
{
    if (m_max <= m_min) return 0.0f;
    return (m_value - m_min) / (m_max - m_min);
}

void EmbossedKnob::setValue(float val)
{
    val = qBound(m_min, val, m_max);
    if (!qFuzzyCompare(val + 1.0f, m_value + 1.0f)) {
        m_value = val;
        emit valueChanged(m_value);
        update();
    }
}

float EmbossedKnob::value() const { return m_value; }

void EmbossedKnob::setRange(float min, float max)
{
    m_min = min;
    m_max = max;
    setValue(m_value); // re-clamp
}

void EmbossedKnob::setLabel(const QString& label)
{
    m_label = label;
    update();
}

QString EmbossedKnob::label() const { return m_label; }

void EmbossedKnob::setSuffix(const QString& suffix)
{
    m_suffix = suffix;
    update();
}

void EmbossedKnob::setDecimals(int decimals)
{
    m_decimals = qBound(0, decimals, 4);
    update();
}

void EmbossedKnob::setArcColor(const QColor& color)
{
    m_arcColor = color;
    update();
}

void EmbossedKnob::setKnobSize(int diameter)
{
    m_knobDiam = qBound(24, diameter, 120);
    setMinimumSize(m_knobDiam + 8, m_knobDiam + 30);
    updateGeometry();
    update();
}

void EmbossedKnob::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int cx     = width() / 2;
    const int radius = m_knobDiam / 2 - 2;
    // Vertical layout: value text (14px) -> knob -> label text (14px)
    const int valueTextH = 14;
    const int labelTextH = 14;
    const int cy     = valueTextH + 2 + radius + 2;

    const float norm = normalizedValue();

    // ── Value text above knob ───────────────────────────────────────────
    {
        QFont vf;
        vf.setPointSize(9);
        vf.setFamily(QStringLiteral("Menlo"));
        vf.setStyleHint(QFont::Monospace);
        p.setFont(vf);
        p.setPen(QColor(220, 220, 220));

        QString valStr = QString::number(static_cast<double>(m_value), 'f', m_decimals);
        if (!m_suffix.isEmpty()) valStr += m_suffix;

        QRect valRect(0, 0, width(), valueTextH);
        p.drawText(valRect, Qt::AlignCenter, valStr);
    }

    // ── Background arc track (full 270-degree range) ────────────────────
    {
        int arcRad = radius + 5;
        QRect arcRect(cx - arcRad, cy - arcRad, arcRad * 2, arcRad * 2);

        // Background arc (dark)
        QPen arcBgPen(QColor(50, 50, 50), 3, Qt::SolidLine, Qt::RoundCap);
        p.setPen(arcBgPen);
        p.setBrush(Qt::NoBrush);
        // Qt arcs: start at 3 o'clock = 0, counter-clockwise positive
        // We want from 7 o'clock (225 deg) sweeping 270 deg clockwise
        // In Qt terms: start = -45*16 (= 315 deg = 5 o'clock), span = 270*16
        int startAngle16 = static_cast<int>((-45.0f) * 16);
        int spanAngle16  = static_cast<int>(kArcSpanDeg * 16);
        p.drawArc(arcRect, startAngle16, spanAngle16);

        // Active arc (colored, showing current value)
        if (norm > 0.001f) {
            QPen arcActivePen(m_arcColor, 3, Qt::SolidLine, Qt::RoundCap);
            p.setPen(arcActivePen);
            // Active portion spans from start angle by (norm * 270) degrees
            int activeSpan = static_cast<int>(norm * kArcSpanDeg * 16);
            p.drawArc(arcRect, startAngle16, activeSpan);
        }
    }

    // ── Embossed outer ring ─────────────────────────────────────────────
    {
        QRadialGradient ringGrad(cx - 1, cy - 1, radius + 3);
        ringGrad.setColorAt(0.00, QColor(110, 110, 115));
        ringGrad.setColorAt(0.70, QColor(70, 70, 75));
        ringGrad.setColorAt(0.90, QColor(50, 50, 55));
        ringGrad.setColorAt(1.00, QColor(35, 35, 40));
        p.setBrush(QBrush(ringGrad));
        p.setPen(QPen(QColor(20, 20, 20), 1));
        p.drawEllipse(QPoint(cx, cy), radius + 2, radius + 2);
    }

    // ── Inner shadow (embossed inset) ───────────────────────────────────
    {
        // Subtle inner shadow ring
        QRadialGradient innerShadow(cx + 1, cy + 1, radius);
        innerShadow.setColorAt(0.80, QColor(0, 0, 0, 0));
        innerShadow.setColorAt(0.95, QColor(0, 0, 0, 50));
        innerShadow.setColorAt(1.00, QColor(0, 0, 0, 80));
        p.setBrush(QBrush(innerShadow));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(cx, cy), radius, radius);
    }

    // ── Knob face with metallic gradient ────────────────────────────────
    {
        int faceR = radius - 2;
        // Light source from top-left
        QRadialGradient knobGrad(cx - faceR * 0.25, cy - faceR * 0.25, faceR * 1.2);
        knobGrad.setColorAt(0.00, QColor(195, 195, 200));
        knobGrad.setColorAt(0.40, QColor(150, 150, 158));
        knobGrad.setColorAt(0.80, QColor(100, 100, 108));
        knobGrad.setColorAt(1.00, QColor(70, 70, 78));
        p.setBrush(QBrush(knobGrad));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(cx, cy), faceR, faceR);

        // Subtle specular highlight on the top-left
        QRadialGradient spec(cx - faceR * 0.3, cy - faceR * 0.3, faceR * 0.5);
        spec.setColorAt(0.0, QColor(255, 255, 255, 50));
        spec.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setBrush(QBrush(spec));
        p.drawEllipse(QPoint(cx, cy), faceR, faceR);
    }

    // ── Indicator line ──────────────────────────────────────────────────
    {
        float angleDeg = kArcStartDeg - norm * kArcSpanDeg;
        float angleRad = angleDeg * static_cast<float>(M_PI) / 180.0f;

        int innerLen = radius - 10;
        int outerLen = radius - 3;

        // Start point (a bit off center)
        int sx = cx + static_cast<int>(static_cast<float>(innerLen) * 0.3f * std::cos(angleRad));
        int sy = cy - static_cast<int>(static_cast<float>(innerLen) * 0.3f * std::sin(angleRad));

        // End point (near edge)
        int ex = cx + static_cast<int>(static_cast<float>(outerLen) * std::cos(angleRad));
        int ey = cy - static_cast<int>(static_cast<float>(outerLen) * std::sin(angleRad));

        p.setPen(QPen(Qt::white, 2.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(sx, sy, ex, ey);

        // Small dot at the tip
        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(ex, ey), 2, 2);
    }

    // ── Label text below knob ───────────────────────────────────────────
    if (!m_label.isEmpty()) {
        QFont lf;
        lf.setPointSize(8);
        p.setFont(lf);
        p.setPen(QColor(170, 170, 170));

        QRect labelRect(0, cy + radius + 6, width(), labelTextH);
        p.drawText(labelRect, Qt::AlignCenter, m_label);
    }
}

void EmbossedKnob::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastY = event->pos().y();
        setCursor(Qt::BlankCursor);
    }
}

void EmbossedKnob::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        int dy = m_lastY - event->pos().y();
        m_lastY = event->pos().y();

        // Sensitivity: full range = ~200 pixels of drag
        float sensitivity = (m_max - m_min) / 200.0f;

        // Hold Shift for fine adjustment
        if (event->modifiers() & Qt::ShiftModifier)
            sensitivity *= 0.1f;

        setValue(m_value + static_cast<float>(dy) * sensitivity);
    }
}

void EmbossedKnob::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        unsetCursor();
    }
}

void EmbossedKnob::wheelEvent(QWheelEvent* event)
{
    float step = (m_max - m_min) / 100.0f;
    if (event->modifiers() & Qt::ShiftModifier)
        step *= 0.1f;

    float delta = (event->angleDelta().y() > 0) ? step : -step;
    setValue(m_value + delta);
    event->accept();
}

} // namespace dawcast::widgets
