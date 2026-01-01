// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "WipeLeft.h"

#include <QPainter>
#include <algorithm>
#include <cmath>

namespace dawcast {

WipeLeft::WipeLeft() = default;
WipeLeft::~WipeLeft() = default;

QImage WipeLeft::process(QImage& frameA, QImage& frameB, float progress)
{
    const int w = frameA.width();
    const int h = frameA.height();

    QImage result = frameA.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // Calculate wipe boundary position
    // For a soft edge, we blend pixels in the transition zone
    const float softPixels = m_edgeSoftness * static_cast<float>(
        (m_direction == Direction::Left || m_direction == Direction::Right) ? w : h);

    for (int y = 0; y < h; ++y) {
        auto* scanA = reinterpret_cast<const QRgb*>(frameA.constScanLine(y));
        auto* scanB = reinterpret_cast<const QRgb*>(frameB.constScanLine(y));
        auto* scanOut = reinterpret_cast<QRgb*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            float pos = 0.0f; // normalised position along wipe axis

            switch (m_direction) {
            case Direction::Left:  pos = static_cast<float>(x) / static_cast<float>(w); break;
            case Direction::Right: pos = 1.0f - static_cast<float>(x) / static_cast<float>(w); break;
            case Direction::Up:    pos = static_cast<float>(y) / static_cast<float>(h); break;
            case Direction::Down:  pos = 1.0f - static_cast<float>(y) / static_cast<float>(h); break;
            }

            // Blend factor: 0 = frame A, 1 = frame B
            float blend = 0.0f;
            if (softPixels > 0.5f) {
                float edge = progress;
                float halfSoft = m_edgeSoftness * 0.5f;
                blend = std::clamp((pos - edge + halfSoft) / m_edgeSoftness, 0.0f, 1.0f);
                blend = 1.0f - blend;
            } else {
                blend = (pos < progress) ? 1.0f : 0.0f;
            }

            // Lerp ARGB
            int rA = qRed(scanA[x]),   gA = qGreen(scanA[x]),   bA = qBlue(scanA[x]);
            int rB = qRed(scanB[x]),   gB = qGreen(scanB[x]),   bB = qBlue(scanB[x]);
            int r = static_cast<int>(rA + (rB - rA) * blend);
            int g = static_cast<int>(gA + (gB - gA) * blend);
            int b = static_cast<int>(bA + (bB - bA) * blend);
            scanOut[x] = qRgb(r, g, b);
        }
    }

    return result;
}

QString WipeLeft::name() const
{
    return QStringLiteral("Directional Wipe");
}

int WipeLeft::parameterCount() const
{
    return 2; // direction, edge_softness
}

} // namespace dawcast
