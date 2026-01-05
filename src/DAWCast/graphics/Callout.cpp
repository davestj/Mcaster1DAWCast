// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Callout.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QFontMetrics>
#include <QRectF>
#include <cmath>

namespace dawcast {

Callout::Callout(QObject *parent)
    : QObject(parent)
{
    m_font.setPixelSize(14);
}

Callout::~Callout() = default;

void Callout::setPosition(const QPointF &pos)
{
    m_position = pos;
}

void Callout::setText(const QString &text)
{
    m_text = text;
}

void Callout::setArrowTarget(const QPointF &target)
{
    m_arrowTarget = target;
}

void Callout::setStyle(const QColor &color, const QFont &font)
{
    m_color = color;
    m_font = font;
}

QPointF Callout::position() const { return m_position; }
QString Callout::text() const { return m_text; }
QPointF Callout::arrowTarget() const { return m_arrowTarget; }

void Callout::render(QPainter &painter)
{
    if (m_text.isEmpty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setFont(m_font);

    QFontMetrics fm(m_font);

    // Calculate text bounding box with padding
    const int padding = 10;
    const int borderWidth = 2;
    QRectF textBounds = fm.boundingRect(m_text);
    QRectF boxRect(
        m_position.x() - padding,
        m_position.y() - textBounds.height() - padding,
        textBounds.width() + padding * 2,
        textBounds.height() + padding * 2);

    // Find the closest point on the box edge to the arrow target
    // (center of the box bottom edge as the arrow origin)
    QPointF boxCenter(boxRect.center().x(), boxRect.bottom());

    // --- Draw arrow line from box to target ---
    painter.setPen(QPen(m_color, borderWidth, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(boxCenter, m_arrowTarget);

    // --- Draw arrowhead triangle at the target end ---
    const double arrowSize = 10.0;
    double dx = m_arrowTarget.x() - boxCenter.x();
    double dy = m_arrowTarget.y() - boxCenter.y();
    double lineLength = std::sqrt(dx * dx + dy * dy);

    if (lineLength > 0.01) {
        // Unit vector along the line
        double ux = dx / lineLength;
        double uy = dy / lineLength;

        // Perpendicular vector
        double px = -uy;
        double py =  ux;

        // Arrowhead vertices
        QPointF tip = m_arrowTarget;
        QPointF left(
            tip.x() - ux * arrowSize + px * arrowSize * 0.4,
            tip.y() - uy * arrowSize + py * arrowSize * 0.4);
        QPointF right(
            tip.x() - ux * arrowSize - px * arrowSize * 0.4,
            tip.y() - uy * arrowSize - py * arrowSize * 0.4);

        QPainterPath arrowHead;
        arrowHead.moveTo(tip);
        arrowHead.lineTo(left);
        arrowHead.lineTo(right);
        arrowHead.closeSubpath();

        painter.setPen(Qt::NoPen);
        painter.setBrush(m_color);
        painter.drawPath(arrowHead);
    }

    // --- Draw text box background with border ---
    QColor bgColor(0, 0, 0, 200);
    QColor borderColor = m_color;
    borderColor.setAlpha(180);

    painter.setPen(QPen(borderColor, borderWidth));
    painter.setBrush(bgColor);
    painter.drawRoundedRect(boxRect, 5, 5);

    // --- Draw text centered in the box ---
    painter.setPen(m_color);
    painter.drawText(boxRect, Qt::AlignCenter, m_text);

    painter.restore();
}

} // namespace dawcast
