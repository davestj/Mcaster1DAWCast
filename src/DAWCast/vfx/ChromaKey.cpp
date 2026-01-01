// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ChromaKey.h"

#include <cmath>
#include <algorithm>

namespace dawcast {

ChromaKey::ChromaKey() = default;
ChromaKey::~ChromaKey() = default;

QImage ChromaKey::process(QImage& frameA, QImage& frameB, float progress)
{
    Q_UNUSED(progress);

    const int w = frameA.width();
    const int h = frameA.height();

    QImage srcA = frameA.convertToFormat(QImage::Format_ARGB32);
    QImage srcB = frameB.convertToFormat(QImage::Format_ARGB32);
    QImage result(w, h, QImage::Format_ARGB32);

    // Key colour hue/sat
    float keyH = 0.0f, keyS = 0.0f, keyV = 0.0f;
    m_keyColor.getHsvF(&keyH, &keyS, &keyV);
    keyH *= 360.0f; // convert to degrees

    for (int y = 0; y < h; ++y) {
        const auto* scanA = reinterpret_cast<const QRgb*>(srcA.constScanLine(y));
        const auto* scanB = reinterpret_cast<const QRgb*>(srcB.constScanLine(y));
        auto* scanOut = reinterpret_cast<QRgb*>(result.scanLine(y));

        for (int x = 0; x < w; ++x) {
            QColor pixel(scanA[x]);
            float pH = 0.0f, pS = 0.0f, pV = 0.0f;
            pixel.getHsvF(&pH, &pS, &pV);
            pH *= 360.0f;

            // Circular hue distance
            float hueDist = std::abs(pH - keyH);
            if (hueDist > 180.0f) hueDist = 360.0f - hueDist;

            // Key alpha: 0 = fully keyed (transparent), 1 = fully opaque
            float alpha = 1.0f;
            if (pS >= m_satThreshold && hueDist <= m_hueRange) {
                // Soft edge based on hue distance
                float softEdge = m_hueRange * 0.2f; // 20% of range is soft
                if (hueDist > m_hueRange - softEdge) {
                    alpha = (hueDist - (m_hueRange - softEdge)) / softEdge;
                } else {
                    alpha = 0.0f;
                }
            }

            // Spill suppression: desaturate keyed colour from foreground
            int rA = qRed(scanA[x]);
            int gA = qGreen(scanA[x]);
            int bA = qBlue(scanA[x]);

            if (m_spillSuppression > 0.0f && alpha > 0.0f && alpha < 1.0f) {
                // Simple spill suppression: limit the key channel
                if (m_keyColor.green() > m_keyColor.red() && m_keyColor.green() > m_keyColor.blue()) {
                    // Green screen — limit green to max of red/blue
                    int maxRB = std::max(rA, bA);
                    gA = static_cast<int>(gA + m_spillSuppression * (maxRB - gA));
                } else if (m_keyColor.blue() > m_keyColor.red() && m_keyColor.blue() > m_keyColor.green()) {
                    // Blue screen — limit blue to max of red/green
                    int maxRG = std::max(rA, gA);
                    bA = static_cast<int>(bA + m_spillSuppression * (maxRG - bA));
                }
            }

            // Composite foreground over background
            int rB = qRed(scanB[x]);
            int gB = qGreen(scanB[x]);
            int bB = qBlue(scanB[x]);
            int r = static_cast<int>(rA * alpha + rB * (1.0f - alpha));
            int g = static_cast<int>(gA * alpha + gB * (1.0f - alpha));
            int b = static_cast<int>(bA * alpha + bB * (1.0f - alpha));
            scanOut[x] = qRgba(std::clamp(r, 0, 255),
                                std::clamp(g, 0, 255),
                                std::clamp(b, 0, 255), 255);
        }
    }

    return result;
}

QString ChromaKey::name() const
{
    return QStringLiteral("Chroma Key");
}

int ChromaKey::parameterCount() const
{
    return ParamCount;
}

} // namespace dawcast
