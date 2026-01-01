// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "TickerCrawl.h"

#include <QPainter>
#include <QFont>
#include <QFontMetrics>

namespace dawcast {

TickerCrawl::TickerCrawl(QObject *parent)
    : QObject(parent)
{
}

TickerCrawl::~TickerCrawl() = default;

void TickerCrawl::setText(const QString &text)
{
    m_text = text;
}

void TickerCrawl::setSpeed(float pixelsPerSecond)
{
    m_speed = pixelsPerSecond;
}

void TickerCrawl::setStyle(const QColor &bg, const QColor &text, int fontSize)
{
    m_bgColor = bg;
    m_textColor = text;
    m_fontSize = fontSize;
}

QString TickerCrawl::text() const { return m_text; }
float TickerCrawl::speed() const { return m_speed; }

void TickerCrawl::render(QPainter &painter, int frameWidth, int frameHeight, double timeSeconds)
{
    if (m_text.isEmpty()) return;

    QFont font;
    font.setPixelSize(m_fontSize);
    painter.setFont(font);

    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(m_text);
    int barHeight = m_fontSize + 10;
    int y = frameHeight - barHeight;

    // Background strip
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_bgColor);
    painter.drawRect(0, y, frameWidth, barHeight);

    // Scrolling text position: starts off-screen right, scrolls left
    // Wraps around when fully off-screen left
    int totalTravel = frameWidth + textWidth;
    int offset = static_cast<int>(m_speed * timeSeconds) % totalTravel;
    int xPos = frameWidth - offset;

    painter.setPen(m_textColor);
    painter.drawText(xPos, y + m_fontSize + 2, m_text);
}

} // namespace dawcast
