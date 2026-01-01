// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LowerThird.h"

#include <QPainter>
#include <QFont>
#include <QRect>

namespace dawcast {

LowerThird::LowerThird(QObject *parent)
    : QObject(parent)
{
}

LowerThird::~LowerThird() = default;

void LowerThird::setLine1(const QString &text)
{
    m_line1 = text;
}

void LowerThird::setLine2(const QString &text)
{
    m_line2 = text;
}

void LowerThird::setStyle(const QColor &bg, const QColor &text, int fontSize)
{
    m_bgColor = bg;
    m_textColor = text;
    m_fontSize = fontSize;
}

void LowerThird::setDuration(double seconds)
{
    m_duration = seconds;
}

QString LowerThird::line1() const { return m_line1; }
QString LowerThird::line2() const { return m_line2; }
double LowerThird::duration() const { return m_duration; }

void LowerThird::render(QPainter &painter, int frameWidth, int frameHeight, double progress)
{
    // TODO: Animate slide-in/slide-out based on progress (0.0 to 1.0)
    // progress < 0.1: slide in from left
    // progress > 0.9: slide out to left
    // middle: fully visible

    const int barHeight = m_fontSize * 3;
    const int y = frameHeight - barHeight - 40;
    const int padding = 20;

    // Background bar
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_bgColor);
    painter.drawRect(0, y, frameWidth, barHeight);

    // Line 1 (primary text)
    QFont font;
    font.setPixelSize(m_fontSize);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(m_textColor);
    painter.drawText(padding, y + m_fontSize + 4, m_line1);

    // Line 2 (secondary text)
    font.setPixelSize(m_fontSize - 4);
    font.setBold(false);
    painter.setFont(font);
    painter.drawText(padding, y + m_fontSize * 2 + 4, m_line2);

    Q_UNUSED(progress)
}

} // namespace dawcast
