// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "PushSlide.h"

#include <QPainter>
#include <cmath>

namespace dawcast {

PushSlide::PushSlide() = default;
PushSlide::~PushSlide() = default;

QImage PushSlide::process(QImage& frameA, QImage& frameB, float progress)
{
    const int w = frameA.width();
    const int h = frameA.height();

    float t = applyEasing(progress);

    QImage result(frameA.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::black);
    QPainter painter(&result);

    int offsetX = 0, offsetY = 0;

    switch (m_direction) {
    case Direction::Left:
        offsetX = -static_cast<int>(t * w);
        painter.drawImage(offsetX, 0, frameA);
        painter.drawImage(offsetX + w, 0, frameB);
        break;
    case Direction::Right:
        offsetX = static_cast<int>(t * w);
        painter.drawImage(offsetX, 0, frameA);
        painter.drawImage(offsetX - w, 0, frameB);
        break;
    case Direction::Up:
        offsetY = -static_cast<int>(t * h);
        painter.drawImage(0, offsetY, frameA);
        painter.drawImage(0, offsetY + h, frameB);
        break;
    case Direction::Down:
        offsetY = static_cast<int>(t * h);
        painter.drawImage(0, offsetY, frameA);
        painter.drawImage(0, offsetY - h, frameB);
        break;
    }

    painter.end();
    return result;
}

QString PushSlide::name() const
{
    return QStringLiteral("Push Slide");
}

int PushSlide::parameterCount() const
{
    return 2; // direction, easing
}

float PushSlide::applyEasing(float t) const
{
    switch (m_easing) {
    case Easing::Linear:
        return t;
    case Easing::EaseIn:
        return t * t;
    case Easing::EaseOut:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case Easing::EaseInOut:
        return t < 0.5f
            ? 2.0f * t * t
            : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
    }
    return t;
}

} // namespace dawcast
