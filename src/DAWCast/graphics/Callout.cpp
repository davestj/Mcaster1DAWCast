// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Callout.h"

#include <QPainter>
#include <QFontMetrics>
#include <QRectF>

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

    painter.setFont(m_font);
    QFontMetrics fm(m_font);

    // Text bounding box with padding
    const int padding = 8;
    QRectF textRect = fm.boundingRect(m_text);
    textRect.moveTo(m_position);
    textRect.adjust(-padding, -padding, padding, padding);

    // Draw arrow line from text box to target
    painter.setPen(QPen(m_color, 2));
    painter.drawLine(m_position, m_arrowTarget);

    // Draw arrowhead at target
    // TODO: Compute proper arrowhead triangle based on line direction

    // Draw text background
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 180));
    painter.drawRoundedRect(textRect, 4, 4);

    // Draw text
    painter.setPen(m_color);
    painter.drawText(m_position, m_text);
}

} // namespace dawcast
