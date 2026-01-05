// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "LowerThird.h"

#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QRect>
#include <cmath>

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

/// Smooth ease-in-out curve for animation (cubic Bezier approximation)
static double easeInOutCubic(double t)
{
    if (t < 0.5) {
        return 4.0 * t * t * t;
    }
    double f = (2.0 * t) - 2.0;
    return 0.5 * f * f * f + 1.0;
}

void LowerThird::render(QPainter &painter, int frameWidth, int frameHeight, double progress)
{
    // Clamp progress to [0, 1]
    progress = qBound(0.0, progress, 1.0);

    // Calculate slide position based on animation phase
    // 0.0 - 0.1: slide in from left  (0% -> 100% visible)
    // 0.1 - 0.9: fully visible
    // 0.9 - 1.0: slide out to left   (100% -> 0% visible)
    double slideAmount = 1.0; // 1.0 = fully visible, 0.0 = fully off-screen left

    if (progress < 0.1) {
        // Slide in: map [0, 0.1] to [0, 1] with easing
        slideAmount = easeInOutCubic(progress / 0.1);
    } else if (progress > 0.9) {
        // Slide out: map [0.9, 1.0] to [1, 0] with easing
        slideAmount = easeInOutCubic(1.0 - (progress - 0.9) / 0.1);
    }

    // Bar dimensions
    const int barHeight = m_fontSize * 3 + 16;
    const int barY = frameHeight - barHeight - 40;
    const int cornerRadius = 6;
    const int padding = 24;
    const int accentWidth = 4;

    // Horizontal offset for slide animation (off-screen left when slideAmount = 0)
    const int xOffset = static_cast<int>((slideAmount - 1.0) * (frameWidth + 20));

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Translate for slide animation
    painter.translate(xOffset, 0);

    // Draw semi-transparent background bar with rounded corners
    QPainterPath bgPath;
    QRectF barRect(0, barY, frameWidth, barHeight);
    bgPath.addRoundedRect(barRect, cornerRadius, cornerRadius);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_bgColor);
    painter.drawPath(bgPath);

    // Draw accent stripe on left edge
    QColor accentColor = m_textColor;
    accentColor.setAlpha(220);
    painter.setBrush(accentColor);
    painter.drawRect(0, barY, accentWidth, barHeight);

    // Line 1 (primary text): large bold
    QFont font1;
    font1.setPixelSize(m_fontSize);
    font1.setBold(true);
    font1.setFamily(QStringLiteral("sans-serif"));
    painter.setFont(font1);
    painter.setPen(m_textColor);

    const int textX = padding + accentWidth;
    const int line1Y = barY + m_fontSize + 8;
    painter.drawText(textX, line1Y, m_line1);

    // Line 2 (secondary text): smaller, lighter
    const int line2FontSize = qMax(m_fontSize - 6, 10);
    QFont font2;
    font2.setPixelSize(line2FontSize);
    font2.setBold(false);
    font2.setFamily(QStringLiteral("sans-serif"));
    painter.setFont(font2);

    QColor subtitleColor = m_textColor;
    subtitleColor.setAlpha(200);
    painter.setPen(subtitleColor);

    const int line2Y = line1Y + line2FontSize + 6;
    painter.drawText(textX, line2Y, m_line2);

    painter.restore();
}

} // namespace dawcast
